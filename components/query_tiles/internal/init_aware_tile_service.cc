// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/init_aware_tile_service.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"

namespace query_tiles {

InitAwareTileService::InitAwareTileService(
    std::unique_ptr<InitializableTileService> tile_service)
    : tile_service_(std::move(tile_service)) {
  tile_service_->Initialize(
      base::BindOnce(&InitAwareTileService::OnTileServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InitAwareTileService::OnTileServiceInitialized(bool success) {
  DCHECK(!init_success_.has_value());
  init_success_ = success;

  // Flush all cached calls in FIFO sequence.
  while (!cached_api_calls_.empty()) {
    auto api_call = std::move(cached_api_calls_.front());
    cached_api_calls_.pop_front();
    std::move(api_call).Run();
  }
}

void InitAwareTileService::GetQueryTiles(GetTilesCallback callback) {
  if (IsReady()) {
    tile_service_->GetQueryTiles(std::move(callback));
    return;
  }

  if (IsFailed()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), TileList()));
    return;
  }

  MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::GetQueryTiles,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(callback)));
}

void InitAwareTileService::GetTile(const std::string& tile_id,
                                   TileCallback callback) {
  if (IsReady()) {
    tile_service_->GetTile(tile_id, std::move(callback));
    return;
  }

  if (IsFailed()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
    return;
  }

  MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::GetTile,
                                   weak_ptr_factory_.GetWeakPtr(), tile_id,
                                   std::move(callback)));
}

void InitAwareTileService::StartFetchForTiles(
    bool is_from_reduced_mode,
    BackgroundTaskFinishedCallback callback) {
  if (IsReady()) {
    tile_service_->StartFetchForTiles(is_from_reduced_mode,
                                      std::move(callback));
    return;
  }

  if (IsFailed()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false /*need_reschedule*/));
    return;
  }

  MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::StartFetchForTiles,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   is_from_reduced_mode, std::move(callback)));
}

void InitAwareTileService::CancelTask() {
  if (IsReady()) {
    tile_service_->CancelTask();
  } else if (!IsFailed()) {
    MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::CancelTask,
                                     weak_ptr_factory_.GetWeakPtr()));
  }
}

void InitAwareTileService::PurgeDb() {
  if (IsReady()) {
    tile_service_->PurgeDb();
  } else if (!IsFailed()) {
    MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::PurgeDb,
                                     weak_ptr_factory_.GetWeakPtr()));
  }
}

void InitAwareTileService::SetServerUrl(const std::string& base_url) {
  if (IsReady()) {
    tile_service_->SetServerUrl(base_url);
  } else if (!IsFailed()) {
    MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::SetServerUrl,
                                     weak_ptr_factory_.GetWeakPtr(), base_url));
  }
}

void InitAwareTileService::OnTileClicked(const std::string& tile_id) {
  if (IsReady()) {
    tile_service_->OnTileClicked(tile_id);
  } else if (!IsFailed()) {
    MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::OnTileClicked,
                                     weak_ptr_factory_.GetWeakPtr(), tile_id));
  }
}

void InitAwareTileService::OnQuerySelected(
    const absl::optional<std::string>& parent_tile_id,
    const std::u16string& query_text) {
  if (IsReady()) {
    tile_service_->OnQuerySelected(std::move(parent_tile_id), query_text);
  } else if (!IsFailed()) {
    MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::OnQuerySelected,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(parent_tile_id), query_text));
  }
}

Logger* InitAwareTileService::GetLogger() {
  return tile_service_->GetLogger();
}

void InitAwareTileService::MaybeCacheApiCall(base::OnceClosure api_call) {
  DCHECK(!init_success_.has_value())
      << "Only cache API calls before initialization.";
  cached_api_calls_.emplace_back(std::move(api_call));
}

bool InitAwareTileService::IsReady() const {
  return init_success_.has_value() && init_success_.value();
}

bool InitAwareTileService::IsFailed() const {
  return init_success_.has_value() && !init_success_.value();
}

InitAwareTileService::~InitAwareTileService() = default;

}  // namespace query_tiles
