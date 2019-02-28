/* editor.cc - Implementation of our editor class.
 *
 * -----------------------------------------------------------------------
 *
 * Copyright (C) 2016 Steve Kemp https://steve.kemp.fi/
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <algorithm>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/socket.h>

#include "editor.h"
#include "intro.h"
#include "util.h"


bool one_key_pressed = false;
std::vector<std::string> intro;

/**
 * Constructor.
 */
Editor::Editor()
{
    /**
     * Create a new state and ensure that we have a buffer
     * which we can work with.
     */
    m_state = new editorState();

    /*
     * Create a new buffer for messages.
     */
    Buffer *tmp = new Buffer("*Messages*");
    m_state->buffers.push_back(tmp);
    m_state->current_buffer = 0;

    /*
     * Ensure that our status-message is empty.
     */
    memset(m_state->statusmsg, '\0', sizeof(m_state->statusmsg));


    /*
     * Setup lua.
     */
    m_lua = luaL_newstate();
    luaopen_base(m_lua);
    luaL_openlibs(m_lua);

    /*
     * Bind functions.
     */
    lua_register(m_lua, "at", at_lua);
    lua_register(m_lua, "buffer", buffer_lua);
    lua_register(m_lua, "buffer_data", buffer_data_lua);
    lua_register(m_lua, "buffer_name", buffer_name_lua);
    lua_register(m_lua, "buffers", buffers_lua);
    lua_register(m_lua, "create_buffer", create_buffer_lua);
    lua_register(m_lua, "delete", delete_lua);
    lua_register(m_lua, "directory_entries", directory_entries_lua);
    lua_register(m_lua, "dirty", dirty_lua);
    lua_register(m_lua, "eof", eof_lua);
    lua_register(m_lua, "eol", eol_lua);
    lua_register(m_lua, "exists", exists_lua);
    lua_register(m_lua, "exit", exit_lua);
    lua_register(m_lua, "height", height_lua);
    lua_register(m_lua, "insert", insert_lua);
    lua_register(m_lua, "key", key_lua);
    lua_register(m_lua, "kill_buffer", kill_buffer_lua);
    lua_register(m_lua, "mark", mark_lua);
    lua_register(m_lua, "menu", menu_lua);
    lua_register(m_lua, "move", move_lua);
    lua_register(m_lua, "open", open_lua);
    lua_register(m_lua, "point", point_lua);
    lua_register(m_lua, "prompt", prompt_lua);
    lua_register(m_lua, "save", save_lua);
    lua_register(m_lua, "search", search_lua);
    lua_register(m_lua, "selection", selection_lua);
    lua_register(m_lua, "sof", sof_lua);
    lua_register(m_lua, "sol", sol_lua);
    lua_register(m_lua, "status", status_lua);
    lua_register(m_lua, "syntax", syntax_lua);
    lua_register(m_lua, "text", text_lua);
    lua_register(m_lua, "update_colours", update_colours_lua);
    lua_register(m_lua, "width", width_lua);

    /*
     * register our colours
     */
    lua_pushinteger(m_lua, 1);
    lua_setglobal(m_lua, "RED");

    lua_pushinteger(m_lua, 2);
    lua_setglobal(m_lua, "GREEN");

    lua_pushinteger(m_lua, 3);
    lua_setglobal(m_lua, "YELLOW");

    lua_pushinteger(m_lua, 4);
    lua_setglobal(m_lua, "BLUE");

    lua_pushinteger(m_lua, 5);
    lua_setglobal(m_lua, "MEGENTA");

    lua_pushinteger(m_lua, 6);
    lua_setglobal(m_lua, "CYAN");

    lua_pushinteger(m_lua, 7);
    lua_setglobal(m_lua, "WHITE");

    lua_pushinteger(m_lua, 8);
    lua_setglobal(m_lua, "REV_RED");

    lua_pushinteger(m_lua, 9);
    lua_setglobal(m_lua, "REV_GREEN");

    lua_pushinteger(m_lua, 10);
    lua_setglobal(m_lua, "REV_YELLOW");

    lua_pushinteger(m_lua, 11);
    lua_setglobal(m_lua, "REV_BLUE");

    lua_pushinteger(m_lua, 12);
    lua_setglobal(m_lua, "REV_MAGENTA");

    lua_pushinteger(m_lua, 13);
    lua_setglobal(m_lua, "REV_CYAN");

    lua_pushstring(m_lua, KILUA_VERSION);
    lua_setglobal(m_lua, "KILUA_VERSION");

}


