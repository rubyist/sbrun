/* *****************************************************************************
 * File:           sbrun.c
 *
 * Purpose:        It's a run minimal dialog with history and completion.
 *                 This is the code for it, if you hadn't surmised.
 *
 * Author:         Scott Barron 
 *
 * Requires:       Gtk+2 and everything it needs.
 *
 * Install:        gcc -o sbrun `pkg-config --cflags --libs gtk+-2.0` sbrun.c
 *                 If you have trouble with this just install GNOME or KDE
 *                 or something, this isn't for you.
 *
 * License:        Yeah, whatever.  If you're so pathetic you'd have to steal
 *                 such simple code, steal it.  You're probably going to hell
 *                 anyway.  If you're not pathetic and you just want a run
 *                 dialog that doesn't blow, use this in whatever way you see
 *                 fit.  You won't be going to hell.
 * ****************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

static int complete_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);

static void activate_cb(GtkWidget *widget, gpointer data);

void murder_string(gpointer data, gpointer u_data);

typedef struct things {
	GIOChannel   *f;
	GList        *commands;
} things;

int main(int argc, char *argv[])
{
	GtkWidget    *window;
	GtkWidget    *entry;
	GCompletion  *completer;
	gchar        *filename;
	gchar        *buf;
	gsize         rlen;
	things       *stuff;

	stuff = g_new0(things, 1);

	stuff->commands  = NULL;
	completer = NULL;

	filename = g_build_filename(g_get_home_dir(), ".sbrun", NULL);
	if (!(stuff->f = g_io_channel_new_file(filename, "a+", NULL))) {
		g_print("Could not open file '%s': %s\n", filename, 
				strerror(errno));
		stuff->f = NULL;
	}

	if (stuff->f) {
		while (g_io_channel_read_line(stuff->f, &buf, &rlen, NULL, NULL) == G_IO_STATUS_NORMAL) {
			g_strstrip(buf);
			stuff->commands = g_list_append(stuff->commands, buf);
		}
		if (stuff->commands) {
			completer = g_completion_new(NULL);
			g_completion_add_items(completer, stuff->commands);
		}
	}

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

	entry  = gtk_entry_new();
	if (completer) {
		g_signal_connect(G_OBJECT(entry), "key-press-event", 
				G_CALLBACK(complete_cb), completer);
	}
	g_signal_connect(G_OBJECT(entry), "activate", 
			G_CALLBACK(activate_cb), stuff);

	gtk_container_add(GTK_CONTAINER(window), entry);

	gtk_widget_show_all(window);

	gtk_main();

	if (stuff->commands) {
		g_list_foreach(stuff->commands, murder_string, NULL);
		g_list_free(stuff->commands);
	}
	
	if (stuff->f) {
		g_io_channel_unref(stuff->f);
	}

	if (completer) {
		g_completion_free(completer);
	}
	g_free(stuff);

	return 0;
}

static int
complete_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	GCompletion *completer;
	GList *completions;
	gchar *prefix;
	gchar *new_prefix;
	gint pos, start, end, len;

	completer = data;

	if (event->keyval == GDK_Escape) {
		gtk_main_quit();
	}

	if (!g_ascii_isalnum(event->string[0]) && !g_ascii_isprint(event->string[0])) {
		return FALSE;
	}

	pos = gtk_editable_get_position(GTK_EDITABLE(widget));
	gtk_editable_get_selection_bounds(GTK_EDITABLE(widget), &start, &end);
	len = strlen(gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1));

	if (len == 0) {
		prefix = g_strdup(event->string);
	} else {
		if (end - start <= 0) {
			start = -1;
		}
		prefix = g_strdup_printf("%s%s",
				gtk_editable_get_chars(GTK_EDITABLE(widget), 0, start),
				event->string);
	}

	completions = g_completion_complete(completer, prefix, &new_prefix);
	if (completions) {
		gtk_entry_set_text(GTK_ENTRY(widget), completions->data);
		gtk_editable_set_position(GTK_EDITABLE(widget), strlen(prefix));
		gtk_editable_select_region(GTK_EDITABLE(widget), strlen(prefix), -1);
		g_free(new_prefix);
		return TRUE;
	}
	g_free(prefix);
	g_free(new_prefix);
	return FALSE;
}

gint stupid_strcmp(gconstpointer s1, gconstpointer s2) {
	return g_ascii_strcasecmp((gchar*)s1, (gchar*)s2);
}

static void
activate_cb(GtkWidget *widget, gpointer data)
{
	gchar    *command;
	gchar  **args;
	gsize    wlen;
	things  *stuff;

	stuff = data;

	command = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
	g_strstrip(command);
	if (g_ascii_strcasecmp(command, "") == 0) {
		gtk_main_quit();
		return;
	}

	args = g_strsplit(command, " ", 0);

	if (g_spawn_async(NULL, args, NULL, G_SPAWN_SEARCH_PATH, NULL,
			NULL, NULL, NULL) == TRUE) {
		if (!g_list_find_custom(stuff->commands, command, stupid_strcmp)) {
			g_io_channel_write_chars(stuff->f, command, -1, &wlen, NULL);
			g_io_channel_write_chars(stuff->f, "\n", -1, &wlen, NULL);
			g_io_channel_flush(stuff->f, NULL);
		}
	}

	g_strfreev(args);

	gtk_main_quit();
}

void murder_string(gpointer data, gpointer u_data)
{
	g_free(data);
}
