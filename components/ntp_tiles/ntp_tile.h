// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_NTP_TILE_H_
#define COMPONENTS_NTP_TILES_NTP_TILE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/ntp_tiles/tile_source.h"
#include "components/ntp_tiles/tile_title_source.h"
#include "url/gurl.h"

namespace ntp_tiles {

// A suggested site shown on the New Tab Page.
struct NTPTile {
  std::u16string title;
  GURL url;
  TileTitleSource title_source;
  TileSource source;

  // Empty unless allowlists are enabled and this site is in an allowlist.
  // However, may be non-empty even if |source| is not |ALLOWLIST|, if this tile
  // is also available from another, higher-priority source.
  base::FilePath allowlist_icon_path;

  // This won't be empty, but might 404 etc.
  GURL favicon_url;

  // Timestamp representing when the tile was originally generated (produced by
  // a ranking algorithm).
  base::Time data_generation_time;

  // True if this tile is a custom link and was initialized from a Most Visited
  // item. Used for debugging.
  bool from_most_visited = false;

  NTPTile();
  NTPTile(const NTPTile&);
  ~NTPTile();
};

bool operator==(const NTPTile& a, const NTPTile& b);
bool operator!=(const NTPTile& a, const NTPTile& b);

using NTPTilesVector = std::vector<NTPTile>;

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_NTP_TILE_H_
