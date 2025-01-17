#include "ibm.h"
#include "disc.h"
#include "fdc.h"
#include "fdd.h"

static struct {
	int type;

	int track;

	int densel;

	int drate;

	int kbps;
	int fdc_kbps;
} fdd[2];

/* Flags:
   Bit 0:	300 rpm supported;
   Bit 1:	360 rpm supported;
   Bit 2:	size (0 = 3.5", 1 = 5.25");
   Bit 3:	double density supported;
   Bit 4:	high density supported;
   Bit 5:	extended density supported;
   Bit 6:	double step for 40-track media;
*/
#define FLAG_RPM_300                1
#define FLAG_RPM_360                2
#define FLAG_525                4
#define FLAG_HOLE0                8
#define FLAG_HOLE1                16
#define FLAG_HOLE2                32
#define FLAG_DOUBLE_STEP        64

static struct {
	int max_track;
	int flags;
} drive_types[] =
	{
		{       /*None*/
			.max_track = 0,
			.flags = 0
		},
		{       /*5.25" DD*/
			.max_track = 41,
			.flags = FLAG_RPM_300 | FLAG_525 | FLAG_HOLE0
		},
		{       /*5.25" HD*/
			.max_track = 82,
			.flags = FLAG_RPM_360 | FLAG_525 | FLAG_HOLE0 | FLAG_HOLE1 | FLAG_DOUBLE_STEP
		},
		{       /*5.25" HD Dual RPM*/
			.max_track = 82,
			.flags = FLAG_RPM_300 | FLAG_RPM_360 | FLAG_525 | FLAG_HOLE0 | FLAG_HOLE1 | FLAG_DOUBLE_STEP
		},
		{       /*3.5" DD*/
			.max_track = 82,
			.flags = FLAG_RPM_300 | FLAG_HOLE0
		},
		{       /*3.5" HD*/
			.max_track = 82,
			.flags = FLAG_RPM_300 | FLAG_HOLE0 | FLAG_HOLE1
		},
		{       /*3.5" HD 3-Mode*/
			.max_track = 82,
			.flags = FLAG_RPM_300 | FLAG_RPM_360 | FLAG_HOLE0 | FLAG_HOLE1
		},
		{       /*3.5" ED*/
			.max_track = 82,
			.flags = FLAG_RPM_300 | FLAG_HOLE0 | FLAG_HOLE1 | FLAG_HOLE2
		}
	};

int fdd_swap = 0;

uint64_t fdd_seek(int drive, int track_diff) {
	drive ^= fdd_swap;

	if (drive >= 2)
		return 1000 * TIMER_USEC;

	fdd[drive].track += track_diff;

	if (fdd[drive].track < 0)
		fdd[drive].track = 0;

	if (fdd[drive].track > drive_types[fdd[drive].type].max_track)
		fdd[drive].track = drive_types[fdd[drive].type].max_track;

//        pclog("fdd_seek: drive=%i track_diff=%i old_track=%i track=%i\n", drive, track_diff, old_track, fdd[drive].track);

	fdc_discchange_clear(drive);

	disc_seek(drive, fdd[drive].track);
	return 1000 * TIMER_USEC;
}

void fdd_disc_changed(int drive) {
	drive ^= fdd_swap;

	/*Force reload of current track data*/
	if (drive < 2)
		disc_seek(drive, fdd[drive].track);
}

int fdd_track0(int drive) {
	drive ^= fdd_swap;

	if (drive >= 2)
		return 0;

	/* If drive is disabled, TRK0 never gets set. */
	if (!drive_types[fdd[drive].type].max_track)
		return 0;

	return !fdd[drive].track;
}

void fdd_set_densel(int densel) {
	fdd[0].densel = densel;
	fdd[1].densel = densel;
}

int fdd_getrpm(int drive) {
	int hole;

	drive ^= fdd_swap;

	if (drive >= 2)
		return 0;

	hole = disc_hole(drive);

	if (!(drive_types[fdd[drive].type].flags & FLAG_RPM_360))
		return 300;
	if (!(drive_types[fdd[drive].type].flags & FLAG_RPM_300))
		return 360;

	if (drive_types[fdd[drive].type].flags & FLAG_525) {
		return fdd[drive].densel ? 360 : 300;
	} else {
		/* disc_hole(drive) returns 0 for double density media, 1 for high density, and 2 for extended density. */
		if (hole == 1) {
			return fdd[drive].densel ? 300 : 360;
		} else {
			return 300;
		}
	}

	switch (fdd[drive].type) {
	case 0:return 300;
	case 1:return 360;
	}
}

void fdd_setswap(int swap) {
	fdd_swap = swap ? 1 : 0;
}

int fdd_can_read_medium(int drive) {
	int hole;

	drive ^= fdd_swap;

	if (drive >= 2)
		return 0;

	hole = disc_hole(drive);

	hole = 1 << (hole + 3);

//	pclog("Drive %02X, type %02X, hole flag %02X, flags %02X, result %02X\n", drive, fdd[drive].type, hole, drive_types[fdd[drive].type].flags, drive_types[fdd[drive].type].flags & hole);
	return (drive_types[fdd[drive].type].flags & hole) ? 1 : 0;
}

int fdd_doublestep_40(int drive) {
	if (drive >= 2)
		return 0;
	return drive_types[fdd[drive].type].flags & FLAG_DOUBLE_STEP;
}

void fdd_set_type(int drive, int type) {
	if (drive < 2)
		fdd[drive].type = type;
}

int fdd_get_type(int drive) {
	if (drive >= 2)
		return 0;
	return fdd[drive].type;
}

int fdd_is_525(int drive) {
	if (drive >= 2)
		return 0;
	return drive_types[fdd[drive].type].flags & FLAG_525;
}

int fdd_is_ed(int drive) {
	if (drive >= 2)
		return 0;
	return drive_types[fdd[drive].type].flags & FLAG_HOLE2;
}
