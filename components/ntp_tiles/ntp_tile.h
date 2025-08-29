// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_NTP_TILE_H_
#define COMPONENTS_NTP_TILES_NTP_TILE_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/ntp_tiles/tile_source.h"
#include "components/ntp_tiles/tile_title_source.h"
#include "url/gurl.h"

namespace ntp_tiles {

// A suggested site shown on the New Tab Page. This is equivalent to "shortcuts"
// which are the user facing name.
struct NTPTile {
  std::u16string title;
  GURL url;
  TileTitleSource title_source;
  TileSource source;

  // This won't be empty, but might 404 etc.
  GURL favicon_url;

  // Timestamp representing when the tile was originally generated (produced by
  // a ranking algorithm).
  base::Time data_generation_time;

  // True if this tile is a custom link and was initialized from a Most Visited
  // item. Used for debugging.
  bool from_most_visited = false;

  // The visit count of a Most Visited item. Used for debugging.
  int visit_count = 0;

  // The last visit time of a Most Visited item. Used for debugging.
  base::Time last_visit_time;

  // The score of a Most Visited item. Used for tweaking algorithm.
  double score = -1;

#if !BUILDFLAG(IS_ANDROID)
  // Whether to allow users to edit the tile in the action menu. Does not apply
  // to top sites. May be false for enterprise shortcuts.
  bool allow_user_edit = true;

  // Whether to allow users to delete the tile in the action menu. Does not
  // apply to top sites. May be false for enterprise shortcuts.
  bool allow_user_delete = true;
#endif  // !BUILDFLAG(IS_ANDROID)

  NTPTile();
  NTPTile(const NTPTile&);
  ~NTPTile();
};

bool operator==(const NTPTile& a, const NTPTile& b);

using NTPTilesVector = std::vector<NTPTile>;

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_NTP_TILE_H_
