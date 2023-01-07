// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_UTILS_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_UTILS_H_

#include <map>
#include <memory>
#include <vector>

#include "components/query_tiles/tile.h"

namespace query_tiles {

// Helper class to shuffle a vector of tiles beginning at the |start| position.
class TileShuffler {
 public:
  TileShuffler() = default;
  TileShuffler(const TileShuffler& other) = delete;
  TileShuffler& operator=(const TileShuffler& other) = delete;

  virtual void Shuffle(std::vector<Tile>* tiles, int start) const;
};

// Function to sort a vector of tiles based on their score in |tile_stats|. If
// a tile ID doesn't exists in |tile_stats|, a new entry will be created and
// a score will be calculated. If a tile ID in |tile_stats| doesn't show up in
// |tiles|, it will be removed if the tile isn't clicked recently.
// To calculate scores for new tiles, ordering from the server response will
// be taken into consideration. As the server has already ordered tiles
// according to their importance.
// For example, if the first tile returned by server never appeared before, we
// should set its score to at least the 2nd tile. so that it can show up in
// the first place if no other tiles in the back have a higher score. For
// a new tile at position x, its score should be the minimum of its neighbors
// at position x-1 and x+1. For new tiles showing up at the end, their score
// will be set to 0.
// For example, if the tile scores are (new_tile, 0.5, 0.7), then the adjusted
// score will be (0.5, 0.5, 0.7). Simularly, (0.5, new_tile1, 0.7, new_tile2)
// will result in (0.5, 0.5, 0.7, 0). And for new tiles at the front, they are
// guaranteed a minimum score. So that if all the other tiles haven't been
// clicked for a while, it will have a chance to be placed at the front.
void SortTilesAndClearUnusedStats(std::vector<std::unique_ptr<Tile>>* tiles,
                                  std::map<std::string, TileStats>* tile_stats);

// Calculates the current tile score based on |current_time|. Tile score will
// decay over time.
double CalculateTileScore(const TileStats& tile_stats, base::Time current_time);

// Checks whether a tile ID is for trending tile.
bool IsTrendingTile(const std::string& tile_id);

// Shuffle tiles from position |TileConfig::GetTileShufflePosition()|
// so that low score tiles has a chance to be seen.
void ShuffleTiles(std::vector<Tile>* tiles, const TileShuffler& shuffer);

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_UTILS_H_
