// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_METRICS_H_
#define COMPONENTS_NTP_TILES_METRICS_H_

#include "components/ntp_tiles/deleted_tile_type.h"
#include "components/ntp_tiles/ntp_tile_impression.h"

namespace ntp_tiles {
namespace metrics {

// Records an NTP impression, after all tiles have loaded.
void RecordPageImpression(int number_of_tiles);

// Records an individual tile impression, which should be called only after the
// visual type of the tile has been determined.
void RecordTileImpression(const NTPTileImpression& impression);

// Records a click on a tile.
void RecordTileClick(const NTPTileImpression& impression);

// Records when a default app tile is deleted with the type of tile.
void RecordsMigratedDefaultAppDeleted(
    const DeletedTileType& most_visited_app_type);

}  // namespace metrics
}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_METRICS_H_
