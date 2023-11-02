// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_TEST_FAKE_TILE_SERVICE_H_
#define COMPONENTS_QUERY_TILES_TEST_FAKE_TILE_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/query_tiles/tile_service.h"

namespace query_tiles {

// A fake TileService that can be used for tests.
class FakeTileService : public TileService {
 public:
  FakeTileService();
  ~FakeTileService() override;

  // Disallow copy/assign.
  FakeTileService(const FakeTileService& other) = delete;
  FakeTileService& operator=(const FakeTileService& other) = delete;

 private:
  // TileService implementation.
  void GetQueryTiles(GetTilesCallback callback) override;
  void GetTile(const std::string& tile_id, TileCallback callback) override;
  void StartFetchForTiles(bool is_from_reduced_mode,
                          BackgroundTaskFinishedCallback callback) override;
  void CancelTask() override;
  void PurgeDb() override;
  void SetServerUrl(const std::string& url) override;
  void OnTileClicked(const std::string& tile_id) override;
  void OnQuerySelected(const absl::optional<std::string>& parent_tile_id,
                       const std::u16string& query_text) override;
  Logger* GetLogger() override;

  std::vector<std::unique_ptr<Tile>> tiles_;
  base::WeakPtrFactory<FakeTileService> weak_ptr_factory_{this};
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_TEST_FAKE_TILE_SERVICE_H_