/**
 * Destructor.
 */
Editor::~Editor()
{
    delete (m_state);
}


/**
 * Called by the main application at launch-time.
 */
void Editor::load_files(std::vector<std::string> files)
{

    /*
     * If we have no files .. create a scratch one.
     */
    if (files.size() > 0)
    {
        for (std::vector<std::string>::iterator it = files.begin(); it != files.end(); ++it)
        {
            Buffer *tmp = new Buffer((*it).c_str());
            m_state->buffers.push_back(tmp);
            m_state->current_buffer = m_state->buffers.size() - 1;

            call_lua("open", "s>", (*it).c_str());
        }
    }
    else
    {
        Buffer *tmp = new Buffer("default");
        m_state->buffers.push_back(tmp);
        m_state->current_buffer = m_state->buffers.size() - 1;
    }
}


/**
 * Run the main loop - polling for input, reacting to the same, and
 * refreshing the screen.
 */
void Editor::main_loop()
{

    while (1)
    {
        unsigned int ch;

        int res = get_wch(&ch);

        /*
         * Chances are this was a timeout.
         *
         * So invoke the handler and redraw.
         */
        if (res == ERR)
        {
            call_lua("on_idle", ">");
            draw_screen();
            continue;
        }

        /*
         * We've had at least one keystroke - so the welcome
         * message can go away.
         */
        one_key_pressed = true;

        /*
         * Expand the key.
         */
        const char *name = lookup_key(ch);


        if (strcmp(name, "KEY_RESIZE") == 0)
            continue;

        /*
         * This is a bit horrid.
         *
         * The intention is to ensure that Lua can process special
         * key-strokes, and they are not inserted.
         */
        if (name && strlen(name) > 1 &&
                (
                    (strncmp(name, "KEY_", 4) == 0) ||
                    (name[0] == '^') ||
                    (strncmp(name, "ESC", 3) == 0)
                ))
        {
            call_lua("on_key", "s>", name);
        }
        else
        {
            /*
             * Convert the character to a string, then call Lua.
             */
            char *ascii = Util::wchar2ascii(ch);
            call_lua("on_key", "s>", ascii);
            delete []ascii;
        }

        /*
         * Draw the screen.
         */
        draw_screen();
    }
}


/**
 * Return the current buffer.
 */
Buffer *Editor::current_buffer()
{
    return (m_state->buffers.at(m_state->current_buffer));
}


/**
 * Return the text in the status-bar.
 */
char * Editor::get_status()
{
    return (m_state->statusmsg);
}


/**
 * Set the status-bar text.
 */
void Editor::set_status(int log, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    memset(m_state->statusmsg, '\0', sizeof(m_state->statusmsg));
    vsnprintf(m_state->statusmsg, sizeof(m_state->statusmsg) - 1, fmt, ap);
    va_end(ap);

    /*
     * Append the message to the *Messages* buffer.
     */
    if (log)
    {
        int old = m_state->current_buffer;

        int b = buffer_by_name("*Messages*");

        if (b != -1)
        {
            m_state->current_buffer = b;

            wchar_t *wide = Util::ascii2wide(m_state->statusmsg);
            int size = wcslen(wide);

            for (int i = 0; i < size; i++)
            {
                insert(wide[i]);
            }

            delete []wide;

            insert('\n');
            m_state->current_buffer = old;
        }
    }
}


/**
 * Draw the screen, as well as the status-bar and the message-area.
 */
