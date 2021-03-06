/*
 * netsniff-ng - the packet sniffing beast
 * Subject to the GPL, version 2.
 */

#include <curses.h>

#include "ui.h"
#include "str.h"
#include "xmalloc.h"

static struct ui_text *ui_text_alloc(size_t len)
{
	struct ui_text *text = xzmalloc(sizeof(*text));

	text->str = xzmalloc(sizeof(chtype) * len + 1);
	text->len = len;

	return text;
}

static void ui_text_len_set(struct ui_text *text, size_t len)
{
	if (text->len == len)
		return;

	if (text->slen + len > text->len) {
		text->str = xrealloc(text->str, sizeof(chtype) * len + 1);
		text->len = len;
	}

	text->slen = min(len, text->slen);
	text->str[text->slen] = 0;
}

static void ui_text_attr_insert(struct ui_text *text, int idx, int attr, const char *str)
{
	size_t slen = strlen(str);
	uint32_t i, j;

	if (idx + slen > text->len)
		ui_text_len_set(text, idx + slen);

	for (j = 0, i = idx; i < idx + slen; i++, j++)
		text->str[i] = str[j] | attr;
}

static void ui_text_free(struct ui_text *text)
{
	xfree(text->str);
	xfree(text);
}

void ui_table_init(struct ui_table *tbl)
{
	memset(tbl, 0, sizeof(*tbl));

	getsyx(tbl->y, tbl->x);

	tbl->rows_y  = tbl->y;
	tbl->width   = COLS;
	tbl->height  = LINES - 2;
	tbl->col_pad = 1;
	tbl->row     = ui_text_alloc(tbl->width);

	INIT_LIST_HEAD(&tbl->cols);
}

void ui_table_uninit(struct ui_table *tbl)
{
	struct ui_col *col, *tmp;

	list_for_each_entry_safe(col, tmp, &tbl->cols, entry)
		xfree(col);

	ui_text_free(tbl->row);
}

void ui_table_pos_set(struct ui_table *tbl, int y, int x)
{
	tbl->y      = y;
	tbl->x      = x;
	tbl->rows_y = y;
}

static struct ui_col *ui_table_col_get(struct ui_table *tbl, uint32_t id)
{
	struct ui_col *col;

	list_for_each_entry(col, &tbl->cols, entry) {
		if (col->id == id)
			return col;
	}

	bug();
}

static void __ui_table_pos_update(struct ui_table *tbl)
{
	struct ui_col *col;
	uint32_t pos = 0;

	list_for_each_entry(col, &tbl->cols, entry) {
		col->pos  = pos;
		pos      += col->len + tbl->col_pad;
	}
}

void ui_table_col_add(struct ui_table *tbl, uint32_t id, char *name, uint32_t len)
{
	struct ui_col *col = xzmalloc(sizeof(*col));

	col->id    = id;
	col->name  = name;
	col->len   = len;
	col->align = UI_ALIGN_LEFT;

	list_add_tail(&col->entry, &tbl->cols);

	__ui_table_pos_update(tbl);
}

void ui_table_col_color_set(struct ui_table *tbl, int col_id, int color)
{
	struct ui_col *col = ui_table_col_get(tbl, col_id);

	col->color = color;
}

void ui_table_col_align_set(struct ui_table *tbl, int col_id, enum ui_align align)
{
	struct ui_col *col = ui_table_col_get(tbl, col_id);

	col->align = align;
}

void ui_table_row_add(struct ui_table *tbl)
{
	tbl->rows_y++;
}

void ui_table_clear(struct ui_table *tbl)
{
	int y;

	tbl->rows_y = tbl->y;

	for (y = tbl->y + 1; y < tbl->y + tbl->height; y++) {
		mvprintw(y, tbl->x, "%*s", tbl->width, " ");
	}
}

#define UI_ALIGN_COL(col) (((col)->align == UI_ALIGN_LEFT) ? "%-*.*s" : "%*.*s")

void ui_table_row_show(struct ui_table *tbl)
{
	mvaddchstr(tbl->rows_y, tbl->x, tbl->row->str + tbl->scroll_x);
	ui_text_len_set(tbl->row, 0);
}

static void __ui_table_row_print(struct ui_table *tbl, struct ui_col *col,
				 int color, const char *str)
{
	char tmp[128];

	slprintf(tmp, sizeof(tmp), UI_ALIGN_COL(col), col->len, col->len, str);
	ui_text_attr_insert(tbl->row, col->pos, color, tmp);

	slprintf(tmp, sizeof(tmp), "%*s", tbl->col_pad, " ");
	ui_text_attr_insert(tbl->row, col->pos + col->len, color, tmp);
}

void ui_table_row_col_set(struct ui_table *tbl, uint32_t col_id, const char *str)
{
	struct ui_col *col = ui_table_col_get(tbl, col_id);

	__ui_table_row_print(tbl, col, col->color, str);
}

void ui_table_header_color_set(struct ui_table *tbl, int color)
{
	tbl->hdr_color = color;
}

void ui_table_height_set(struct ui_table *tbl, int height)
{
	tbl->height = height;
}

void ui_table_header_print(struct ui_table *tbl)
{
	struct ui_col *col;

	attron(tbl->hdr_color);
	mvprintw(tbl->y, tbl->x, "%*s", tbl->width, " ");
	attroff(tbl->hdr_color);

	list_for_each_entry(col, &tbl->cols, entry) {
		__ui_table_row_print(tbl, col, tbl->hdr_color, col->name);

	}

	ui_table_row_show(tbl);
}

#define SCROLL_X_STEP 10

void ui_table_event_send(struct ui_table *tbl, enum ui_event_id evt_id)
{
	if (evt_id == UI_EVT_SCROLL_RIGHT) {
		tbl->scroll_x += SCROLL_X_STEP;
	} else if (evt_id == UI_EVT_SCROLL_LEFT) {
		tbl->scroll_x -= SCROLL_X_STEP;
		if (tbl->scroll_x < 0)
			tbl->scroll_x = 0;
	}
}
