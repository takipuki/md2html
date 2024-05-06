#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef linux
#include <unistd.h>
#define deb(x) printf(#x " = %d\n", x);
#define debc(x) printf(#x " = '%c'\n", x);

#define HELP_TEXT "\
                                                          \n\
md2html is a tool to convert a subset of markdown to html.\n\
                                                          \n\
Usage: md2html [options] [target]                         \n\
Options:                                                  \n\
  -h          show this help                              \n\
  -o FILE     html output file; stdout if not specified   \n\
  -c FILE     css file to link                            \n\
  -m          enable math rendering                       \n\
                                                          \n\
"
#endif
#ifdef _WIN32
#define HELP_TEXT "\
                                                          \n\
md2html is a tool to convert a subset of markdown to html.\n\
                                                          \n\
Usage: md2html [target] [output] [css] [math]             \n\
                                                          \n\
e.g.                                                      \n\
  md2html in.md                       # output in stdout  \n\
  md2html in.md out.html                                  \n\
  md2html in.md out.html clean.css    # link css          \n\
  md2html in.md out.html clean.css m  # enable math       \n\
                                                          \n\
"
#endif

typedef struct {
    int cursor;
    int len;
    char *text;
} Parser;
Parser p;

#define INLINE_TAGS "*_`^~"
#define UL_SYMS "*-+"

#define print_range(a, b) for (char *i = (a); i < (b); i++) {putchar(*i);}
#define skipline() for (; p.text[p.cursor] && p.text[p.cursor] != '\n'; p.cursor++);
#define trim() for (; p.text[p.cursor] == ' ' || p.text[p.cursor] == '\n'; p.cursor++);
void parse_par();
void parse_tag();
void parse_tag_a();
void parse_tag_img();
void parse_code_block();
void parse_math_inline();
void parse_math_block();
void parse_math_content();
void parse_sen();
void parse_ul(int);
int is_start_ol();
void parse_ol(int, int);
void parse();

int main(int argc, char **argv) {
    // options
#ifdef linux
    char *css = NULL;
    bool mathjax = false;
    for (char opt; (opt = getopt(argc, argv, "o:c:mh")) != -1; ) {
        switch (opt) {
        case 'o':
            freopen(optarg, "w", stdout);
            break;
        case 'h':
            printf(HELP_TEXT);
            return 0;
        case 'c':
            css = optarg;
            break;
        case 'm':
            mathjax = true;
            break;
        }
    }
    if (optind == argc) {
        puts("Target not specified. -h for help.");
        return 0;
    }
#endif

#ifdef _WIN32
    if (argc < 2) {
        printf(HELP_TEXT);
        return 0;
    }
    switch (argc) {
    case 5:
        math = true;
    case 4:
        css = argv[3];
    case 3:
        freopen(argv[2], "w", stdout);
    case 2:
        input = argv[1];
    }
#endif

    FILE *fp = fopen(argv[optind], "r");
    fseek(fp, 0, SEEK_END);
    p.len = ftell(fp);
    p.text = malloc(p.len);
    fseek(fp, 0, SEEK_SET);
    fread(p.text, sizeof(char), p.len, fp);
    fclose(fp);
    p.text[p.len] = '\0';
    //puts(p.text);

    puts("<!DOCTYPE html>");
    puts("<head>");
    if (mathjax) {
        puts("<script src=\"https://polyfill.io/v3/polyfill.min.js?features=es6\"></script>");
        puts("<script id=\"MathJax-script\" async src=\"https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js\"></script>");
    }
    if (css)
        printf("<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\"", css);
    puts("</head>");
    puts("<body>");
    parse();
    puts("</body>");
    puts("</html>");
    free(p.text);
}

void parse() {
    int ol_start;
    for (char ch = p.text[p.cursor]; p.cursor < p.len; ch = p.text[p.cursor]) {
        if (ch == '\n') {
            putchar('\n');
            p.cursor++;
        }

        // heading
        else if (ch == '#') {
            int level = 1;
            char c;
            for (c = p.text[++p.cursor]; p.cursor < p.len; c = p.text[++p.cursor]) {
                if (c == '#') level++;
                else break;
            }
            if (level > 6) level = 6;

            printf("\n<h%d>", level);
            for (c = p.text[p.cursor]; p.cursor < p.len; c = p.text[++p.cursor]) {
                if (c == '\n') break;
                putchar(c);
            }

            printf("</h%d>\n", level);
            p.cursor++;
        }

        // code block
        else if (p.cursor < p.len-2 &&
                 ch == '`' &&
                 p.text[p.cursor+1] == '`' &&
                 p.text[p.cursor+2] == '`')
        {
            parse_code_block();
        }

        // math block
        else if (ch == '$' && p.text[p.cursor+1] == '$') {
            parse_math_block();
        }

        // ul
        else if ((strchr(UL_SYMS, ch)) &&
                 p.text[p.cursor+1] == ' ')
        {
            parse_ul(0);
        }

        // ol
        else if ((ol_start = is_start_ol()) > 0) {
            parse_ol(0, ol_start);
        }

        else {
            parse_par();
        }
        trim();
    }
}

