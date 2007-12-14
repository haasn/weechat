/*
 * Copyright (c) 2003-2007 by FlashCode <flashcode@flashtux.org>
 * See README for License detail, AUTHORS for developers list.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* gui-curses-main.c: main loop for Curses GUI */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "../../core/weechat.h"
#include "../../core/wee-config.h"
#include "../../core/wee-hook.h"
#include "../../core/wee-string.h"
#include "../../core/wee-utf8.h"
#include "../../core/wee-util.h"
#include "../../plugins/plugin.h"
#include "../gui-main.h"
#include "../gui-chat.h"
#include "../gui-color.h"
#include "../gui-infobar.h"
#include "../gui-input.h"
#include "../gui-history.h"
#include "../gui-window.h"
#include "gui-curses.h"


/*
 * gui_main_pre_init: pre-initialize GUI (called before gui_init)
 */

void
gui_main_pre_init (int *argc, char **argv[])
{
    /* nothing for Curses interface */
    (void) argc;
    (void) argv;
}

/*
 * gui_main_init: init GUI
 */

void
gui_main_init ()
{
    struct t_gui_buffer *ptr_buffer;
    
    initscr ();
    
    curs_set (1);
    noecho ();
    nodelay (stdscr, TRUE);
    raw ();
    
    gui_color_init ();
    gui_chat_prefix_build ();
    
    gui_infobar = NULL;
    
    gui_ok = ((COLS > WINDOW_MIN_WIDTH) && (LINES > WINDOW_MIN_HEIGHT));

    refresh ();
    
    /* init clipboard buffer */
    gui_input_clipboard = NULL;
    
    /* get time length */
    gui_chat_time_length = util_get_time_length (CONFIG_STRING(config_look_buffer_time_format));

    /* create new window/buffer */
    if (gui_window_new (NULL, 0, 0, COLS, LINES, 100, 100))
    {
        gui_current_window = gui_windows;
        ptr_buffer = gui_buffer_new (NULL, "weechat", "weechat", NULL);
        if (ptr_buffer)
        {
            gui_init_ok = 1;
            gui_buffer_set_title (ptr_buffer,
                                  PACKAGE_STRING " " WEECHAT_COPYRIGHT_DATE
                                  " - " WEECHAT_WEBSITE);
            gui_window_redraw_buffer (ptr_buffer);
        }
        else
            gui_init_ok = 0;
        
        if (CONFIG_BOOLEAN(config_look_set_title))
            gui_window_title_set ();
        
        signal (SIGWINCH, gui_window_refresh_screen_sigwinch);
    }
}

/*
 * gui_main_quit: quit weechat (signal received)
 */

void
gui_main_quit ()
{
    quit_weechat = 1;
}

/*
 * gui_main_loop: main loop for WeeChat with ncurses GUI
 */

void
gui_main_loop ()
{
    struct t_gui_buffer *ptr_buffer;
    struct timeval tv_timeout;
    fd_set read_fds, write_fds, except_fds;
    
    quit_weechat = 0;
    
    /* if SIGTERM or SIGHUP received => quit */
    signal (SIGTERM, gui_main_quit);
    signal (SIGHUP, gui_main_quit);
    
    while (!quit_weechat)
    {
        /* execute hook timers */
        hook_timer_exec ();
        
        /* infobar count down */
        
        
        /* refresh needed ? */
        if (gui_refresh_screen_needed)
            gui_window_refresh_screen (0);
        
        for (ptr_buffer = gui_buffers; ptr_buffer;
             ptr_buffer = ptr_buffer->next_buffer)
        {
            if (ptr_buffer->chat_refresh_needed)
            {
                gui_chat_draw (ptr_buffer, 0);
                ptr_buffer->chat_refresh_needed = 0;
            }
        }
        
        /* wait for keyboard or network activity */
        FD_ZERO (&read_fds);
        FD_ZERO (&write_fds);
        FD_ZERO (&except_fds);
        hook_fd_set (&read_fds, &write_fds, &except_fds);
        FD_SET (STDIN_FILENO, &read_fds);
        if (hook_timer_time_to_next (&tv_timeout))
            select (FD_SETSIZE, &read_fds, &write_fds, &except_fds, &tv_timeout);
        else
            select (FD_SETSIZE, &read_fds, &write_fds, &except_fds, NULL);
        if (FD_ISSET (STDIN_FILENO, &read_fds))
        {
            gui_keyboard_read ();
            gui_keyboard_flush ();
        }
        hook_fd_exec (&read_fds, &write_fds, &except_fds);
    }
}

/*
 * gui_main_end: GUI end
 */

void
gui_main_end ()
{
    /* free clipboard buffer */
    if (gui_input_clipboard)
        free (gui_input_clipboard);
    
    /* delete all windows */
    while (gui_windows)
        gui_window_free (gui_windows);
    gui_window_tree_free (&gui_windows_tree);

    /* delete all buffers */
    while (gui_buffers)
        gui_buffer_free (gui_buffers, 0);
    
    /* delete global history */
    gui_history_global_free ();
    
    /* delete infobar messages */
    while (gui_infobar)
        gui_infobar_remove ();

    /* reset title */
    if (CONFIG_BOOLEAN(config_look_set_title))
	gui_window_title_reset ();
    
    /* end of Curses output */
    refresh ();
    endwin ();
}
