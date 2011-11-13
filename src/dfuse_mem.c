/* Helper functions for reading the memory map in a device
 * following the ST DfuSe 1.1a specification.
 *
 * (C) 2011 Tormod Volden <debian.tormod@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "dfuse_mem.h"

extern int verbose;

int add_segment(struct memsegment **segment_list, struct memsegment segment)
{
	struct memsegment *new_element;

	new_element = malloc(sizeof(struct memsegment));
	if (!new_element)
		return -ENOMEM;
	*new_element = segment;
	new_element->next = NULL;

	if (*segment_list == NULL)
		/* list can be empty on first call */
		*segment_list = new_element;
	else {
		struct memsegment *next_element;

		/* find last element in list */
		next_element = *segment_list;
		while (next_element->next != NULL)
			next_element = next_element->next;
		next_element->next = new_element;
	}
	return 0;
}

struct memsegment *find_segment(struct memsegment *segment_list,
				unsigned int address)
{
	while (segment_list != NULL) {
		if (segment_list->start <= address &&
		    segment_list->end >= address)
			return segment_list;
		segment_list = segment_list->next;
	}
	return NULL;
}

void free_segment_list(struct memsegment *segment_list)
{
	struct memsegment *next_element;

	while (segment_list->next != NULL) {
		next_element = segment_list->next;
		free(segment_list);
		segment_list = next_element;
	}
	free(segment_list);
}

struct memsegment *parse_memory_layout(char *intf_desc)
{

	char multiplier, memtype;
	unsigned int address;
	int sectors, size;
	char *name, *typestring;
	int ret;
	int count = 0;
	char separator;
	int scanned;
	struct memsegment *segment_list = NULL;
	struct memsegment segment;

#ifdef DEBUG_DRY
	intf_desc = "@fake /0x08000000/12*001Ka,11*001Kg,9*2Ka,24*4Kg";
#endif
	name = malloc(strlen(intf_desc));
	if (!name) {
		fprintf(stderr, "Error: Cannot allocate memory\n");
		exit(1);
	}
	ret = sscanf(intf_desc, "@%[^/]%n", name, &scanned);
	if (ret < 1) {
		fprintf(stderr, "Error: Could not read name, sscanf returned"
			"%d\n", ret);
		free(name);
		return NULL;
	}
	printf("DfuSe interface name: \"%s\"\n", name);
	free(name);

	intf_desc += scanned;
	typestring = malloc(strlen(intf_desc));
	if (!typestring) {
		fprintf(stderr, "Error: Cannot allocate memory\n");
		exit(1);
	}
	while (ret = sscanf(intf_desc, "/0x%x/%n", &address, &scanned),
	       ret > 0) {

		intf_desc += scanned;
		while (ret = sscanf(intf_desc, "%d*%d%c%[^,/]%n",
				    &sectors, &size, &multiplier, typestring,
				    &scanned), ret > 2) {
			intf_desc += scanned;

			count++;
			memtype = 0;
			if (ret == 4) {
				if (strlen(typestring) == 1
				    && typestring[0] != '/')
					memtype = typestring[0];
				else {
					fprintf(stderr,
						"Parsing type identifier '%s' "
						"failed for segment %i\n",
						typestring, count);
					continue;
				}
			}

			switch (multiplier) {
			case 'B':
				break;
			case 'K':
				size *= 1024;
				break;
			case 'M':
				size *= 1024 * 1024;
				break;
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
			case 'g':
				if (!memtype) {
					fprintf(stderr,
						"Non-valid multiplier '%c', "
						"interpreted as type "
						"identifier instead\n",
						multiplier);
					memtype = multiplier;
					break;
				}
				/* fallthrough if memtype was already set */
			default:
				fprintf(stderr,
					"Non-valid multiplier '%c', "
					"assuming bytes\n", multiplier);
			}

			if (!memtype) {
				fprintf(stderr,
					"No valid type for segment %d\n\n",
					count);
				continue;
			}

			segment.start = address;
			segment.end = address + sectors * size - 1;
			segment.pagesize = size;
			segment.memtype = memtype & 7;
			add_segment(&segment_list, segment);

			if (verbose)
				printf("Memory segment at 0x%08x %3d x %4d = "
				       "%5d (%s%s%s)\n",
				       address, sectors, size, sectors * size,
				       memtype & DFUSE_READABLE  ? "r" : "",
				       memtype & DFUSE_ERASABLE  ? "e" : "",
				       memtype & DFUSE_WRITEABLE ? "w" : "");

			address += sectors * size;

			separator = *intf_desc;
			if (separator == ',')
				intf_desc += 1;
			else
				break;
		}	/* while per segment */

	}		/* while per address */
	free(typestring);

	return segment_list;
}