void parse_ul(int indent) {
    // - first
    // ^
    printf("<ul>\n<li><p>");
    bool para = false;
    p.cursor += 2;
    for (; p.text[p.cursor];) {
        if (p.text[p.cursor] == '\n') {
            if (p.text[p.cursor+1] == '\n') {
                para = true;
                p.cursor += 1;
                continue;
            }

            p.cursor += 1;
            int flag = true;
            for (int i = 0; i < indent; i++)
                if (p.text[p.cursor+i] != ' ')
                    flag = false;
            if (!flag) {
                p.cursor -= 1;
                printf("</p></li>\n</ul>\n");
                return;
            }
            p.cursor += indent;

            int i = 0;
            for (; p.text[p.cursor+i] == ' '; i++);

            if (strchr(UL_SYMS, p.text[p.cursor+i]) && p.text[p.cursor+i+1] == ' ') {
                if (i > 0) {
                    printf("</p>\n");
                    p.cursor += i;
                    parse_ul(indent+i);
                } else {
                    printf("</p></li>\n<li><p>");
                    p.cursor += i + 2;
                }
            } else {
                if (i > 0) {
                    if (para) {
                        printf("</p>\n<p>");
                    }
                    putchar(' ');
                    p.cursor += i;
                } else {
                    printf("</p></li>\n</ul>\n");
                    return;
                }
            }
        }

        else if (p.text[p.cursor]== '\\') {
            if (p.text[p.cursor+1] == '\n' || p.text[p.cursor+1] == '\0')
                puts("<br>");
            else
                putchar(p.text[p.cursor]);
            p.cursor += 2;
        }

        else if (p.text[p.cursor]== '[')
            parse_tag_a();

        else if (p.text[p.cursor]== '!') {
            if (p.text[p.cursor+1] == '[')
                parse_tag_img();
            else {
                putchar(p.text[p.cursor]);
                p.cursor++;
            }
        }

        else if (p.text[p.cursor]== '$')
            parse_math_inline();

        else if (strchr(INLINE_TAGS, p.text[p.cursor]) != NULL)
            parse_tag();

        else {
            putchar(p.text[p.cursor]);
            p.cursor += 1;
        }
    }
}

int is_start_ol() {
    if (!isdigit(p.text[p.cursor])) return 0;
    int i = p.cursor;
    for (p.cursor++; isdigit(p.text[p.cursor]); p.cursor++);
    if (p.text[p.cursor] != '.' || p.text[p.cursor+1] != ' ') {
        p.cursor = i;
        return 0;
    }
    p.text[p.cursor] = 0;
    p.cursor += 2;
    return atoi(p.text+i);
}

void parse_ol(int indent, int start) {
    // 1. first
    //    ^
    printf("<ol start='%d'>\n<li><p>", start);
    for (; p.text[p.cursor];) {
        if (p.text[p.cursor] == '\n') {
            if (p.text[p.cursor+1] == '\n') {
                p.cursor += 1;
                continue;
            }

            p.cursor += 1;
            bool flag = true;
            for (int i = 0; i < indent; i++)
                if (p.text[p.cursor+i] != ' ')
                    flag = false;
            if (!flag) {
                p.cursor -= 1;
                printf("</p></li>\n</ol>\n");
                return;
            }
            p.cursor += indent;

            int i = 0;
            for (; p.text[p.cursor+i] == ' '; i++);

            int j = p.cursor;
            p.cursor += i;
            start = is_start_ol();
            if (start > 0) {
                if (i > 0) {
                    puts("</p>");
                    parse_ol(indent+i, start);
                } else {
                    printf("</p></li>\n<li><p>");
                }
            } else {
                if (i > 0) {
                    printf("</p>\n<p>");
                    putchar(' ');
                } else {
                    p.cursor = j;
                    printf("</p></li>\n</ol>\n");
                    return;
                }
            }
        }

        else if (p.text[p.cursor]== '\\') {
            if (p.text[p.cursor+1] == '\n' || p.text[p.cursor+1] == '\0')
                puts("<br>");
            else
                putchar(p.text[p.cursor]);
            p.cursor += 2;
        }

        else if (p.text[p.cursor]== '[')
            parse_tag_a();

        else if (p.text[p.cursor]== '!') {
            if (p.text[p.cursor+1] == '[')
                parse_tag_img();
            else {
                putchar(p.text[p.cursor]);
                p.cursor++;
            }
        }

        else if (p.text[p.cursor]== '$')
            parse_math_inline();

        else if (strchr(INLINE_TAGS, p.text[p.cursor]) != NULL)
            parse_tag();

        else {
            putchar(p.text[p.cursor]);
            p.cursor += 1;
        }
    }
}

