// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_ITERATOR_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_ITERATOR_H_

#include <queue>
#include <utility>
#include <vector>

namespace query_tiles {

struct Tile;
struct TileGroup;

// Breadth first search iterator that can iterate through first few levels or
// everything in the tile tree structure. During iteration, tiles can't be
// changed or deleted within the lifecycle of the iterator.
// Example usage:
// void IterateGroup(const TileGroup& group) {
//   TileIterator it(group, TileIterator::kAllTiles);
//   while (it->HasNext()) {
//     const auto* tile = it->Next();
//     // Use tile data.
//   }
// }
//
class TileIterator {
 public:
  // Pass to |levels_| to iterates through all tiles.
  static constexpr int kAllTiles = -1;

  // Constructs an iterator that iterates first few |levels| of the |tiles|.
  // If |levels| is 0, only root tiles in |tiles| will be iterated.
  TileIterator(std::vector<const Tile*> tiles, int levels);

  // Constructs an iterator for a tile group.
  TileIterator(const TileGroup& tile_group, int levels);

  // Returns whether there are any remaining elements.
  bool HasNext() const;

  // Returns the next tile and moves to the next tile. When the iterator is
  // empty, return nullptr.
  const Tile* Next();

  ~TileIterator();
  TileIterator(const TileIterator&) = delete;
  TileIterator operator=(const TileIterator&) = delete;

 private:
  using TileLevelPair = std::pair<int, const Tile*>;
  using TilesQueue = std::queue<TileLevelPair>;

  // Adds an element to |tiles_queue_| if |tile| is valid.
  void MaybeAddToQueue(int level, const Tile* tile);

  TilesQueue tiles_queue_;

  // The number of levels of tiles to iterate on.
  const int levels_;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_ITERATOR_H_
