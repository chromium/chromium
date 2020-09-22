// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_group.h"

#include <sstream>
#include <utility>

#include "components/query_tiles/internal/tile_utils.h"

namespace query_tiles {

namespace {
// Score to be received by a tile when it is clicked.
constexpr double kTileClickScore = 1.0;

void DeepCopyGroup(const TileGroup& input, TileGroup* output) {
  DCHECK(output);

  output->id = input.id;
  output->locale = input.locale;
  output->last_updated_ts = input.last_updated_ts;
  output->tiles.clear();
  for (const auto& tile : input.tiles)
    output->tiles.emplace_back(std::make_unique<Tile>(*tile.get()));
  output->tile_stats = input.tile_stats;
}

}  // namespace

TileGroup::TileGroup() = default;

TileGroup::~TileGroup() = default;

bool TileGroup::operator==(const TileGroup& other) const {
  return id == other.id && locale == other.locale &&
         last_updated_ts == other.last_updated_ts &&
         tiles.size() == other.tiles.size();
}

bool TileGroup::operator!=(const TileGroup& other) const {
  return !(*this == other);
}

void TileGroup::OnTileClicked(const std::string& tile_id) {
  base::Time now_time = base::Time::Now();
  auto iter = tile_stats.find(tile_id);
  double score =
      (iter == tile_stats.end())
          ? kTileClickScore
          : kTileClickScore + CalculateTileScore(iter->second, now_time);
  tile_stats[tile_id] = TileStats(now_time, score);
}

TileGroup::TileGroup(const TileGroup& other) {
  DeepCopyGroup(other, this);
}

TileGroup::TileGroup(TileGroup&& other) = default;

TileGroup& TileGroup::operator=(const TileGroup& other) {
  DeepCopyGroup(other, this);
  return *this;
}

TileGroup& TileGroup::operator=(TileGroup&& other) = default;

std::string TileGroup::DebugString() {
  std::stringstream out;
  out << "Group detail: \n";
  out << "id: " << this->id << " | locale: " << this->locale
      << " | last_updated_ts: " << this->last_updated_ts << " \n";
  for (const auto& tile : this->tiles)
    out << tile->DebugString();
  return out.str();
}

}  // namespace query_tiles