void Editor::draw_screen()
{
    /*
     * Clear the screen.
     */
    clear();

    /*
     * The current buffer, and row-count.
     */
    Buffer *cur = m_state->buffers.at(m_state->current_buffer);
    int rows = cur->rows.size();

    /*
     * Width of screen.
     */
    int w = width();

    /*
     * Count of characters which are before the screen position.
     *
     * We use this to show the marked region.
     */
    int count = 0;

    for (int y = 0; y < cur->rowoff; y++)
    {
        erow *row = cur->rows.at(y);
        int row_size = row->chars->size();
        count += row_size + 1;
    }

    /*
     * The position of the point and mark.
     */
    int m_pos = cur->pos2offset(cur->markx, cur->marky);
    int c_pos = cur->pos2offset(cur->cx + cur->coloff,  cur->cy + cur->rowoff);

    /*
     * The character offsets - the characters between these
     * two numbers should be in reverse.
     */
    int sel_min = std::min(m_pos, c_pos);
    int sel_max = std::max(m_pos, c_pos);

    /*
     * If the mark is not set disable the whole damn thing.
     */
    if ((cur->markx == -1) && (cur->marky == -1))
    {
        sel_min = -1;
        sel_max = -1;
    }


    /*
     * For each row ..
     */
    for (int y = 0; y < m_state->screenrows();  y++)
    {
        /*
         * If this row is past the end of our list - draw "~" and exit.
         */
        if ((y + cur->rowoff) >= rows)
        {
            std::wstring x;
            x += '~';

            /* Reset to white */
            color_set(7, NULL);
            mvwaddwstr(stdscr, y, 0, x.c_str());
            continue;
        }

        /*
         * The row of characters.
         */
        erow *row = cur->rows.at(y + cur->rowoff);
        int row_max = row->chars->size();

        /*
         * For each possible position in the row
         */
        int x = 0;

        for (int c = 0; c < row_max; c++)
        {
            /*
             * If this character is visible ..
             */
            if ((c >= cur->coloff) && (c < (cur->coloff + w)))
            {
                /*
                 * Default colour - white.
                 */
                int col = 7;

                /*
                 * Get & set the colour.
                 */
                if (c < (int)row->cols->size())
                    col = row->cols->at(c);

                color_set(col, NULL);

                /*
                 * Is the current character between the point
                 * and the mark?  If so enable the reverse-drawing.
                 */
                if (count >= sel_min && count <= sel_max)
                    attron(A_STANDOUT);

                /*
                 * Draw the character.
                 */
                std::wstring t = row->chars->at(c);

                /*
                 * Is it a TAB?  Change to space, because otherwise
                 * trailing whitespace screws up.
                 */
                if (t.at(0) == '\t')
                    t = ' ';

                /*
                 * Draw the character, and reset the colour to white.
                 */
                mvwaddwstr(stdscr, y, x, t.c_str());
                color_set(7, NULL); /* white */

                /*
                 * Is the current character between the point
                 * and the mark?  If so disable the reverse-drawing.
                 */
                if (count >= sel_min && count <= sel_max)
                    attroff(A_STANDOUT);

                count += 1;

                x += 1;
            }
            else
            {
                /*
                 *  none visible character in the row, we must
                 * account for it still.
                 */
                count += 1;
            }
        }

        count += 1; /*newline*/
    }

    if ((rows == 1) && (one_key_pressed == false))
    {
        /*
         * Setup the introductionary message.
         */
        init_intro();

        int row = 0;

        for (auto it = intro.begin(); it != intro.end() ; ++it)
        {
            mvwaddstr(stdscr, row, 0, (*it).c_str());
            row += 1;
        }
    }



    /*
     * Get the status-bar from Lua, if we can.
     */
    char* result = NULL;
    call_lua("get_status_bar", ">s",  &result);
    std::string status;

    if (result)
        status = result;
    else
        status = "Please define 'get_status_bar()'";

    while ((int)status.length() < m_state->screencols())
    {
        status += " ";
    }

    /*
     * Enable reverse.
     */
    attron(A_STANDOUT);
    mvwaddstr(stdscr, m_state->screenrows(), 0, status.c_str());
    attroff(A_STANDOUT);

    /*
     * Draw the message-area.
     */
    std::string s = get_status();

    if ((int)s.length() >  m_state->screencols())
        s = s.substr(s.length() - m_state->screencols() + 1);

    while ((int)s.length() < m_state->screencols())
    {
        s += " ";
    }

    mvwaddstr(stdscr, m_state->screenrows() + 1, 0, s.c_str());

    /*
     * The cursor can't be in the bottom two lines.
     */
    if (cur->cy >= (m_state->screenrows()))
        cur->cy  = (m_state->screenrows() - 1);

    /*
     * Show the cursor in the right location.
     */
    ::move(cur->cy, cur->cx);


    refresh();
}

/*
 * Magic.
 */
