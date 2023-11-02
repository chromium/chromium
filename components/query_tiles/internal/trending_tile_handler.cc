// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/trending_tile_handler.h"

#include "components/query_tiles/internal/stats.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_utils.h"
#include "components/query_tiles/switches.h"

namespace query_tiles {

TrendingTileHandler::TrendingTileHandler() = default;

TrendingTileHandler::~TrendingTileHandler() = default;

void TrendingTileHandler::Reset() {
  tile_impressions_.clear();
}

std::vector<Tile> TrendingTileHandler::FilterExtraTrendingTiles(
    const std::vector<std::unique_ptr<Tile>>& tiles) {
  int trending_count = 0;
  std::vector<Tile> result;
  for (const auto& tile : tiles) {
    if (IsTrendingTile(tile->id)) {
      if (trending_count >= TileConfig::GetNumTrendingTilesToDisplay())
        continue;
      ++trending_count;
      RecordImpression(tile->id);
    }
    result.emplace_back(*tile);
  }
  return result;
}

void TrendingTileHandler::OnTileClicked(const std::string& tile_id) {
  if (IsTrendingTile(tile_id))
    stats::RecordTrendingTileEvent(stats::TrendingTileEvent::kClicked);
}

std::vector<std::string> TrendingTileHandler::GetTrendingTilesToRemove() {
  std::vector<std::string> tile_ids;
  ImpressionMap::iterator it = tile_impressions_.begin();
  while (it != tile_impressions_.end()) {
    if (it->second >= TileConfig::GetMaxTrendingTileImpressions()) {
      tile_ids.emplace_back(it->first);
      it = tile_impressions_.erase(it);
      stats::RecordTrendingTileEvent(stats::TrendingTileEvent::kRemoved);
    } else {
      ++it;
    }
  }

  return tile_ids;
}

void TrendingTileHandler::RecordImpression(const std::string& tile_id) {
  ++tile_impressions_[tile_id];
  stats::RecordTrendingTileEvent(stats::TrendingTileEvent::kShown);
}

}  // namespace query_tiles
