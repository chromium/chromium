// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_iterator.h"

#include <ostream>

#include "base/check_op.h"
#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/tile.h"

namespace query_tiles {

TileIterator::TileIterator(std::vector<const Tile*> tiles, int levels)
    : levels_(levels) {
  DCHECK(levels_ >= 0 || levels_ == kAllTiles);
  for (const auto* tile : tiles)
    MaybeAddToQueue(0, tile);
}

TileIterator::TileIterator(const TileGroup& tile_group, int levels)
    : levels_(levels) {
  DCHECK(levels_ >= 0 || levels_ == kAllTiles);
  for (const auto& tile : tile_group.tiles)
    MaybeAddToQueue(0, tile.get());
}

TileIterator::~TileIterator() = default;

bool TileIterator::HasNext() const {
  return !tiles_queue_.empty();
}

const Tile* TileIterator::Next() {
  if (!HasNext())
    return nullptr;

  TileLevelPair tile = tiles_queue_.front();
  tiles_queue_.pop();

  // Add subtiles into the queue if next level is still needed.
  if (levels_ == kAllTiles || tile.first < levels_) {
    for (const auto& subtile : tile.second->sub_tiles)
      MaybeAddToQueue(tile.first + 1, subtile.get());
  }

  return tile.second;
}

void TileIterator::MaybeAddToQueue(int level, const Tile* tile) {
  DCHECK_GE(level, 0) << "level should be non negative.";
  if (!tile)
    return;
  tiles_queue_.push(TileLevelPair(level, tile));
}

}  // namespace query_tiles
