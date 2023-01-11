// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_MANAGER_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/query_tiles/internal/store.h"
#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/internal/tile_types.h"
#include "components/query_tiles/tile.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace query_tiles {

// Manages the in-memory tile group and coordinates with storage layer.
class TileManager {
 public:
  using TileStore = Store<TileGroup>;
  using TileGroupStatusCallback = base::OnceCallback<void(TileGroupStatus)>;
  using GetTilesCallback = base::OnceCallback<void(std::vector<Tile>)>;
  using TileCallback = base::OnceCallback<void(absl::optional<Tile>)>;

  // Creates the instance.
  static std::unique_ptr<TileManager> Create(
      std::unique_ptr<TileStore> tile_store,
      const std::string& accept_languages);

  // Initializes the query tile store, loading them into memory after
  // validating.
  virtual void Init(TileGroupStatusCallback callback) = 0;

  // Returns tiles to the caller in the given |locale|.
  virtual void GetTiles(bool shuffle_tiles, GetTilesCallback callback) = 0;

  // Returns the tile associated with |tile_id| to the caller.
  virtual void GetTile(const std::string& tile_id,
                       bool shuffle_tiles,
                       TileCallback callback) = 0;

  // Save the query tiles into database.
  virtual void SaveTiles(std::unique_ptr<TileGroup> tile_group,
                         TileGroupStatusCallback callback) = 0;

  // Delete everything in db. Used for debugging in WebUI only.
  virtual TileGroupStatus PurgeDb() = 0;

  // Dump the group. Used for debugging in WebUI only.
  virtual TileGroup* GetTileGroup() = 0;

  // Called when a tile is clicked.
  virtual void OnTileClicked(const std::string& tile_id) = 0;

  // Called when the final query is formed. |parent_tile_id| is the parent
  // Id of the last tile, if it exists.
  virtual void OnQuerySelected(
      const absl::optional<std::string>& parent_tile_id,
      const std::u16string& query_text) = 0;

  TileManager();
  virtual ~TileManager() = default;

  TileManager(const TileManager& other) = delete;
  TileManager& operator=(const TileManager& other) = delete;

  // ------------------------Testing------------------------
  virtual void SetAcceptLanguagesForTesting(
      const std::string& accept_languages) = 0;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_MANAGER_H_
