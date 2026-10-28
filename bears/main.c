#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define TEMPLATE_FILENAME "template.html"

#define TITLE_BLUEP "[TITLE]"
#define POEM_BLUEP  "[POEM]"
#define DATE_BLUEP  "[DATE]"

typedef struct
{
    char*  title;
    char*  date;
    char** verses;
    size_t verse_count;
} PoemData;

static char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char* buf = malloc(size + 1);
    size_t got = fread(buf, 1, size, f);
    buf[got] = 0;

    fclose(f);
    return buf;
}

static char* non_evil_strdup(const char* s) {
    size_t len = strlen(s);
    char* out = malloc(len + 1);
    memcpy(out, s, len + 1);
    return out;
}

static char* replace_all(char* src, const char* pattern, const char* text) {
    size_t pat_len  = strlen(pattern);
    size_t text_len = strlen(text);

    size_t count = 0;
    for (const char* p = strstr(src, pattern); p; p = strstr(p + pat_len, pattern)) count++;
    if (count == 0) return src;

    size_t new_len = strlen(src) - count * pat_len + count * text_len + 1;
    char* out = malloc(new_len);
    char* w = out;

    const char* cur = src;
    const char* pos;
    while ((pos = strstr(cur, pattern)) != NULL)
    {
        size_t before = pos - cur;
        memcpy(w, cur, before);
        w += before;
        memcpy(w, text, text_len);
        w += text_len;
        cur = pos + pat_len;
    }
    memcpy(w, cur, strlen(cur) + 1);

    free(src);
    return out;
}

static char* escape_html(const char* s) {
    size_t len = 0;
    for (const char* p = s; *p; p++)
    {
        if (*p == '&') len += 5;
        else if (*p == '<' || *p == '>') len += 4;
        else len++;
    }

    char* out = malloc(len + 1);
    char* w = out;
    for (const char* p = s; *p; p++)
    {
        if (*p == '&') { memcpy(w, "&amp;", 5); w += 5; }
        else if (*p == '<') { memcpy(w, "&lt;", 4); w += 4; }
        else if (*p == '>') { memcpy(w, "&gt;", 4); w += 4; }
        else *w++ = *p;
    }
    *w = 0;
    return out;
}

static void append_str(char** buf, size_t* len, const char* text) {
    size_t add = strlen(text);
    *buf = realloc(*buf, *len + add + 1);
    memcpy(*buf + *len, text, add + 1);
    *len += add;
}

static void free_PoemData(PoemData* p) {
    free(p->title);
    free(p->date);
    for (size_t i = 0; i < p->verse_count; i++) free(p->verses[i]);
    free(p->verses);
}

// the poem txt file structure should be:
/*    title \n
      empty line \n
      verses (stanzas split by empty line) \n
      empty line \n
      date
*/
static int parse_poem_file(const char* filename, PoemData* poem) {
    char* data = read_file(filename);
    if (!data) return 0;

    // normalize windows line endings goddamnnnnnnnnnnnnnnnnnnnn
    char* w = data;
    for (char* r = data; *r; r++)
    {
        if (*r != '\r') *w++ = *r;
    }
    *w = 0;

    // split into lines in place
    size_t line_count = 1;
    for (char* p = data; *p; p++) if (*p == '\n') line_count++;

    char** lines = malloc(line_count * sizeof(char*));
    size_t n = 0;
    lines[n++] = data;
    for (char* p = data; *p; p++)
    {
        if (*p == '\n')
        {
            *p = 0;
            lines[n++] = p + 1;
        }
    }

    // date is last non empty line in txt file
    size_t date_idx = n;
    for (size_t j = n; j > 1; j--)
    {
        if (*lines[j - 1])
        {
            date_idx = j - 1;
            break;
        }
    }

    poem->title = non_evil_strdup(lines[0]);
    poem->date  = non_evil_strdup(date_idx < n ? lines[date_idx] : "");

    // everything in between is verses, split by blank lines and each verse keeps its lines joined with newline char
    poem->verses = malloc(n * sizeof(char*));
    poem->verse_count = 0;

    size_t i = 1;
    while (i < date_idx)
    {
        if (!*lines[i])
        {
            i++;
            continue;
        }

        char* verse = non_evil_strdup("");
        size_t len = 0;
        while (i < date_idx && *lines[i])
        {
            if (len > 0) append_str(&verse, &len, "\n");
            append_str(&verse, &len, lines[i]);
            i++;
        }
        poem->verses[poem->verse_count++] = verse;
    }

    free(lines);
    free(data);
    return 1;
}

static char* render_poem(PoemData* poem) {
    char* html = non_evil_strdup("");
    size_t len = 0;

    for (size_t i = 0; i < poem->verse_count; i++)
    {
        char* verse = escape_html(poem->verses[i]);
        verse = replace_all(verse, "\n", "<br>\n");

        append_str(&html, &len, "<p>");
        append_str(&html, &len, verse);
        append_str(&html, &len, "</p>\n");

        free(verse);
    }

    return html;
}

static char* make_output_filename(const char* title) {
    size_t len = strlen(title);
    char* out = malloc(len + 6);

    for (size_t i = 0; i < len; i++)
    {
        char c = title[i];
        if (c == ' ' || strchr("/\\:*?\"<>|", c)) c = '_';
        else c = tolower((unsigned char)c);
        out[i] = c;
    }
    memcpy(out + len, ".html", 6);
    return out;
}

int main(int argc, char** argv) {
    if (argc != 2)
    {
        printf("usage: %s <txt file>", argv[0]);
        return 1;
    }

    PoemData poem = {0};
    if (!parse_poem_file(argv[1], &poem)) return 1;

    char* template = read_file(TEMPLATE_FILENAME);
    if (!template)
    {
        free_PoemData(&poem);
        return 1;
    }

    char* title = escape_html(poem.title);
    char* date  = escape_html(poem.date);
    char* body  = render_poem(&poem);

    template = replace_all(template, TITLE_BLUEP, title);
    template = replace_all(template, DATE_BLUEP, date);
    // poem goes last so verse text is never scanned for blueprints #ACCOUNTING FOR EXPERIMENTELLEPOESIE
    template = replace_all(template, POEM_BLUEP, body);

    char* out_name = make_output_filename(poem.title);
    FILE* final_webpage = fopen(out_name, "w");
    if (!final_webpage) return 1;

    fprintf(final_webpage, "%s", template);
    fclose(final_webpage);

    free(out_name);
    free(title);
    free(date);
    free(body);
    free(template);
    free_PoemData(&poem);

    return 0;
}