void parse_sen() {
    trim();
    for (char ch = p.text[p.cursor]; p.cursor < p.len; ch = p.text[p.cursor]) {
        if (ch == '\n') {
            if (p.cursor >= p.len) return;
            char ch_next = p.text[++p.cursor];

            if (ch_next == '\n') {
                while (ch_next == '\n') ch_next = p.text[++p.cursor];
                return;
            }

            else putchar(ch);
        }

        else if (ch == '\\') {
            if (p.text[p.cursor+1] == '\n' || p.text[p.cursor+1] == '\0')
                puts("<br>");
            else
                putchar(p.text[p.cursor]);
            p.cursor += 2;
        }

        else if (ch == '[')
            parse_tag_a();

        else if (ch == '!') {
            if (p.text[p.cursor+1] == '[')
                parse_tag_img();
            else {
                putchar(ch);
                p.cursor++;
            }
        }

        else if (ch == '$')
            parse_math_inline();

        else if (strchr(INLINE_TAGS, ch) != NULL)
            parse_tag();

        else {
            putchar(ch);
            p.cursor++;
        }
    }
}

void parse_par() {
    printf("\n<p>\n");
    parse_sen();
    printf("\n</p>\n");
}

void parse_tag() {
    char tag_ch = p.text[p.cursor];
    char *tag_html;

    switch (tag_ch) {
    case '*':
        tag_html = "b";
        break;
    case '_':
        tag_html = "i";
        break;
    case '`':
        tag_html = "code";
        break;
    case '^':
        tag_html = "sup";
        break;
    case '~':
        tag_html = "sub";
        break;
    }

    printf("<%s>", tag_html);
    p.cursor++;
    for (char ch = p.text[p.cursor]; p.cursor < p.len; ch = p.text[p.cursor]) {
        if (ch == tag_ch) {
            printf("</%s>", tag_html);
            p.cursor++;
            return;
        }

        if (strchr(INLINE_TAGS, ch) != NULL)
            parse_tag();
        else {
            putchar(ch);
            p.cursor++;
        }
    }
}

void parse_tag_img() {
    // at '!'
    char *j;
    if ((j = strchr(p.text+p.cursor, ']')) == NULL) {
        print_range(p.text+p.cursor, p.text+p.len);
        return;
    }

    if (*(j+1) != '(') {
        print_range(p.text+p.cursor, j+1);
        return;
    }

    char *k;
    if ((k = strchr(j+1, ')')) == NULL) {
        print_range(p.text+p.cursor, p.text+p.len);
        return;
    }

    printf("<img alt='");
    print_range(p.text+p.cursor+2, j);
    printf("' src='");
    print_range(j+2, k);
    printf("'>");

    // ![hi](hi.jpg)
    // ^           ^
    p.cursor += k - (p.text+p.cursor) + 1;
}

void parse_tag_a() {
    // at '['
    char *j;
    if ((j = strchr(p.text+p.cursor, ']')) == NULL) {
        print_range(p.text+p.cursor, p.text+p.len);
        return;
    }

    if (*(j+1) != '(') {
        print_range(p.text+p.cursor, j+1);
        return;
    }

    char *k;
    if ((k = strchr(j+1, ')')) == NULL) {
        print_range(p.text+p.cursor, p.text+p.len);
        return;
    }

    printf("<a href='");
    print_range(j+2, k);
    printf("'>");
    print_range(p.text+p.cursor+1, j);
    printf("</a>");

    // [googel](google.com)
    // ^           ^
    p.cursor += k - (p.text+p.cursor) + 1;
}

void parse_math_inline() {
    printf("\\(");
    p.cursor++;
    parse_math_content();
    printf("\\)");
}

void parse_math_block() {
    printf("\\[");
    p.cursor += 2; // $$
    parse_math_content();
    printf("\\]");
    p.cursor++; // skipped one $ in parse_math_content
}

void parse_math_content() {
    for (; p.text[p.cursor] != '$'; p.cursor++) {
        if (p.cursor >= p.len)
            return;
        if (p.text[p.cursor] == '\\') {
            if (p.text[p.cursor+1] == '$') {
                putchar('$');
                p.cursor++;
            }
            else
                putchar('\\');
        }
        else
            putchar(p.text[p.cursor]);
    }
    p.cursor++;
}

void parse_code_block() {
    // at ```
    skipline();
    p.cursor++;
    if (p.cursor >= p.len) return;
    puts("<pre>");
    for (; p.text[p.cursor]; p.cursor++) {
        if (p.cursor < p.len-2 && p.text[p.cursor] == '`') {
            if (p.text[p.cursor+1] == '`' && p.text[p.cursor+2] == '`')
                break;
        }
        if (p.text[p.cursor] == '<')
            printf("&lt;");
        else if (p.text[p.cursor] == '>')
            printf("&gt;");
        else
            putchar(p.text[p.cursor]);
    }
    puts("</pre>");
    if (p.text[p.cursor]) skipline();
    p.cursor += 1;
}
