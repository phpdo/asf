/*
  +----------------------------------------------------------------------+
  | API Services Framework                                               |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2018 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Jiapeng Fan <fanjiapeng@126.com>                             |
  +----------------------------------------------------------------------+
*/

#ifndef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_asf.h"
#include "asf_func.h"
#include "Zend/zend_stream.h"
#include "ext/standard/php_filestat.h" /* php_stat */
#include "zend_smart_str.h" /* smart_str */
#include "ext/standard/php_string.h" /* php_basename */
#include "asf_exception.h"

_Bool asf_func_isempty(const char *s) /* {{{ */
{
    while (*s != '\0') {
        if (!isspace(*s)) {
            return 0;
        }
        s++;
    }
    return 1;
}
/* }}} */

void asf_func_set_cur_module(char *module) /* {{{ */
{
    char *path = NULL;
    size_t path_len = 0;

    path_len = spprintf(&path, 0, "%s%c%s%c", ZSTR_VAL(ASF_G(root_path)), DEFAULT_SLASH, module, DEFAULT_SLASH);

    if (EXPECTED(path_len)) {
        ASF_G(root_path_route) = zend_string_init(path, path_len, 0);
    }

    efree(path);
}
/* }}} */

php_stream *asf_func_fopen(const char *fpath, size_t fpath_len, zend_string *dpath, _Bool exception) /* {{{ */
{
    zval exists_flag;
    char *dirname = NULL;
    size_t dirname_len = 0;
    int ret = 1;

    if (dpath == NULL) {
        dirname = estrndup(fpath, fpath_len);
        dirname_len = php_dirname(dirname, fpath_len);
    } else {
        dirname = ZSTR_VAL(dpath);
        dirname_len = ZSTR_LEN(dpath);
    }

    php_stat(dirname, dirname_len, FS_IS_DIR, &exists_flag);

    if (Z_TYPE(exists_flag) == IS_FALSE) {
        ret = php_stream_mkdir(dirname, 0755, PHP_STREAM_MKDIR_RECURSIVE, NULL);
    }

    if (dpath == NULL) {
        efree(dirname);
    }

    if (!ret) {
        return NULL;
    }

    ZVAL_UNDEF(&exists_flag);
    php_stat(fpath, fpath_len, FS_IS_FILE, &exists_flag);

    char *mode = NULL;
    if (Z_TYPE(exists_flag) == IS_FALSE) {
        mode = "wb";
    } else {
        mode = "ab";
    }

    php_stream *stream = php_stream_fopen(fpath, mode, NULL);

    if (NULL == stream && !exception) {
        asf_trigger_error(ASF_ERR_LOGGER_STREAM_ERROR, "File open failed: %s", fpath);
    }

    return stream;
}
/* }}} */

void asf_func_shutdown_buffer(_Bool exception) /* {{{ */
{
    zend_string *key = NULL;
    zval *entry_1 = NULL, *entry_2 = NULL;
    php_stream *stream = NULL;
    smart_str buf = {0};

    HashTable *ht = Z_ARRVAL(ASF_G(log_buffer));

    ZEND_HASH_FOREACH_STR_KEY_VAL(ht, key, entry_1) {
        if (!key || IS_ARRAY != Z_TYPE_P(entry_1)) {
            continue;
        }

        if (NULL == (stream = asf_func_fopen(ZSTR_VAL(key), ZSTR_LEN(key), NULL, exception))) {
            continue;
        }

        if (zend_hash_num_elements(Z_ARRVAL_P(entry_1)) == 1) { 
            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(entry_1), entry_2) {
                php_stream_write(stream, Z_STRVAL_P(entry_2), Z_STRLEN_P(entry_2));
            } ZEND_HASH_FOREACH_END();
        } else {
            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(entry_1), entry_2) {
                smart_str_appendl(&buf, Z_STRVAL_P(entry_2), Z_STRLEN_P(entry_2));
            } ZEND_HASH_FOREACH_END();

            smart_str_0(&buf);
            php_stream_write(stream, ZSTR_VAL(buf.s), ZSTR_LEN(buf.s));
            smart_str_free(&buf);
        }

        php_stream_close(stream);
        stream = NULL;
    } ZEND_HASH_FOREACH_END();

    zval_ptr_dtor(&ASF_G(log_buffer));
    ZVAL_UNDEF(&ASF_G(log_buffer));
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
