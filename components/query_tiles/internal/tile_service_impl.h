// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "components/query_tiles/internal/image_prefetcher.h"
#include "components/query_tiles/internal/tile_fetcher.h"
#include "components/query_tiles/internal/tile_manager.h"
#include "components/query_tiles/internal/tile_service_scheduler.h"
#include "components/query_tiles/internal/tile_types.h"
#include "components/query_tiles/logger.h"
#include "components/query_tiles/tile_service.h"

namespace query_tiles {

// A TileService that needs to be explicitly initialized.
class InitializableTileService : public TileService {
 public:
  // Initializes the tile service.
  virtual void Initialize(SuccessCallback callback) = 0;

  InitializableTileService() = default;
  ~InitializableTileService() override = default;
};

class TileServiceImpl : public InitializableTileService,
                        public TileServiceScheduler::Delegate {
 public:
  TileServiceImpl(std::unique_ptr<ImagePrefetcher> image_prefetcher,
                  std::unique_ptr<TileManager> tile_manager,
                  std::unique_ptr<TileServiceScheduler> scheduler,
                  std::unique_ptr<TileFetcher> tile_fetcher,
                  base::Clock* clock,
                  std::unique_ptr<Logger> logger);
  ~TileServiceImpl() override;

  // Disallow copy/assign.
  TileServiceImpl(const TileServiceImpl& other) = delete;
  TileServiceImpl& operator=(const TileServiceImpl& other) = delete;

 private:
  friend class TileServiceImplTest;

  // InitializableTileService implementation.
  void Initialize(SuccessCallback callback) override;
  void GetQueryTiles(GetTilesCallback callback) override;
  void GetTile(const std::string& tile_id, TileCallback callback) override;
  void StartFetchForTiles(bool is_from_reduced_mode,
                          BackgroundTaskFinishedCallback callback) override;
  void CancelTask() override;
  void PurgeDb() override;
  void SetServerUrl(const std::string& base_url) override;
  void OnTileClicked(const std::string& tile_id) override;
  void OnQuerySelected(const absl::optional<std::string>& parent_tile_id,
                       const std::u16string& query_text) override;
  Logger* GetLogger() override;

  // TileServiceScheduler::Delegate implementation.
  TileGroup* GetTileGroup() override;

  // Called when tile manager is initialized.
  void OnTileManagerInitialized(SuccessCallback callback,
                                TileGroupStatus status);

  // Called when fetching from server is completed.
  void OnFetchFinished(bool is_from_reduced_mode,
                       BackgroundTaskFinishedCallback task_finished_callback,
                       TileInfoRequestStatus status,
                       const std::unique_ptr<std::string> response_body);

  // Called when saving to db via manager layer is completed.
  void OnTilesSaved(TileGroup tile_group,
                    bool is_from_reduced_mode,
                    BackgroundTaskFinishedCallback task_finished_callback,
                    TileGroupStatus status);

  // Called when image prefetching are finished.
  void OnPrefetchImagesDone(
      BackgroundTaskFinishedCallback task_finished_callback);

  // Used to preload tile images.
  std::unique_ptr<ImagePrefetcher> image_prefetcher_;

  // Manages in memory tile group and coordinates with TileStore.
  std::unique_ptr<TileManager> tile_manager_;

  // Scheduler wraps background_task::Scheduler and manages reschedule logic.
  std::unique_ptr<TileServiceScheduler> scheduler_;

  // Fetcher to execute download jobs from Google server.
  std::unique_ptr<TileFetcher> tile_fetcher_;

  // Clock object.
  raw_ptr<base::Clock> clock_;

  std::unique_ptr<Logger> logger_;

  base::WeakPtrFactory<TileServiceImpl> weak_ptr_factory_{this};
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_
