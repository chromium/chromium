// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_METRICS_H_
#define COMPONENTS_NTP_TILES_METRICS_H_

#include <string_view>

#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/ntp_tiles/tile_type.h"

namespace ntp_tiles {
namespace metrics {

// Records a page impression, after all tiles have loaded.
void RecordPageImpression(int number_of_tiles,
                          std::string_view prefix = "NewTabPage");

// Records the number of Custom Tiles, to be called when the page loads for the
// first time and Custom Tiles are enabled.
void RecordNumberOfCustomTilesOnFirstNtp(
    int number_of_custom_tiles,
    std::string_view prefix = "NewTabPage");

// Records an individual tile impression, which should be called only after the
// visual type of the tile has been determined.
void RecordTileImpression(const NTPTileImpression& impression,
                          std::string_view prefix = "NewTabPage");

// Records a click on a tile.
void RecordTileClick(const NTPTileImpression& impression,
                     std::string_view prefix = "NewTabPage");

// Records when a default app tile is deleted with the type of tile.
void RecordsMigratedDefaultAppDeleted(const TileType& most_visited_app_type);

}  // namespace metrics
}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_METRICS_H_
