// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_group.h"

#include <set>
#include <sstream>
#include <utility>

#include "components/query_tiles/internal/tile_iterator.h"
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

// Removes |id| from |id_set|. Returns true if |id| is found, or false
// otherwise.
bool RemoveIdFromSet(std::set<std::string>* id_set, const std::string& id) {
  const auto it = id_set->find(id);
  if (it != id_set->end()) {
    id_set->erase(it);
    return true;
  }
  return false;
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

void TileGroup::RemoveTiles(const std::vector<std::string>& tile_ids) {
  std::set<std::string> id_set(tile_ids.begin(), tile_ids.end());
  std::queue<Tile*> tile_queue;
  // Check if there are top level tiles to be removed.
  for (auto iter = tiles.begin(); iter != tiles.end();) {
    if (RemoveIdFromSet(&id_set, (*iter)->id)) {
      iter = tiles.erase(iter);
      if (id_set.empty())
        return;
    } else {
      tile_queue.push(iter->get());
      ++iter;
    }
  }

  // Recursively check if there are sub tiles to be removed.
  while (!tile_queue.empty()) {
    Tile* tile = tile_queue.front();
    tile_queue.pop();
    for (auto it = tile->sub_tiles.begin(); it != tile->sub_tiles.end();) {
      if (RemoveIdFromSet(&id_set, (*it)->id)) {
        it = tile->sub_tiles.erase(it);
        if (id_set.empty())
          return;
      } else {
        tile_queue.push(it->get());
        ++it;
      }
    }
  }
}

}  // namespace query_tiles
