/*
 * Copyright (c) 2004-2013 Sergey Lyubka <valenok@gmail.com>
 * Copyright (c) 2013 Cesanta Software Limited
 * Copyright (c) 2020 Julian Smythe <sausage@tehsausage.com>
 * All rights reserved
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "elsa/escape.c"
#include "elsa/fread.c"
#include "elsa/next.c"
#include "elsa/prettify.c"
#include "elsa/printer.c"
#include "elsa/printf.c"
#include "elsa/scanf.c"
#include "elsa/setf.c"
#include "elsa/walk.c"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *tok_type_names[] = {
    "INVALID", "STRING",       "NUMBER",     "TRUE",        "FALSE",
    "NULL",    "OBJECT_START", "OBJECT_END", "ARRAY_START", "ARRAY_END",
};

#define FAIL(str, line)                           \
  do {                                            \
    printf("Fail on line %d: [%s]\n", line, str); \
    return str;                                   \
  } while (0)

#define ASSERT(expr)                    \
  do {                                  \
    static_num_tests++;                 \
    if (!(expr)) FAIL(#expr, __LINE__); \
  } while (0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define RUN_TEST(test)        \
  do {                        \
    const char *msg = test(); \
    if (msg) return msg;      \
  } while (0)

static int static_num_tests = 0;

static const char *test_errors(void) {
  /* clang-format off */
  static const char *invalid_tests[] = {
      "p",        "a:3",           "\x01",         "{:",
      " { 1",     "{a:\"\n\"}",    "{a:1x}",       "{a:1e}",
      "{a:.1}",   "{a:0.}",        "{a:0.e}",      "{a:0.e1}",
      "{a:0.1e}", "{a:\"\\u\" } ", "{a:\"\\yx\"}", "{a:\"\\u111r\"}",
      NULL};
  static const char *incomplete_tests[] = {"",
                                           " \r\n\t",
                                           "{",
                                           " { a",
                                           "{a:",
                                           "{a:\"",
                                           " { a : \"xx",
                                           "{a:12",
                                           "{a:\"\\uf",
                                           "{a:\"\\uff",
                                           "{a:\"\\ufff",
                                           "{a:\"\\uffff",
                                           "{a:\"\\uffff\"",
                                           "{a:\"\\uffff\" ,",
                                           "{a:n",
                                           "{a:nu",
                                           "{a:nul",
                                           "{a:null",
                                           NULL};
  /* clang-format on */
  static const struct {
    const char *str;
    int expected_len;
  } success_tests[] = {{"{}", 2},
                       /* 2, 3, 4 byte utf-8 chars */
                       {"{a:\"\xd0\xb1\xe3\x81\xaf\xf0\xa2\xb3\x82\"}", 15},
                       {"{a:\"\\u0006\"}", 12},
                       {" { } ", 4},
                       {"{a:1}", 5},
                       {"{a:1.23}", 8},
                       {"{a:1e23}", 8},
                       {"{a:1.23e2}", 10},
                       {"{a:-123}", 8},
                       {"{a:-1.3}", 8},
                       {"{a:-1.3e-2}", 11},
                       {"{a:\"\"}", 6},
                       {"{a:\" \\n\\t\\r\"}", 13},
                       {" {a:[1]} 123456", 8},
                       {" {a:[]} 123456", 7},
                       {" {a:[1,2]} 123456", 10},
                       {"{a:1,b:2} xxxx", 9},
                       {"{a:1,b:{},c:[{}]} xxxx", 17},
                       {"{a:true,b:[false,null]} xxxx", 23},
                       {"[1.23, 3, 5]", 12},
                       {"[13, {\"a\":\"hi there\"}, 5]", 25},
                       {NULL, 0}};
  const char *s1 =
      " { a: 1, b: \"hi there\", c: true, d: false, "
      " e : null, f: [ 1, -2, 3], g: { \"1\": [], h: [ 7 ] } } ";
  int i;

  ASSERT(json_walk(NULL, 0, NULL, 0) == JSON_STRING_INVALID);
  for (i = 0; invalid_tests[i] != NULL; i++) {
    ASSERT(json_walk(invalid_tests[i], strlen(invalid_tests[i]), NULL, NULL) ==
           JSON_STRING_INVALID);
  }

  for (i = 0; incomplete_tests[i] != NULL; i++) {
    ASSERT(json_walk(incomplete_tests[i], strlen(incomplete_tests[i]), NULL,
                     NULL) == JSON_STRING_INCOMPLETE);
  }

  for (i = 0; success_tests[i].str != NULL; i++) {
    ASSERT(json_walk(success_tests[i].str, strlen(success_tests[i].str), NULL,
                     NULL) == success_tests[i].expected_len);
  }

  ASSERT(json_walk("{}", 2, NULL, NULL) == 2);
  ASSERT(json_walk(s1, strlen(s1), NULL, 0) > 0);

  return NULL;
}

