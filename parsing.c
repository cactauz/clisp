#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char *readline(char *prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

typedef struct lval
{
    int type;
    double dbl;
    long lng;
    char *err;
    char *sym;
    int count;
    struct lval **cell;
} lval;

enum
{
    LVAL_DOUBLE,
    LVAL_LONG,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_ERR
};

enum
{
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

lval *lval_long(long x)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_LONG;
    v->lng = x;
    return v;
}

lval *lval_double(double x)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_DOUBLE;
    v->dbl = x;
    return v;
}

lval *lval_err(char *m)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

lval *lval_sym(char *s)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval *lval_sexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval *v)
{
    switch (v->type)
    {
    case LVAL_DOUBLE:
        break;
    case LVAL_LONG:
        break;
    case LVAL_ERR:
        free(v->err);
        break;
    case LVAL_SYM:
        free(v->sym);
        break;
    case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++)
        {
            lval_del(v->cell[i]);
        }
        free(v->cell);
        break;
    }

    free(v);
}

lval *lval_read_long(mpc_ast_t *t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_long(x) : lval_err("invalid long");
}

lval *lval_read_double(mpc_ast_t *t)
{
    errno = 0;
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ? lval_double(x) : lval_err("invalid double");
}

lval *lval_add(lval *v, lval *x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval *) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval *lval_read(mpc_ast_t *t)
{
    if (strstr(t->tag, "double"))
    {
        return lval_read_double(t);
    }
    if (strstr(t->tag, "long"))
    {
        return lval_read_long(t);
    }
    if (strstr(t->tag, "symbol"))
    {
        return lval_sym(t->contents);
    }

    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0)
    {
        x = lval_sexpr();
    }
    if (strstr(t->tag, "sexpr"))
    {
        x = lval_sexpr();
    }

    for (int i = 0; i < t->children_num; i++)
    {
        if (strcmp(t->children[i]->contents, "(") == 0)
        {
            continue;
        }
        if (strcmp(t->children[i]->contents, ")") == 0)
        {
            continue;
        }
        if (strcmp(t->children[i]->tag, "regex") == 0)
        {
            continue;
        }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_print(lval *v);

void lval_expr_print(lval *v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++)
    {
        lval_print(v->cell[i]);

        if (i != (v->count - 1))
        {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval *v)
{
    switch (v->type)
    {
    case LVAL_LONG:
        printf("%li", v->lng);
        break;
    case LVAL_DOUBLE:
        printf("%f", v->dbl);
        break;
    case LVAL_ERR:
        printf("error: %s", v->err);
        break;
    case LVAL_SYM:
        printf("%s", v->sym);
        break;
    case LVAL_SEXPR:
        lval_expr_print(v, '(', ')');
        break;
    }
}

void lval_println(lval *v)
{
    lval_print(v);
    putchar('\n');
}

lval *eval_longs(long x, char *op, long y)
{
    if (strcmp(op, "+") == 0)
    {
        return lval_long(x + y);
    }
    if (strcmp(op, "-") == 0)
    {
        return lval_long(x - y);
    }
    if (strcmp(op, "*") == 0)
    {
        return lval_long(x * y);
    }
    if (strcmp(op, "/") == 0)
    {
        if (y == 0)
        {
            return lval_err("divide by zero");
        }
        return lval_long(x / y);
    }
    if (strcmp(op, "%") == 0)
    {
        return lval_long(x % y);
    }
    if (strcmp(op, "^") == 0)
    {
        return lval_long(x ^ y);
    }
    if (strcmp(op, "min") == 0)
    {
        return lval_long(x > y ? y : x);
    }
    if (strcmp(op, "max") == 0)
    {
        return lval_long(x > y ? x : y);
    }

    return lval_err("bad operator bro");
}

lval *eval_doubles(double x, char *op, double y)
{
    if (strcmp(op, "+") == 0)
    {
        return lval_double(x + y);
    }
    if (strcmp(op, "-") == 0)
    {
        return lval_double(x - y);
    }
    if (strcmp(op, "*") == 0)
    {
        return lval_double(x * y);
    }
    if (strcmp(op, "/") == 0)
    {
        if (y == 0)
        {
            return lval_err("divide by zero");
        }
        return lval_double(x / y);
    }
    if (strcmp(op, "min") == 0)
    {
        return lval_double(x > y ? y : x);
    }
    if (strcmp(op, "max") == 0)
    {
        return lval_double(x > y ? x : y);
    }

    return lval_err("bad operator bro");
}

lval *lval_pop(lval *v, int i)
{
    lval *x = v->cell[i];

    memmove(&v->cell[i], &v->cell[i + 1],
            sizeof(lval *) * (v->count - i - 1));

    v->count--;

    v->cell = realloc(v->cell, sizeof(lval *) * v->count);
    return x;
}

lval *lval_take(lval *v, int i)
{
    lval *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval *builtin_op(lval *a, char *op)
{
    // ensure args are nums
    for (int i = 0; i < a->count; i++)
    {
        int type = a->cell[i]->type;
        if (type != LVAL_DOUBLE && type != LVAL_LONG)
        {
            lval_del(a);
            return lval_err("cannot operate on non numbers");
        }
    }

    // pop first thing
    lval *x = lval_pop(a, 0);

    // unary negation
    if ((strcmp(op, "-") == 0) && a->count == 0)
    {
        if (x->type == LVAL_DOUBLE)
        {
            x->dbl = -x->dbl;
        }
        else
        {
            x->lng = -x->lng;
        }
    }

    while (a->count > 0)
    {
        lval *y = lval_pop(a, 0);
        if (x->type == LVAL_DOUBLE || y->type == LVAL_DOUBLE)
        {
            double a = x->type == LVAL_LONG ? (double)x->lng : x->dbl;
            double b = y->type == LVAL_LONG ? (double)y->lng : y->dbl;
            x = eval_doubles(a, op, b);
        }
        else
        {
            x = eval_longs(x->lng, op, y->lng);
        }

        lval_del(y);
    }

    lval_del(a);
    return x;
}

lval *lval_eval(lval *v);

lval *lval_eval_sexpr(lval *v)
{
    // evaluate children
    for (int i = 0; i < v->count; i++)
    {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // check for errors
    for (int i = 0; i < v->count; i++)
    {
        if (v->cell[i]->type == LVAL_ERR)
            return lval_take(v, i);
    }

    // empty expression
    if (v->count == 0)
    {
        return v;
    }

    // single expression
    if (v->count == 1)
    {
        return lval_take(v, 0);
    }

    // ensure first is symbol
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_SYM)
    {
        lval_del(f);
        lval_del(v);
        return lval_err("sexpression does not start with symbol");
    }

    lval *result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval *lval_eval(lval *v)
{
    if (v->type == LVAL_SEXPR)
    {
        return lval_eval_sexpr(v);
    }
    return v;
}

int main(int argc, char **argv)
{
    mpc_parser_t *Double = mpc_new("double");
    mpc_parser_t *Long = mpc_new("long");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Clisp = mpc_new("clisp");

    mpca_lang(MPCA_LANG_DEFAULT, "                                     \
        double   : /-?[0-9]+\\.[0-9]+/ ; \
        long     : /-?[0-9]+/ ;                                        \
        symbol   : '+' | '-' | '*' | '/' | '%' | '^' | /min/ | /max/ ; \
        sexpr    : '(' <expr>* ')' ; \
        expr     : <double> | <long> | <symbol> | <sexpr> ;             \
        clisp    : /^/ <expr>+ /$/ ;                        \
    ",
              Double, Long, Symbol, Sexpr, Expr, Clisp);

    puts("clisp v 0.1");
    puts("press ctrl+c to exit\n");

    while (1)
    {
        char *input = readline("clisp> ");

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Clisp, &r))
        {
            lval *x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        }
        else
        {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
    }

    mpc_cleanup(6, Double, Long, Symbol, Sexpr, Expr, Clisp);
    return 0;
}