void Editor::insert(wchar_t c)
{
    /*
     * Get the current buffer.
     */
    Buffer *cur = m_state->buffers.at(m_state->current_buffer);

    /*
     * Current offset
     */
    int row = cur->cy + cur->rowoff;
    int col = cur->cx + cur->coloff;

    /*
     * The current row.
     */
    erow *cur_row = cur->rows.at(row);

    /*
     * If inserting a newline that's the same as inserting
     * a new row.
     */
    if (c == '\n')
    {
        /*
         * OK the user pressed RETURN.
         *
         * We know this means we need to create a new-line, which we'll insert.
         */
        erow *new_row = new erow();

        /*
         * Take the characters in the current row after the point.
         *
         * We're going to remove them, and insert them at the start
         * of the new row.
         */
        std::vector<std::wstring> tmp;

        /*
         * Copy the characters after the point.
         */
        for (std::vector<std::wstring>::iterator it = cur_row->chars->begin() + col; it != cur_row->chars->end(); ++it)
        {
            tmp.push_back(*it);
        }


        /*
         * Add the characters onto the new row.
         */
        for (std::vector<std::wstring>::iterator it = tmp.begin(); it != tmp.end(); ++it)
        {
            new_row->chars->push_back(*it);
        }

        /*
         * Erase the characters from the current row.
         */
        cur_row->chars->erase(cur_row->chars->begin() + col, cur_row->chars->end());

        /*
         * Insert the new row.
         */
        cur->rows.insert(cur->rows.begin() + row + 1, new_row);

        /*
         * Because we've added a new row we need to move down one
         * row, and to the start of the next line.
         */
        move("down");
        sol_lua(NULL);
        return;
    }

    /*
     * Trying to insert a character at an impossible position?
     */
    if (row > (int) cur->rows.size())
        return ;


    /*
     * We have a row.  Insert the character.
     */
    std::wstring x;
    x += c;

    /*
     * Insert the new character at the correct position.
     */
    cur_row->chars->insert(cur_row->chars->begin() + col, x);

    /*
     * Move right - this handles scrolling correctly.
     */
    move("right");

}


/*
 * More magic.
 */
void Editor::delete_char()
{
    /*
     * There are two cases - deletion from the middle/end
     * of a row, and deletion at the start of the row.
     *
     * The first case is simple, we just find the current
     * row and erase one "character" from it.
     *
     * The second case is harder, we need to remove the
     * current row, and append the characters after the mark
     * to it.
     *
     * punt for now
     */

    /*
     * Get the current buffer.
     */
    Buffer *cur = m_state->buffers.at(m_state->current_buffer);

    /*
     * Current offset
     */
    int row = cur->cy + cur->rowoff;
    int col = cur->cx + cur->coloff;

    /*
     * Deleting from the top of the file is impossible.
     */
    if ((col == 0) && (row == 0))
    {
        return;
    }

    if (col == 0)
    {

        /*
         * The current row, and the previous row.
         */
        erow *p_row = cur->rows.at(row - 1);
        int p_len = p_row->chars->size();
        erow *c_row = cur->rows.at(row);

        /*
         * For each character in the current row, append to the previous.
         */
        for (std::vector<std::wstring>::iterator it = c_row->chars->begin() + col; it != c_row->chars->end(); ++it)
        {
            p_row->chars->push_back(*it);
        }

        /*
         * Now delete the current row.
         */
        cur->rows.erase(cur->rows.begin() + row);
        delete c_row;

        /*
         * Finally we need to move the cursor to the correct location.
         *
         * The correct location is the end of the old line.
         */
        move("up");
        sol_lua(NULL);

        while (p_len)
        {
            move("right");
            p_len--;
        }

        return;
    }

    /*
     * deleting from the middle of a row.
     */
    erow *cur_row = cur->rows.at(row);
    cur_row->chars->erase(cur_row->chars->begin() + col - 1);
    move("left");
}


/**
 * Convert the given key to a human-readable version of it.
 */
const char *Editor::lookup_key(unsigned int c)
{
    if (c == 0)
        return ("^ ");

    if (c == '\n')
        return ("ENTER");

    if (c == 27)
        return ("ESC");

    if (c == '\t')
        return ("TAB");

    if (c == ' ')
        return ("SPACE");

    return (keyname(c));
}


/**
 * Call a lua function, with variadic arguments.
 */