struct my_struct {
  int a, b;
};

static int print_my_struct(struct json_out *out, va_list *ap) {
  struct my_struct *p = va_arg(*ap, struct my_struct *);
  return json_printf(out, "{a: %d, b: %d}", p->a, p->b);
}

static const char *test_json_printf(void) {
  char buf[200] = "";

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "222 57005 3735928559 16045690985373621933 42";
    json_printf(&out, "%hhu %hu %lu %llu %d",
                (uint8_t)0xde,
                (uint16_t)0xdead,
                (uint32_t)0xdeadbeefUL,
                (uint64_t)0xdeadbeeffee1deadULL, 42);
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "1 2 3 4 5 6 7 8 9";
    json_printf(&out, "%hhd %hd %d %ld %lld %jd %zd %td %d",
                1, 2, 3, 4L, 5LL, (intmax_t)6LL, (size_t)7ULL, (ptrdiff_t)8, 9);
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "1 2 3 4 5 6 7 8 9";
    json_printf(&out, "%hhu %hu %u %lu %llu %ju %zu %tu %u",
                1U, 2U, 3U, 4UL, 5ULL, (uintmax_t)6ULL, (size_t)7ULL,
                (ptrdiff_t)8, 9U);
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    void *ptr = (void *)0xaabbccdd;
    json_printf(&out, "%p", ptr);
    /* there is no standard representation for %p to compare to */
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "Hello";
    wchar_t wstr[6];
    size_t nchars = mbstowcs(wstr, "Hello", sizeof(wstr));
    /* if this isn't 5 then mbstowcs didn't work as expected */
    if (nchars == 5) {
      json_printf(&out, "%ls", wstr);
      ASSERT(strcmp(buf, result) == 0);
    }
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "1 % 2";
    json_printf(&out, "%d %% %d", 1, 2);
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    signed char hhn;
    short hn;
    int n;
    long ln;
    long long lln;
    intmax_t jn;
    size_t zn;
    ptrdiff_t tn;
    json_printf(&out, "%d%hhn%hn%n%ln%lln%jn%zn%tn", 123,
                &hhn, &hn, &n, &ln, &lln, &jn, &zn, &tn);
    ASSERT(hhn == 3 && hn == 3 && n == 3 && ln == 3 && lln == 3);
    ASSERT(jn == 3 && zn == 3 && tn == 3);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "1 2 3";
    json_printf(&out, "%" PRIdMAX " %" PRId64 " %d",
                (intmax_t)1, (int64_t)2, 3);
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "{\"foo\": 123, \"x\": [false, true], \"y\": \"hi\"}";
    json_printf(&out, "{%Q: %d, x: [%B, %B], y: %Q}", "foo", 123, 0, -1, "hi");
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    int arr[] = {-2387, 943478};
    json_printf(&out, "%M", json_printf_array, arr, sizeof(arr), sizeof(arr[0]),
                "%d");
    ASSERT(strcmp(buf, "[-2387, 943478]") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    double arr[] = {9.32156, 3.1415926};
    json_printf(&out, "%M", json_printf_array, arr, sizeof(arr), sizeof(arr[0]),
                "%.2lf");
    ASSERT(strcmp(buf, "[9.32, 3.14]") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    unsigned short arr[] = {65535, 777};
    const char *result = "{\"a\": [-1, 777], \"b\": 37}";
    json_printf(&out, "{a: %M, b: %d}", json_printf_array, arr, sizeof(arr),
                sizeof(arr[0]), "%hd", 37);
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "{\"a\": \"\\\"\\\\\\r\\nя\\t\\u0002\"}";
    json_printf(&out, "{a: %Q}", "\"\\\r\nя\t\x02");
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *arr[] = {"hi", "there", NULL};
    const char *result = "[\"hi\", \"there\", null]";
    json_printf(&out, "%M", json_printf_array, arr, sizeof(arr), sizeof(arr[0]),
                "%Q");
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    struct my_struct mys = {1, 2};
    const char *result = "{\"foo\": {\"a\": 1, \"b\": 2}, \"bar\": 3}";
    json_printf(&out, "{foo: %M, bar: %d}", print_my_struct, &mys, 3);
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    out.u.buf.size = 3;
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "{%d}", 123) == 5);
    ASSERT(memcmp(buf, "{1\x00\x00\x00", 5) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "\"foo\"";
    out.u.buf.size = 6;
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%.*Q", 3, "foobar") == 5);
    ASSERT(memcmp(buf, result, 5) == 0);
  }

  {
    /*
     * Check long string (which forces elsa to allocate a temporary buffer
     * from heap)
     */
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result =
        "{\"foo\": "
        "\"12345678901234567890123456789012345678901234567890123456789012345678"
        "90123456789012345678901234567890\"}";
    json_printf(&out, "{foo: %s}",
                "\"123456789012345678901234567890123456789012345678901234567890"
                "1234567890123456789012345678901234567890\"");
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *fmt = "{a: \"%s\"}", *result = "{\"a\": \"b\"}";
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, fmt, "b") > 0);
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%.*s %.*s %d", 2, "abc", 4, "abc", 5) > 0);
    ASSERT(strcmp(buf, "ab abc 5") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%5s", "abc") > 0);
    ASSERT(strcmp(buf, "  abc") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%-5s", "abc") > 0);
    ASSERT(strcmp(buf, "abc  ") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%*s", 5, "abc") > 0);
    ASSERT(strcmp(buf, "  abc") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%*.*s", 5, 2, "abc") > 0);
    ASSERT(strcmp(buf, "   ab") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%.1Lf", (long double)1.5) > 0);
    ASSERT(strcmp(buf, "1.5") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%*.*s", 0, 0, "") == 0);
    ASSERT(strcmp(buf, "") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "\"a_b0\": 1";
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "a_b0: %d", 1) > 0);
    ASSERT(strcmp(buf, result) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%V", "a2", 2) > 0);
    ASSERT(strcmp(buf, "\"YTI=\"") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%V", "\x00 \x01 \x02 abc", 9) > 0);
    ASSERT(strcmp(buf, "\"ACABIAIgYWJj\"") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%H", 9, "\x00 \x01 \x02 abc") > 0);
    ASSERT(strcmp(buf, "\"002001200220616263\"") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "%c", 0x53) > 0);
    ASSERT(strcmp(buf, "S") == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    wchar_t wstr[2];
    size_t nchars = mbstowcs(wstr, "S", sizeof(wstr));
    /* if this isn't 1 then mbstowcs didn't work as expected */
    if (nchars == 1) {
      ASSERT(json_printf(&out, "%lc", wstr[0]) > 0);
      ASSERT(strcmp(buf, "S") == 0);
    }
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *result = "<\"array\">0f";
    memset(buf, 0, sizeof(buf));
    ASSERT(json_printf(&out, "<array>%02x", 15) > 0);
    ASSERT(strcmp(buf, result) == 0);
  }

  return NULL;
}

static void cb(void *data, const char *name, size_t name_len, const char *path,
               const struct json_token *token) {
  char *buf = (char *) data;

  const char *snull = "<null>";

  sprintf(buf + strlen(buf), "name:'%.*s', path:'%s', type:%s, val:'%.*s'\n",
          (int) (name != NULL ? name_len : strlen(snull)),
          name != NULL ? name : snull, path, tok_type_names[token->type],
          (int) (token->ptr != NULL ? token->len : (int) strlen(snull)),
          token->ptr != NULL ? token->ptr : snull);
}

static const char *test_callback_api(void) {
  const char *s =
      "{\"c\":[\"foo\", \"bar\", {\"a\":9, \"b\": \"x\"}], "
      "\"mynull\": null, \"mytrue\": true, \"myfalse\": false}";

  const char *result =
      "name:'<null>', path:'', type:OBJECT_START, val:'<null>'\n"
      "name:'c', path:'.c', type:ARRAY_START, val:'<null>'\n"
      "name:'0', path:'.c[0]', type:STRING, val:'foo'\n"
      "name:'1', path:'.c[1]', type:STRING, val:'bar'\n"
      "name:'2', path:'.c[2]', type:OBJECT_START, val:'<null>'\n"
      "name:'a', path:'.c[2].a', type:NUMBER, val:'9'\n"
      "name:'b', path:'.c[2].b', type:STRING, val:'x'\n"
      "name:'<null>', path:'.c[2]', type:OBJECT_END, val:'{\"a\":9, \"b\": "
      "\"x\"}'\n"
      "name:'<null>', path:'.c', type:ARRAY_END, val:'[\"foo\", \"bar\", "
      "{\"a\":9, \"b\": \"x\"}]'\n"
      "name:'mynull', path:'.mynull', type:NULL, val:'null'\n"
      "name:'mytrue', path:'.mytrue', type:TRUE, val:'true'\n"
      "name:'myfalse', path:'.myfalse', type:FALSE, val:'false'\n"
      "name:'<null>', path:'', type:OBJECT_END, val:'{\"c\":[\"foo\", \"bar\", "
      "{\"a\":9, \"b\": \"x\"}], \"mynull\": null, \"mytrue\": true, "
      "\"myfalse\": false}'\n";

  char buf[4096] = "";
  ASSERT(json_walk(s, strlen(s), cb, buf) == (int) strlen(s));
  ASSERT(strcmp(buf, result) == 0);
  return NULL;
}

/*
 * Tests with the path which is longer than JSON_MAX_PATH_LEN (at the moment,
 * 60)
 */
static const char *test_callback_api_long_path(void) {
  const char *s =
      "{\"MyWZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZvf\": {}, \"jYP-27917287424p\": {}}";

  const char *result =
      "name:'<null>', path:'', type:OBJECT_START, val:'<null>'\n"
      "name:'"
      "MyWZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZvf', "
      "path:'."
      "MyWZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ', "
      "type:OBJECT_START, val:'<null>'\n"
      "name:'<null>', "
      "path:'."
      "MyWZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ', "
      "type:OBJECT_END, val:'{}'\n"
      "name:'jYP-27917287424p', path:'.jYP-27917287424p', type:OBJECT_START, "
      "val:'<null>'\n"
      "name:'<null>', path:'.jYP-27917287424p', type:OBJECT_END, val:'{}'\n"
      "name:'<null>', path:'', type:OBJECT_END, "
      "val:'{"
      "\"MyWZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"
      "ZZZZZZZZZZZZZZZZvf\": {}, \"jYP-27917287424p\": {}}'\n";

  char buf[4096] = "";
  ASSERT(json_walk(s, strlen(s), cb, buf) == (int) strlen(s));
  ASSERT(strcmp(buf, result) == 0);
  return NULL;
}

static void scan_array(const char *str, int len, void *user_data) {
  struct json_token t;
  int i;
  char *buf = (char *) user_data;
  for (i = 0; json_scanf_array_elem(str, len, ".x", i, &t) > 0; i++) {
    sprintf(buf + strlen(buf), "%d[%.*s] ", i, t.len, t.ptr);
  }
}

static const char *test_scanf(void) {
  {

    char buf[100] = "";
    int a = 0, b = 0;
    char *d = NULL;

    const char *str =
        "{ a: 1234, b : true, \"c\": {x: [17, 78, -20]}, d: \"hi%20there\" }";

    ASSERT(json_scanf(str, strlen(str), "{a: %d, b: %B, c: [%M], d: %Q}", &a, &b,
                    &scan_array, buf, &d) == 4);
    ASSERT(a == 1234);
    ASSERT(b == 1);
    ASSERT(strcmp(buf, "0[17] 1[78] 2[-20] ") == 0);
    ASSERT(d != NULL);
    ASSERT(strcmp(d, "hi%20there") == 0);
    free(d);
  }

  {
    /* Test errors */
    const char *str = "{foo:1, bar:[2,3,4]}";
    size_t i;
    ASSERT(json_walk(str, strlen(str), NULL, NULL) == (int) strlen(str));
    for (i = 1; i < strlen(str); i++) {
      ASSERT(json_walk(str, i, NULL, NULL) == JSON_STRING_INCOMPLETE);
    }
  }

  {
    /* Test that paths are utf8 */
    const char *str = "{\"ы\": 123}";
    int x = 0;
    ASSERT(json_scanf(str, strlen(str), "{ы: %d}", &x) == 1);
    ASSERT(x == 123);
  }

  {
    /* Test that paths are utf8 */
    const char *str = "{a: 123, b: [1,2,3]}";
    struct json_token t;
    memset(&t, 0, sizeof(t));
    ASSERT(json_scanf(str, strlen(str), "{b: %T}", &t) == 1);
    ASSERT(t.type == JSON_TYPE_ARRAY_END);
    ASSERT(t.len == 7);
    ASSERT(strncmp(t.ptr, "[1,2,3]", t.len) == 0);
  }

  {
    /* Test zero termination */
    char *s = NULL;
    const char *str = "{a: \"foo\", b:123}";
    ASSERT(json_scanf(str, strlen(str), "{a: %Q}", &s) == 1);
    ASSERT(s != NULL);
    ASSERT(s[3] == '\0');
    free(s);
  }

  {
    /* Test for scalar value being a root element */
    int n = 0;
    const char *str = " true ";
    ASSERT(json_scanf(str, strlen(str), " %B ", &n) == 1);
    ASSERT(n == 1);
  }

  {
    /* Test array of objects */
    const char *str = " { \"a\": [ {\"b\": 123}, {\"b\": 345} ]} ";
    int i, value, len = strlen(str), values[] = {123, 345};
    struct json_token t;

    /* Scan each array element into a token */
    for (i = 0; json_scanf_array_elem(str, len, ".a", i, &t) > 0; i++) {
      /* Now scan each token */
      ASSERT(t.type == JSON_TYPE_OBJECT_END);
      ASSERT(json_scanf(t.ptr, t.len, "{b: %d}", &value) == 1);
      ASSERT((size_t) i < sizeof(values) / sizeof(values[0]));
      ASSERT(values[i] == value);
    }
    ASSERT(i == 2);
  }

  {
    const char *str = "{a : [\"foo\", \"\", \"a\"] }";
    struct json_token t;
    ASSERT(json_scanf_array_elem(str, strlen(str), ".a", 0, &t) == 3);
    ASSERT(json_scanf_array_elem(str, strlen(str), ".a", 1, &t) == 0);
    ASSERT(json_scanf_array_elem(str, strlen(str), ".a", 2, &t) == 1);
    ASSERT(json_scanf_array_elem(str, strlen(str), ".a", 3, &t) == -1);
  }

  {
    const char *str = "{a : \"foo\\b\\f\\n\\r\\t\\\\\" }";
    char *result;
    ASSERT(json_scanf(str, strlen(str), "{a: %Q}", &result) == 1);
    ASSERT(strcmp(result, "foo\b\f\n\r\t\\") == 0);
    free(result);

    ASSERT(json_scanf(str, 9, "{a: %Q}", &result) == 0);
  }

  {
    const char *str = "{a : \"привет\" }";
    char *result;
    ASSERT(json_scanf(str, strlen(str), "{a: %Q}", &result) == 1);
    ASSERT(strcmp(result, "привет") == 0);
    free(result);
  }

  {
    const char *str = "{a : \"YTI=\" }";
    int len;
    char *result;
    ASSERT(json_scanf(str, strlen(str), "{a: %V}", &result, &len) == 1);
    ASSERT(len == 2);
    ASSERT(strcmp(result, "a2") == 0);
    free(result);
  }

  {
    const char *str = "{a : \"61626320\" }";
    int len = 0;
    char *result = NULL;
    ASSERT(json_scanf(str, strlen(str), "{a: %H}", &len, &result) == 1);
    ASSERT(len == 4);
    ASSERT(strcmp(result, "abc ") == 0);
    free(result);
  }

  {
    const char *str = "{a : \"0L/RgNC40LLQtdGC0Ys=\" }";
    int len;
    char *result;
    ASSERT(json_scanf(str, strlen(str), "{a: %V}", &result, &len) == 1);
    ASSERT(len == 14);
    ASSERT(strcmp(result, "приветы") == 0);
    free(result);
  }

  {
    const char *str = "{a : null }";
    char *result = (char *) 123;
    ASSERT(json_scanf(str, strlen(str), "{a: %Q}", &result) == 0);
    ASSERT(result == NULL);
    free(result);
  }

  {
    int a = 0;
    bool b = false;
    int c = 0xFFFFFFFF;
    const char *str = "{\"b\":true,\"c\":false,\"a\":2}";
    ASSERT(json_scanf(str, strlen(str), "{a:%d, b:%B, c:%B}", &a, &b, &c) == 3);
    ASSERT(a == 2);
    ASSERT(b == true);
    if (sizeof(bool) == 1)
      ASSERT((char) c == false);
    else
      ASSERT(c == false);
  }

  {
    const char *str = "{ fa: 1, fb: 2.34, fc: 5.67 }";
    float a = 1.0f, b = 2.34f;
    double c = 5.67;
    float fa = 0.0, fb = 0.0;
    double fc = 0.0;
    ASSERT(json_scanf(str, strlen(str), "{fa: %f, fb: %f, fc: %lf}", &fa, &fb,
                      &fc) == 3);
    ASSERT(fa == a);
    ASSERT(fb == b);
    ASSERT(fc == c);
  }

  return NULL;
}

static const char *test_json_unescape(void) {
  ASSERT(json_unescape("foo", 3, NULL, 0) == 3);
  ASSERT(json_unescape("foo\\", 4, NULL, 0) == JSON_STRING_INCOMPLETE);
  ASSERT(json_unescape("foo\\x", 5, NULL, 0) == JSON_STRING_INVALID);
  ASSERT(json_unescape("\\ueeee", 5, NULL, 0) == JSON_STRING_INVALID);
  return NULL;
}

static void cb2(void *data, const char *name, size_t name_len, const char *path,
                const struct json_token *token) {
  struct json_token *pt = (struct json_token *) data;
  pt->ptr = token->ptr;
  pt->len = token->len;
  (void) path;
  (void) name_len;
  (void) name;
}

static const char *test_parse_string(void) {
  const char *str = " \" foo\\bar\"";
  const int str_len = strlen(str);
  struct json_token t;
  struct walk_ctx ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.end = str + str_len;
  ctx.cur = str;
  ctx.callback_data = (void *) &t;
  ctx.callback = cb2;

  ASSERT(parse_string(&ctx) == 0);
  ASSERT(strncmp(t.ptr, " foo\\bar", t.len) == 0);

  return NULL;
}

static const char *test_eos(void) {
  const char *s = "{\"a\": 12345}";
  size_t n = 999;
  char *buf = (char *) malloc(n);
  int s_len = strlen(s), a = 0;
  memset(buf, 'x', n);
  memcpy(buf, s, s_len);
  ASSERT(json_scanf(buf, n, "{a:%d}", &a) == 1);
  ASSERT(a == 12345);
  free(buf);
  return NULL;
}

static int compare_file(const char *file_name, const char *s) {
  int res = -1;
  char *p = json_fread(file_name);
  if (p == NULL) return res;
  res = strcmp(p, s);
  free(p);
  return res == 0;
}

static const char *test_fprintf(void) {
  const char *fname = "a.json";
  char *p;
  ASSERT(json_fprintf(fname, "{a:%d}", 123) > 0);
  ASSERT((p = json_fread(fname)) != NULL);
  ASSERT(strcmp(p, "{\"a\":123}\n") == 0);
  free(p);
  remove(fname);
  ASSERT(json_fread(fname) == NULL);
  return NULL;
}

static const char *test_json_setf(void) {
  char buf[200];
  const char *s1 = "{ \"a\": 123, \"b\": [ 1 ], \"c\": true }";

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 = "{ \"a\": 7, \"b\": [ 1 ], \"c\": true }";
    int res = json_setf(s1, strlen(s1), &out, ".a", "%d", 7);
    ASSERT(res == 1);
    ASSERT(strcmp(buf, s2) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 = "{ \"a\": 123, \"b\": false, \"c\": true }";
    int res = json_setf(s1, strlen(s1), &out, ".b", "%B", 0);
    ASSERT(res == 1);
    ASSERT(strcmp(buf, s2) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 = "{ \"a\": 123, \"b\": [ 2 ], \"c\": true }";
    int res = json_setf(s1, strlen(s1), &out, ".b[0]", "%d", 2);
    ASSERT(res == 1);
    ASSERT(strcmp(buf, s2) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 = "{ \"b\": [ 1 ], \"c\": true }";
    int res = json_setf(s1, strlen(s1), &out, ".a", NULL);
    ASSERT(res == 1);
    ASSERT(strcmp(buf, s2) == 0);
  }

  {
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 = "{ \"a\": 123, \"b\": [ 1 ] }";
    int res = json_setf(s1, strlen(s1), &out, ".c", NULL);
    ASSERT(res == 1);
    ASSERT(strcmp(buf, s2) == 0);
  }

  {
    /* Delete non-existent key */
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s = "{\"a\":1}";
    int res = json_setf(s, strlen(s), &out, ".d", NULL);
    ASSERT(res == 0);
    ASSERT(strcmp(buf, s) == 0);
  }

  {
    /* Delete non-existent key, spaces in obj */
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    int res = json_setf(s1, strlen(s1), &out, ".d", NULL);
    ASSERT(res == 0);
    ASSERT(strcmp(buf, s1) == 0);
  }

  {
    /* Change the whole JSON object */
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 = "123";
    int res = json_setf(s1, strlen(s1), &out, "", "%d", 123);
    ASSERT(res == 1);
    ASSERT(strcmp(buf, s2) == 0);
  }

  {
    /* Add missing keys */
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 =
        "{ \"a\": 123, \"b\": [ 1 ], \"c\": true,\"d\":{\"e\":8} }";
    int res = json_setf(s1, strlen(s1), &out, ".d.e", "%d", 8);
    ASSERT(res == 0);
    ASSERT(strcmp(buf, s2) == 0);
  }

  {
    /* Append to arrays */
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 = "{ \"a\": 123, \"b\": [ 1,2 ], \"c\": true }";
    int res = json_setf(s1, strlen(s1), &out, ".b[]", "%d", 2);
    ASSERT(res == 0);
    ASSERT(strcmp(buf, s2) == 0);
  }

  {
    /* Delete from array */
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 = "{ \"a\": 123, \"b\": [ ], \"c\": true }";
    int res = json_setf(s1, strlen(s1), &out, ".b[0]", NULL);
    ASSERT(res == 1);
    ASSERT(strcmp(buf, s2) == 0);
  }

  {
    /* Create array and push value  */
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 = "{ \"a\": 123, \"b\": [ 1 ], \"c\": true,\"d\":[3] }";
    int res = json_setf(s1, strlen(s1), &out, ".d[]", "%d", 3);
    // printf("[%s]\n[%s]\n", buf, s2);
    ASSERT(res == 0);
    ASSERT(strcmp(buf, s2) == 0);
  }

  return NULL;
}

static const char *test_prettify(void) {
  const char *fname = "a.json";
  char buf[200];

  {
    const char *s1 = "{ \"a\":   1, \"b\":2,\"c\":[null,\"aa\",{},true]}";
    struct json_out out = JSON_OUT_BUF(buf, sizeof(buf));
    const char *s2 =
        "{\n  \"a\": 1,\n  \"b\": 2,\n  \"c\": [\n    null,\n    \"aa\",\n    "
        "{},\n    true\n  ]\n}";
    ASSERT(json_prettify(s1, strlen(s1), &out) > 0);
    ASSERT(strcmp(buf, s2) == 0);
  }

  {
    remove(fname);
    ASSERT(json_prettify_file(fname) == -1);
  }

  {
    ASSERT(compare_file(fname, "") == -1);
    json_fprintf(fname, "::");
    ASSERT(json_prettify_file(fname) == JSON_STRING_INVALID);
    ASSERT(compare_file(fname, "::\n"));
    remove(fname);
  }

  {
    ASSERT(compare_file(fname, "") == -1);
    json_fprintf(fname, "{");
    ASSERT(json_prettify_file(fname) == JSON_STRING_INCOMPLETE);
    ASSERT(compare_file(fname, "{\n"));
    remove(fname);
  }

  {
    ASSERT(compare_file(fname, "") == -1);
    json_fprintf(fname, "%d", 123);
    ASSERT(compare_file(fname, "123\n"));
    ASSERT(json_prettify_file(fname) > 0);
    ASSERT(compare_file(fname, "123\n"));
    remove(fname);
  }

  {
    const char *s = "{\n  \"a\": 123\n}\n";
    ASSERT(compare_file(fname, "") == -1);
    json_fprintf(fname, "{a:%d}", 123);
    ASSERT(json_prettify_file(fname) > 0);
    ASSERT(compare_file(fname, s));
    remove(fname);
  }

  return NULL;
}

static const char *test_json_next(void) {
  const char *s = "{ \"a\": [], \"b\": [ 1, {} ], \"c\": true }";
  struct json_token key, val;
  char buf[100];
  int len = strlen(s);

  {
    /* Traverse an object */
    void *h = NULL;
    int i = 0;
    const char *results[] = {"[a] -> [[]]", "[b] -> [[ 1, {} ]]",
                             "[c] -> [true]"};
    while ((h = json_next_key(s, len, h, "", &key, &val)) != NULL) {
      snprintf(buf, sizeof(buf), "[%.*s] -> [%.*s]", key.len, key.ptr, val.len,
               val.ptr);
      ASSERT(strcmp(results[i], buf) == 0);
      i++;
    }
    ASSERT(i == 3);
  }

  {
    /* Traverse an array */
    void *h = NULL;
    int i = 0, idx;
    const char *results[] = {"[0] -> [1]", "[1] -> [{}]"};
    while ((h = json_next_elem(s, len, h, ".b", &idx, &val)) != NULL) {
      snprintf(buf, sizeof(buf), "[%d] -> [%.*s]", idx, val.len, val.ptr);
      ASSERT(strcmp(results[i], buf) == 0);
      i++;
    }
    ASSERT(i == 2);
  }

  {
    /* Traverse more complex object */
    const char *s2 = "{ \"a\": [], \"b\": { \"c\": true, \"d\": 1234 } }";
    void *h = NULL;
    int i = 0;
    const char *results[] = {"[c] -> [true]", "[d] -> [1234]"};
    while ((h = json_next_key(s2, len, h, ".b", &key, &val)) != NULL) {
      snprintf(buf, sizeof(buf), "[%.*s] -> [%.*s]", key.len, key.ptr, val.len,
               val.ptr);
      ASSERT(strcmp(results[i], buf) == 0);
      i++;
    }
    ASSERT(i == 2);
  }

  return NULL;
}

static const char *run_all_tests(void) {
  RUN_TEST(test_json_next);
  RUN_TEST(test_prettify);
  RUN_TEST(test_eos);
  RUN_TEST(test_scanf);
  RUN_TEST(test_errors);
  RUN_TEST(test_json_printf);
  RUN_TEST(test_callback_api);
  RUN_TEST(test_callback_api_long_path);
  RUN_TEST(test_json_unescape);
  RUN_TEST(test_parse_string);
  RUN_TEST(test_fprintf);
  RUN_TEST(test_json_setf);
  return NULL;
}

int main(void) {
  const char *fail_msg = run_all_tests();
  printf("%s, tests run: %d\n", fail_msg ? "FAIL" : "PASS", static_num_tests);
  return fail_msg == NULL ? EXIT_SUCCESS : EXIT_FAILURE;
}

