// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_GROUP_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_GROUP_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/query_tiles/tile.h"

namespace query_tiles {

// A group of query tiles and metadata.
struct TileGroup {
  TileGroup();
  TileGroup(const TileGroup& other);
  TileGroup(TileGroup&& other);

  ~TileGroup();

  TileGroup& operator=(const TileGroup& other);
  TileGroup& operator=(TileGroup&& other);

  bool operator==(const TileGroup& other) const;
  bool operator!=(const TileGroup& other) const;

  // Called when a tile was clicked, need to recalculate |tile_stats|.
  void OnTileClicked(const std::string& tile_id);

  // Remove a tile from |tiles| given by its ID.
  void RemoveTiles(const std::vector<std::string>& tile_ids);

  // Find a tile with the given ID;
  Tile* FindTile(const std::string& tile_id);

  // Unique id for the group.
  std::string id;

  // Locale setting of this group.
  std::string locale;

  // Last updated timestamp in milliseconds.
  base::Time last_updated_ts;

  // Top level tiles.
  std::vector<std::unique_ptr<Tile>> tiles;

  // Map from tile id to its stats.
  std::map<std::string, TileStats> tile_stats;

  // Print pretty formatted content in TileGroup struct.
  std::string DebugString();
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_GROUP_H_