void Editor::call_lua(const char *func, const char *sig, ...)
{
    va_list vl;
    int narg, nres;

    va_start(vl, sig);

    lua_getglobal(m_lua, func);

    if (lua_isnil(m_lua, -1))
        return;

    /* push arguments */
    narg = 0;

    while (*sig)
    {
        /* push arguments */
        switch (*sig++)
        {

        case 'd':  /* double argument */
            lua_pushnumber(m_lua, va_arg(vl, double));
            break;

        case 'i':  /* int argument */
            lua_pushnumber(m_lua, va_arg(vl, int));
            break;

        case 's':  /* string argument */
            lua_pushstring(m_lua, va_arg(vl, char *));
            break;

        case '>':
            goto endwhile;

        default:
        {
            fprintf(stderr, "invalid option (%c)", *(sig - 1));
            return;
        }
        }

        narg++;
    }

endwhile:


    /* do the call */
    nres = strlen(sig);  /* number of expected results */

    if (lua_pcall(m_lua, narg, nres, 0) != 0)   /* do the call */
    {
        fprintf(stderr, "error running function `%s': %s", func, lua_tostring(m_lua, -1));
        return;
    }

    /* retrieve results */
    nres = -nres;  /* stack index of first result */

    while (*sig)
    {
        /* get results */
        switch (*sig++)
        {

        case 'd':  /* double result */
            if (!lua_isnumber(m_lua, nres))
            {

                fprintf(stderr, "wrong result type: expected number");
                return;
            }

            *va_arg(vl, double *) = lua_tonumber(m_lua, nres);
            break;

        case 'i':  /* int result */
            if (!lua_isnumber(m_lua, nres))
            {
                fprintf(stderr, "wrong result type: expected number");
                return;
            }

            *va_arg(vl, int *) = (int)lua_tonumber(m_lua, nres);
            break;

        case 's':  /* string result */
            if (!lua_isstring(m_lua, nres))
            {
                fprintf(stderr, "wrong result type: expected string");
                return;
            }

            *va_arg(vl, const char **) = lua_tostring(m_lua, nres);
            break;

        default:
        {
            fprintf(stderr, "invalid result-type");
            return;
        }
        }

        nres++;
    }

    va_end(vl);

}


/**
 * Move the cursor to the given position, if possible.
 */
void Editor::warp(int x, int y)
{

    if (y < 0)
        y = 0;

    if (x < 0)
        x = 0;

    /*
     * Move to top-left
     */
    Buffer *buffer = current_buffer();
    buffer->cx = buffer->coloff = 0;
    buffer->cy = buffer->rowoff = 0;

    /*
     * Is that where we wanted to go?
     */
    if (x == 0 && y == 0)
        return;

    /*
     * Move down first - that should always work.
     */
    while (y > 0)
    {
        move("down");
        y -= 1;
    }

    /*
     * Now move right.
     */
    while (x > 0)
    {
        move("right");
        x -= 1;
    }
}


/**
 * Move the cursor in the given direction.
 */
void Editor::move(const char *direction)
{
    Editor *e = Editor::instance();
    Buffer *buffer = e->current_buffer();

    int max_row = buffer->rows.size();

    if (strcmp(direction, "up") == 0)
    {
        if (buffer->cy > 0)
            buffer->cy -= 1;
        else if (buffer->rowoff > 0)
            buffer->rowoff -= 1;
    }
    else if (strcmp(direction, "down") == 0)
    {
        if (buffer->cy + buffer->rowoff < max_row - 1)
        {
            buffer->cy += 1;

            if (buffer->cy >= e->height())
            {
                buffer->cy = e->height();
                buffer->rowoff++;
            }
        }
    }
    else if (strcmp(direction, "left") == 0)
    {
        if (buffer->cx == 0)
        {
            if (buffer->coloff > 0)
            {
                buffer->coloff --;
            }
            else
            {
                int filerow = buffer->cy + buffer->rowoff;

                if (filerow > 0)
                {
                    buffer->cy--;

                    erow *row = buffer->rows.at(buffer->cy + buffer->rowoff);

                    if ((int)row->chars->size() < e->width())
                    {
                        buffer->coloff = 0;
                        buffer->cx = row->chars->size() ;
                    }
                    else
                    {
                        buffer->cx = e->width() - 1;
                        buffer->coloff = row->chars->size() - e->width() + 1;
                    }
                }
            }
        }
        else
        {
            buffer->cx -= 1;
        }

    }
    else  if (strcmp(direction, "right") == 0)
    {

        int x = buffer->cx + buffer->coloff;
        int y = buffer->cy + buffer->rowoff;

        erow *row = buffer->rows.at(y);

        if (x < (int)row->chars->size())
        {
            buffer->cx += 1;

            if (buffer->cx >= e->width())
            {
                buffer->cx -= 1;
                buffer->coloff += 1;
            }
        }
        else
        {
            /*
             * Ensure moving right on the last row doesn't work.
             */
            if (y + 1 < max_row)
            {
                buffer->cx = 0;
                buffer->coloff = 0;
                buffer->cy += 1;

                if (buffer->cy >= e->width())
                {
                    buffer->cy -= 1;
                    buffer->rowoff += 1;
                }
            }
        }
    }

    while (buffer->cy + buffer->rowoff >= max_row)
    {
        if (buffer->rowoff)
            buffer->rowoff--;
        else
            buffer->cy--;
    }

    erow *row = buffer->rows.at(buffer->cy + buffer->rowoff);

    if ((int)row->chars->size() < (buffer->cx + buffer->coloff))
    {
        eol_lua(NULL);
    }
}


