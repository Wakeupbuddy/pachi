#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"


struct board *
board_init(void)
{
	return calloc(1, sizeof(struct board));
}

struct board *
board_copy(struct board *b1)
{
	struct board *b2 = board_init();
	memcpy(b2, b1, sizeof(struct board));
	b2->b = calloc(b2->size * b2->size, sizeof(*b2->b));
	b2->g = calloc(b2->size * b2->size, sizeof(*b2->g));
	memcpy(b2->b, b1->b, b2->size * b2->size * sizeof(*b2->b));
	memcpy(b2->g, b1->g, b2->size * b2->size * sizeof(*b2->g));
	return b2;
}

void
board_done(struct board *board)
{
	if (board->b) free(board->b);
	if (board->g) free(board->g);
	free(board);
}

void
board_resize(struct board *board, int size)
{
	board->size = size;
	board->b = realloc(board->b, board->size * board->size * sizeof(*board->b));
	board->g = realloc(board->g, board->size * board->size * sizeof(*board->g));
}

void
board_clear(struct board *board)
{
	memset(board->b, 0, board->size * board->size * sizeof(*board->b));
	memset(board->g, 0, board->size * board->size * sizeof(*board->g));
	board->captures[S_BLACK] = board->captures[S_WHITE] = 0;
	board->moves = 0;
}


void
board_print(struct board *board, FILE *f)
{
	fprintf(f, "Move: % 3d  Komi: %2.1f  Captures B: %d W: %d\n     ",
		board->moves, board->komi,
		board->captures[S_BLACK], board->captures[S_WHITE]);
	int x, y;
	char asdf[] = "ABCDEFGHJKLMNOPQRSTUVWXYZ";
	for (x = 0; x < board->size; x++)
		fprintf(f, "%c ", asdf[x]);
	fprintf(f, "\n   +-");
	for (x = 0; x < board->size; x++)
		fprintf(f, "--");
	fprintf(f, "+\n");
	for (y = board->size - 1; y >= 0; y--) {
		fprintf(f, "%2d | ", y + 1);
		for (x = 0; x < board->size; x++)
			fprintf(f, "%c ", stone2char(board_atxy(board, x, y)));
		fprintf(f, "|\n");
	}
	fprintf(f, "   +-");
	for (x = 0; x < board->size; x++)
		fprintf(f, "--");
	fprintf(f, "+\n\n");
}


#define foreach_point(board_) \
	do { \
		int x, y; \
		for (x = 0; x < board_->size; x++) { \
			for (y = 0; y < board_->size; y++) { \
				struct coord c = { x, y }; c = c; /* shut up gcc */
#define foreach_point_end \
			} \
		} \
	} while (0)

#define foreach_in_group(board_, group_) \
	do { \
		int group__ = group_; \
		foreach_point(board_) \
			if (group_at(board_, c) == group__)
#define foreach_in_group_end \
		foreach_point_end; \
	} while (0)

#define foreach_neighbor(board_, coord_) \
	do { \
		struct coord q__[] = { { (coord_).x - 1, (coord_).y }, \
		                       { (coord_).x, (coord_).y - 1 }, \
		                       { (coord_).x + 1, (coord_).y }, \
		                       { (coord_).x, (coord_).y + 1 } }; \
		int fn__i; \
		for (fn__i = 0; fn__i < 4; fn__i++) { \
			int x = q__[fn__i].x, y = q__[fn__i].y; struct coord c = { x, y }; \
			if (x < 0 || y < 0 || x >= board_->size || y >= board->size) \
				continue;
#define foreach_neighbor_end \
		} \
	} while (0)


static int
board_play_nocheck(struct board *board, struct move *m)
{
	int gid = 0;

	board_at(board, m->coord) = m->color;

	foreach_neighbor(board, m->coord) {
		if (board_at(board, c) == m->color && group_at(board, c) != gid) {
			if (gid <= 0) {
				gid = group_at(board, c);
			} else {
				/* Merge groups */
				foreach_in_group(board, group_at(board, c)) {
					group_at(board, c) = gid;
				} foreach_in_group_end;
			}
		} else if (board_at(board, c) == stone_other(m->color)
			   && board_group_libs(board, group_at(board, c)) == 0) {
			board_group_capture(board, group_at(board, c));
		}
	} foreach_neighbor_end;

	if (gid <= 0)
		gid = ++board->last_gid;
	group_at(board, m->coord) = gid;

	board->moves++;

	return gid;
}

int
board_play(struct board *board, struct move *m)
{
	if (!board_valid_move(board, m, false))
		return 0;
	return board_play_nocheck(board, m);
}

bool
board_no_valid_moves(struct board *board, enum stone color)
{
	foreach_point(board) {
		struct move m;
		m.coord.x = x; m.coord.y = y; m.color = color;
		/* Self-atari doesn't count. :-) */
		if (board_valid_move(board, &m, true))
			return false;
	} foreach_point_end;
	return true;
}

bool
board_valid_move(struct board *board, struct move *m, bool sensible)
{
	struct board *b2;

	if (board_at(board, m->coord) != S_NONE)
		return false;

	/* Try it! */
	b2 = board_copy(board);
	board_play_nocheck(b2, m);
	if (board_group_libs(b2, group_at(b2, m->coord)) <= sensible) {
		/* oops, suicide (or self-atari if sensible) */
		board_done(b2);
		return false;
	}

	board_done(b2);
	return true;
}


int
board_local_libs(struct board *board, struct coord *coord)
{
	int l = 0;

	foreach_neighbor(board, *coord) {
		if (board->libcount_watermark) {
			/* If we get called in loop, our caller can prevent us
			 * from counting some liberties multiple times. */
			if (board->libcount_watermark[c.x + board->size * c.y])
				continue;
			board->libcount_watermark[c.x + board->size * c.y] = true;
		}
		l += (board_at(board, c) == S_NONE);
	} foreach_neighbor_end;
	return l;
}


int
board_group_libs(struct board *board, int group)
{
	int l = 0;
	bool watermarks[board->size * board->size];
	memset(watermarks, 0, sizeof(watermarks));

	board->libcount_watermark = watermarks;

	foreach_in_group(board, group) {
		l += board_local_libs(board, &c);
	} foreach_in_group_end;

	board->libcount_watermark = NULL;
	return l;
}

void
board_group_capture(struct board *board, int group)
{
	foreach_in_group(board, group) {
		board->captures[stone_other(board_at(board, c))]++;
		board_at(board, c) = S_NONE;
		group_at(board, c) = 0;
	} foreach_in_group_end;
}
