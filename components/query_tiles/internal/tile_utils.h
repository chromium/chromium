// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_UTILS_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_UTILS_H_

#include <map>
#include <memory>
#include <vector>

#include "components/query_tiles/tile.h"

namespace query_tiles {

// Function to sort a vector of tiles based on their score in |tile_stats|. If
// a tile ID doesn't exists in |tile_stats|, a new entry will be created and
// a score will be calculated.
void SortTiles(std::vector<std::unique_ptr<Tile>>* tiles,
               std::map<std::string, TileStats>* tile_stats);

// Calculates the current tile score based on |current_time|. Tile score will
// decay over time.
double CalculateTileScore(const TileStats& tile_stats, base::Time current_time);

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_UTILS_H_