/*
 * Create a new buffer, and make it active.
 */
void Editor::new_buffer(const char *name)
{
    /*
     * Is this buffer named?
     */
    Buffer *tmp;

    if (name != NULL)
    {
        tmp = new Buffer(name);
    }
    else
    {
        std::string t;
        t = "*Buffer-" + std::to_string(count_buffers() + 1) + "*";
        tmp = new Buffer(t.c_str());
    }

    m_state->buffers.push_back(tmp);
    m_state->current_buffer = count_buffers() - 1;
}

/*
 * Jump to the given buffer.
 */
void Editor::set_current_buffer(int off)
{
    int max = count_buffers();

    if (off >= 0 && off < max)
    {
        m_state->current_buffer = off;
    }
}

/*
 * Get the index of the current buffer.
 */
int Editor::get_current_buffer()
{
    return (m_state->current_buffer);
}

/*
 * Count the buffers.
 */
int Editor::count_buffers()
{
    return (m_state->buffers.size());
}

/**
 * Lookup the index of the buffer with the specified name.
 *
 * Returns -1 on failure.
 */
int Editor::buffer_by_name(const char *name)
{
    int offset = 0;

    for (std::vector<Buffer *>::iterator it = m_state->buffers.begin();
            it != m_state->buffers.end(); ++it)
    {
        Buffer *tmp          = (*it);
        const char *tmp_name = tmp->get_name();

        if (tmp_name && (strcmp(tmp_name, name) == 0))
        {
            return (offset);
        }

        offset += 1;
    }

    return (-1);
}


/**
 * Remove the current buffer.
 *
 * If that means there are no remaining buffers then terminate.
 */
void Editor::kill_current_buffer()
{
    /*
     * Kill the current buffer.
     */
    auto it = m_state->buffers.begin() + m_state->current_buffer;
    delete *it;
    m_state->buffers.erase(it);

    if (count_buffers() < 1)
    {
        endwin();
        exit(1);
    }

    m_state->current_buffer = count_buffers() - 1;
}


/**
 * Return all the buffers.
 */
std::vector<Buffer *> Editor::get_buffers()
{
    return (m_state->buffers);
}

/**
 * Return the height of the edit-area - which is the height of the terminal
 * minus two rows for the footer.
 */
int Editor::height()
{
    return (m_state->screenrows());
}

/**
 * Return the width of the edit-area / terminal
 */
int Editor::width()
{
    return (m_state->screencols());
}


/**
 * Execute the contents of the given file, as lua.
 */
int Editor::load_lua(const char *filename)
{
    if (access(filename, 0) == 0)
    {
        int erred = luaL_dofile(m_lua, filename);

        if (erred)
        {
            endwin();

            if (lua_isstring(m_lua, -1))
                fprintf(stderr, "%s\n", lua_tostring(m_lua, -1));

            fprintf(stderr, "Failed to load %s - aborting\n", filename);
            exit(1);
        }

        return 1;
    }
    else
    {
        set_status(1, "Not loading Lua file %s, it is not present", filename);

    }

    return 0;
}

/**
 * Eval a given string.
 */
int Editor::eval_lua(const char *text)
{

    int erred = luaL_dostring(m_lua, text);

    if (erred)
    {
        if (lua_isstring(m_lua, -1))
            set_status(1, "%s", lua_tostring(m_lua, -1));

        return (0);
    }

    return 1;
}


/**
 * Update the syntax-path
 */
void Editor::set_syntax_path(const char *path)
{
    lua_pushstring(m_lua, path);
    lua_setglobal(m_lua, "syntax_path");
}


/**
 * Get the hostname we're running on.
 */
