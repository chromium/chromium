// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TRENDING_TILE_HANDLER_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TRENDING_TILE_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/tile.h"

namespace query_tiles {

// Class for handling trending tiles. It checks whether a trending tile
// should be displayed, hidden or removed.
class TrendingTileHandler {
 public:
  // Map between tile ID and tile impression.
  using ImpressionMap = std::map<std::string, int>;

  TrendingTileHandler();
  ~TrendingTileHandler();

  TrendingTileHandler(const TrendingTileHandler& other) = delete;
  TrendingTileHandler& operator=(const TrendingTileHandler& other) = delete;

  // Resets the impression for all tiles. Must call this before calling other
  // methods. If tile group changes, this method need to be called again.
  void Reset();

  // Given a list of tiles, remove extra trending tiles and return a list
  // of tiles for display.
  std::vector<Tile> FilterExtraTrendingTiles(
      const std::vector<std::unique_ptr<Tile>>& tiles);

  // Called when a tile is clicked.
  void OnTileClicked(const std::string& tile_id);

  // Returns a list of trending tile Ids to remove.
  std::vector<std::string> GetTrendingTilesToRemove();

 private:
  // Record the impression for a tile with the give id.
  void RecordImpression(const std::string& tile_id);

  // Map to track how many times each tile is requested.
  // A tile's impression is cleared after click.
  // TODO(qinmin): move this to |tile_stats_group_|.
  ImpressionMap tile_impressions_;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TRENDING_TILE_HANDLER_H_
