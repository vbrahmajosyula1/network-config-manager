/* Copyright 2023 VMware, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "alloc-util.h"
#include "ctl.h"
#include "macros.h"
#include "log.h"

int ctl_manager_new(const Ctl *ctl_commands, CtlManager **ret) {
        _auto_cleanup_ CtlManager *m = NULL;

        assert(ctl_commands);
        assert(ret);

        m = new(CtlManager, 1);
        if (!m)
                return log_oom();

        *m = (CtlManager) {
               .table = g_hash_table_new(g_str_hash, g_str_equal),
               .table_alias = g_hash_table_new(g_str_hash, g_str_equal),
               .commands = (Ctl *) ctl_commands,
        };
        if (!m->table)
                return log_oom();

        if (!m->table_alias)
                return log_oom();

        for (size_t i = 0; ctl_commands[i].name; i++)
                g_hash_table_insert(m->table, (gpointer *) ctl_commands[i].name, (gpointer *) &ctl_commands[i]);

        for (size_t i = 0; ctl_commands[i].alias; i++)
                g_hash_table_insert(m->table_alias, (gpointer *) ctl_commands[i].alias, (gpointer *) &ctl_commands[i]);

        *ret = steal_ptr(m);
        return 0;
}

void ctl_free(CtlManager *m) {
        if (!m)
                return;

        g_hash_table_unref(m->table);
        g_hash_table_unref(m->table_alias);
        free(m);
}

static Ctl *ctl_get_command(const CtlManager *m, const char *name) {
        assert(m);
        assert(name);

        if (g_hash_table_lookup(m->table, name))
                return g_hash_table_lookup(m->table, name);
        else
                return g_hash_table_lookup(m->table_alias, name);
}

int ctl_run_command(const CtlManager *m, int argc, char *argv[]) {
        Ctl *command = NULL;
        int remaining_argc;
        char *name;

        assert(m);

        remaining_argc = argc - optind;

        argv += optind;
        optind = 0;
        name = argv[0];

        /* run default if no command specified */
        if (!name) {
                for (size_t i = 0;; i++) {
                        if (m->commands[i].default_command)
                                command = m->commands;
                        remaining_argc = 1;
                        return command->run(remaining_argc, argv);
                }
        }

        command = ctl_get_command(m, name);
        if (!command) {
                log_warning("Unknown ctl command '%s'.", name);
                return -EINVAL;
        }

        if (command->min_args != WORD_ANY && (unsigned) remaining_argc <= command->min_args) {
                log_warning("Too few arguments.");
                return -EINVAL;
        }

        if (command->max_args != WORD_ANY && (unsigned) remaining_argc > command->max_args) {
                log_warning("Too many arguments.");
                return -EINVAL;
        }

        return command->run(remaining_argc, argv);
}