char *Editor::hostname()
{
    const char *env = getenv("HOSTNAME");

    if (env != NULL)
        return (strdup(env));

    /*
     * Get the short version.
     */
    char res[1024];
    res[sizeof(res) - 1] = '\0';
    gethostname(res, sizeof(res) - 1);

    /*
     * Attempt to get the full vrsion.
     */
    struct hostent *hstnm;
    hstnm = gethostbyname(res);

    if (hstnm)
    {
        /*
         * Success.
         */
        return (strdup(hstnm->h_name));
    }
    else
    {
        /*
         * Failure: Return the short-version.
         */
        return (strdup(res));
    }
}


/**
 * Prompt the user to choose from a series of strings.
 *
 * Return the index of the selected choice, -1 if cancelled.
 */
int Editor::menu(std::vector<std::string> choices)
{
    int selected = 0;
    int offset   = 0;

    int w = width();
    int h = height();

    /*
     * Hide the cursor & clear the screen.
     */
    curs_set(0);
    ::clear();

    while (true)
    {
        int max = choices.size();

        for (int i = 0; i < h ; i++)
        {
            int row = offset + i;

            if (selected == i)
                attron(A_STANDOUT);

            /*
             * Get the choice.
             */
            std::string tmp;

            if (row < max)
                tmp = choices.at(row);
            else
                tmp = "~";

            /*
             * Pad, where appropriate.
             */
            while ((int)tmp.size() < w)
                tmp += " ";

            mvwaddstr(stdscr, i, 0, tmp.c_str());

            if (selected == i)
                attroff(A_STANDOUT);
        }

        /*
         * poll for input.
         */
        unsigned int ch;

        int res = get_wch(&ch);

        if (res == ERR)
            continue;

        if (ch == '\n')
        {
            curs_set(1);
            return (offset + selected);
        }

        if (ch == 27)
        {
            curs_set(1);
            return (-1);
        }

        if (ch == KEY_HOME)
        {
            // start
            selected = 0;
            offset = 0;
        }

        if (ch == KEY_END)
        {
            // end
            selected = 0;
            offset = max - 1;
        }

        if ((ch == KEY_UP) || (ch == KEY_PPAGE))
        {
            int times = (ch == KEY_PPAGE) ? h - 2 : 1;

            for (int i = 0; i < times; i++)
            {
                if (selected > 0)
                    selected -= 1;
                else if (offset > 0)
                    offset -= 1;
            }
        }

        /*
         * TAB moves down, because this menu-code is used in
         * TAB-completion and that saves a hand-movement.
         */
        if ((ch == KEY_DOWN) || (ch == KEY_NPAGE) || (ch == '\t'))
        {
            int times = (ch == KEY_NPAGE) ? h - 2 : 1;

            for (int i = 0; i < times; i++)
            {
                if (selected + offset < max - 1)
                    selected += 1;

                if (selected >= h)
                {
                    selected = h - 1;

                    if (selected + offset < max - 1)
                        offset += 1;
                }
            }
        }

    }

    /*
     * Never reached.
     */
    return 0;

}

/**
 * Get the selected text.
 */
std::wstring Editor::get_selection()
{
    std::wstring result;

    /*
     * The current buffer, and row-count.
     */
    Buffer *cur = m_state->buffers.at(m_state->current_buffer);

    /*
     * If there is no mark - return early.
     */
    if ((cur->markx == -1) && (cur->marky == -1))
        return result;

    /*
     * Count the rows.
     */
    int rows = cur->rows.size();

    /*
     * The position of the point and mark.
     */
    int m_pos = cur->pos2offset(cur->markx, cur->marky);
    int c_pos = cur->pos2offset(cur->cx + cur->coloff,  cur->cy + cur->rowoff);

    /*
     * The character offsets - the characters between these two offsets
     * into the buffer are the selection.
     */
    int sel_min = std::min(m_pos, c_pos);
    int sel_max = std::max(m_pos, c_pos);

    /*
     * Now build up the selection.
     */
    int count = 0;

    for (int y = 0; y < rows; y++)
    {
        erow *row = cur->rows.at(y);
        int row_size = row->chars->size();

        /*
         * NOTE: We add one character to the row
         * to cope with the trailing newline.
         */
        for (int x = 0; x < row_size + 1; x++)
        {
            if (count >= sel_min && count <= sel_max)
            {
                if (x < row_size)
                    result += row->chars->at(x);
                else
                    result += '\n';
            }

            count += 1;
        }
    }

    return (result);
}
