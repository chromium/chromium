// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_service_impl.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "components/query_tiles/internal/proto_conversion.h"
#include "components/query_tiles/internal/tile_config.h"

namespace query_tiles {

TileServiceImpl::TileServiceImpl(
    std::unique_ptr<ImagePrefetcher> image_prefetcher,
    std::unique_ptr<TileManager> tile_manager,
    std::unique_ptr<TileServiceScheduler> scheduler,
    std::unique_ptr<TileFetcher> tile_fetcher,
    base::Clock* clock,
    std::unique_ptr<Logger> logger)
    : image_prefetcher_(std::move(image_prefetcher)),
      tile_manager_(std::move(tile_manager)),
      scheduler_(std::move(scheduler)),
      tile_fetcher_(std::move(tile_fetcher)),
      clock_(clock),
      logger_(std::move(logger)) {}

TileServiceImpl::~TileServiceImpl() = default;

void TileServiceImpl::Initialize(SuccessCallback callback) {
  tile_manager_->Init(base::BindOnce(&TileServiceImpl::OnTileManagerInitialized,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

void TileServiceImpl::OnTileManagerInitialized(SuccessCallback callback,
                                               TileGroupStatus status) {
  bool success = (status == TileGroupStatus::kSuccess ||
                  status == TileGroupStatus::kNoTiles);
  DCHECK(callback);
  scheduler_->SetDelegate(this);
  scheduler_->OnTileManagerInitialized(status);
  std::move(callback).Run(success);
}

void TileServiceImpl::GetQueryTiles(GetTilesCallback callback) {
  tile_manager_->GetTiles(true /*shuffle_tiles*/, std::move(callback));
}

void TileServiceImpl::GetTile(const std::string& tile_id,
                              TileCallback callback) {
  tile_manager_->GetTile(tile_id, true /*shuffle_tiles*/, std::move(callback));
}

void TileServiceImpl::StartFetchForTiles(
    bool is_from_reduced_mode,
    BackgroundTaskFinishedCallback task_finished_callback) {
  DCHECK(tile_fetcher_);
  tile_fetcher_->StartFetchForTiles(base::BindOnce(
      &TileServiceImpl::OnFetchFinished, weak_ptr_factory_.GetWeakPtr(),
      is_from_reduced_mode, std::move(task_finished_callback)));
  scheduler_->OnFetchStarted();
}

void TileServiceImpl::CancelTask() {
  scheduler_->CancelTask();
}

void TileServiceImpl::PurgeDb() {
  auto status = tile_manager_->PurgeDb();
  scheduler_->OnDbPurged(status);
}

void TileServiceImpl::SetServerUrl(const std::string& base_url) {
  if (base_url.empty())
    return;
  tile_fetcher_->SetServerUrl(
      TileConfig::GetQueryTilesServerUrl(base_url, true));
}

void TileServiceImpl::OnFetchFinished(
    bool is_from_reduced_mode,
    BackgroundTaskFinishedCallback task_finished_callback,
    TileInfoRequestStatus status,
    const std::unique_ptr<std::string> response_body) {
  query_tiles::proto::ServerResponse response_proto;
  if (status == TileInfoRequestStatus::kSuccess) {
    bool parse_success = response_proto.ParseFromString(*response_body.get());
    if (parse_success) {
      TileGroup group;
      TileGroupFromResponse(response_proto, &group);
      group.id = base::GenerateGUID();
      group.last_updated_ts = clock_->Now();
      auto group_copy = std::make_unique<TileGroup>(group);
      tile_manager_->SaveTiles(
          std::move(group_copy),
          base::BindOnce(&TileServiceImpl::OnTilesSaved,
                         weak_ptr_factory_.GetWeakPtr(), std::move(group),
                         is_from_reduced_mode,
                         std::move(task_finished_callback)));
    } else {
      status = TileInfoRequestStatus::kShouldSuspend;
    }
  } else {
    std::move(task_finished_callback).Run(false /*reschedule*/);
  }
  scheduler_->OnFetchCompleted(status);
}

void TileServiceImpl::OnTilesSaved(
    TileGroup tile_group,
    bool is_from_reduced_mode,
    BackgroundTaskFinishedCallback task_finished_callback,
    TileGroupStatus status) {
  scheduler_->OnGroupDataSaved(status);

  if (status != TileGroupStatus::kSuccess) {
    std::move(task_finished_callback).Run(false /*reschedule*/);
    return;
  }

  image_prefetcher_->Prefetch(
      std::move(tile_group), is_from_reduced_mode,
      base::BindOnce(&TileServiceImpl::OnPrefetchImagesDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(task_finished_callback)));
}

void TileServiceImpl::OnPrefetchImagesDone(
    BackgroundTaskFinishedCallback task_finished_callback) {
  DCHECK(task_finished_callback);
  std::move(task_finished_callback).Run(false /*reschedule*/);
}

TileGroup* TileServiceImpl::GetTileGroup() {
  return tile_manager_->GetTileGroup();
}

void TileServiceImpl::OnTileClicked(const std::string& tile_id) {
  tile_manager_->OnTileClicked(tile_id);
}

void TileServiceImpl::OnQuerySelected(
    const absl::optional<std::string>& parent_tile_id,
    const std::u16string& query_text) {
  tile_manager_->OnQuerySelected(std::move(parent_tile_id), query_text);
}

Logger* TileServiceImpl::GetLogger() {
  return logger_.get();
}

}  // namespace query_tiles
