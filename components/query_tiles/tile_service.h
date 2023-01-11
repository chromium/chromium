// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_TILE_SERVICE_H_
#define COMPONENTS_QUERY_TILES_TILE_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/query_tiles/logger.h"
#include "components/query_tiles/tile.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace query_tiles {

using TileList = std::vector<Tile>;
using GetTilesCallback = base::OnceCallback<void(TileList)>;
using TileCallback = base::OnceCallback<void(absl::optional<Tile>)>;
using VisualsCallback = base::OnceCallback<void(const gfx::Image&)>;
using BackgroundTaskFinishedCallback = base::OnceCallback<void(bool)>;

// The central class on chrome client responsible for fetching, storing,
// managing, and displaying query tiles in chrome.
class TileService : public KeyedService, public base::SupportsUserData {
 public:
  // Called to retrieve all the tiles.
  virtual void GetQueryTiles(GetTilesCallback callback) = 0;

  // Called to retrieve the tile associated with |tile_id|.
  virtual void GetTile(const std::string& tile_id, TileCallback callback) = 0;

  // Start fetch query tiles from server.
  virtual void StartFetchForTiles(bool is_from_reduced_mode,
                                  BackgroundTaskFinishedCallback callback) = 0;

  // Cancel any existing scheduled task, and reset backoff.
  virtual void CancelTask() = 0;

  // Used for debugging and testing only. Clear everything in db.
  virtual void PurgeDb() = 0;

  // Used for setting the server url for test.
  virtual void SetServerUrl(const std::string& base_url) = 0;

  // Called when a tile was clicked.
  virtual void OnTileClicked(const std::string& tile_id) = 0;

  // Called when the final level of tile is selected. |parent_tile_id| is
  // the Id of the parent tile, if it exists.
  virtual void OnQuerySelected(
      const absl::optional<std::string>& parent_tile_id,
      const std::u16string& query_text) = 0;

  // Returns a Logger instance that is meant to be used by logging and debug UI
  // components in the larger system.
  virtual Logger* GetLogger() = 0;

  TileService() = default;
  ~TileService() override = default;

  TileService(const TileService&) = delete;
  TileService& operator=(const TileService&) = delete;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_TILE_SERVICE_H_
