/*
    Authors:
        Jakub Hrozek <jhrozek@redhat.com>
        Pavel Březina <pbrezina@redhat.com>

    Copyright (C) 2014 Red Hat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <talloc.h>
#include <errno.h>
#include <popt.h>

#include "sbus/sssd_dbus.h"
#include "tests/cmocka/common_mock.h"
#include "tests/common.h"

#define BASE_PATH "/some/path"

void test_sbus_opath_strip_prefix(void **state)
{
    const char *prefix = "/org/freedesktop/sssd/";
    const char *path = "/org/freedesktop/sssd/infopipe";
    const char *strip;

    strip = sbus_opath_strip_prefix(path, prefix);
    assert_non_null(prefix);
    assert_string_equal(strip, "infopipe");

    strip = sbus_opath_strip_prefix("/other/path", prefix);
    assert_null(strip);
}

void test_sbus_opath_escape_unescape(void **state)
{
    char *escaped;
    char *raw;
    TALLOC_CTX *mem_ctx;

    assert_true(leak_check_setup());
    mem_ctx = talloc_new(global_talloc_context);

    escaped = sbus_opath_escape_part(mem_ctx, "noescape");
    assert_non_null(escaped);
    assert_string_equal(escaped, "noescape");
    raw = sbus_opath_unescape_part(mem_ctx, escaped);
    talloc_free(escaped);
    assert_non_null(raw);
    assert_string_equal(raw, "noescape");
    talloc_free(raw);

    escaped = sbus_opath_escape_part(mem_ctx, "redhat.com");
    assert_non_null(escaped);
    assert_string_equal(escaped, "redhat_2ecom"); /* dot is 0x2E in ASCII */
    raw = sbus_opath_unescape_part(mem_ctx, escaped);
    talloc_free(escaped);
    assert_non_null(raw);
    assert_string_equal(raw, "redhat.com");
    talloc_free(raw);

    escaped = sbus_opath_escape_part(mem_ctx, "path_with_underscore");
    assert_non_null(escaped);
    /* underscore is 0x5F in ascii */
    assert_string_equal(escaped, "path_5fwith_5funderscore");
    raw = sbus_opath_unescape_part(mem_ctx, escaped);
    talloc_free(escaped);
    assert_non_null(raw);
    assert_string_equal(raw, "path_with_underscore");
    talloc_free(raw);

    /* empty string */
    escaped = sbus_opath_escape_part(mem_ctx, "");
    assert_non_null(escaped);
    assert_string_equal(escaped, "_");
    raw = sbus_opath_unescape_part(mem_ctx, escaped);
    talloc_free(escaped);
    assert_non_null(raw);
    assert_string_equal(raw, "");
    talloc_free(raw);

    /* negative tests */
    escaped = sbus_opath_escape_part(mem_ctx, NULL);
    assert_null(escaped);
    raw = sbus_opath_unescape_part(mem_ctx, "wrongpath_");
    assert_null(raw);

    assert_true(leak_check_teardown());
}

void test_sbus_opath_compose(void **state)
{
    char *path;

    /* Doesn't need escaping */
    path = sbus_opath_compose(NULL, BASE_PATH, "domname");
    assert_non_null(path);
    assert_string_equal(path, BASE_PATH "/domname");
    talloc_free(path);
}

void test_sbus_opath_compose_escape(void **state)
{
    char *path;

    /* A dot needs escaping */
    path = sbus_opath_compose(NULL, BASE_PATH, "redhat.com", NULL);
    assert_non_null(path);
    assert_string_equal(path, BASE_PATH "/redhat_2ecom");
    talloc_free(path);
}

void test_sbus_opath_get_object_name(void **state)
{
    const char *path = BASE_PATH "/redhat_2ecom";
    char *name;

    name = sbus_opath_get_object_name(NULL, path, BASE_PATH);
    assert_non_null(name);
    assert_string_equal(name, "redhat.com");
    talloc_free(name);

    name = sbus_opath_get_object_name(NULL, path, BASE_PATH "/");
    assert_non_null(name);
    assert_string_equal(name, "redhat.com");
    talloc_free(name);

    name = sbus_opath_get_object_name(NULL, BASE_PATH, BASE_PATH);
    assert_null(name);

    name = sbus_opath_get_object_name(NULL, "invalid", BASE_PATH);
    assert_null(name);
}

int main(int argc, const char *argv[])
{
    poptContext pc;
    int opt;
    struct poptOption long_options[] = {
        POPT_AUTOHELP
        SSSD_DEBUG_OPTS
        POPT_TABLEEND
    };

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sbus_opath_strip_prefix),
        cmocka_unit_test(test_sbus_opath_escape_unescape),
        cmocka_unit_test(test_sbus_opath_compose),
        cmocka_unit_test(test_sbus_opath_compose_escape),
        cmocka_unit_test(test_sbus_opath_get_object_name)
    };

    /* Set debug level to invalid value so we can deside if -d 0 was used. */
    debug_level = SSSDBG_INVALID;

    pc = poptGetContext(argv[0], argc, argv, long_options, 0);
    while((opt = poptGetNextOpt(pc)) != -1) {
        switch(opt) {
        default:
            fprintf(stderr, "\nInvalid option %s: %s\n\n",
                    poptBadOption(pc, 0), poptStrerror(opt));
            poptPrintUsage(pc, stderr, 0);
            return 1;
        }
    }
    poptFreeContext(pc);

    DEBUG_CLI_INIT(debug_level);

    return cmocka_run_group_tests(tests, NULL, NULL);
}