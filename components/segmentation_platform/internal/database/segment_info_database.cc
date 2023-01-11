// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/segment_info_database.h"

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"

namespace segmentation_platform {

namespace {

std::string ToString(SegmentId segment_id) {
  return base::NumberToString(static_cast<int>(segment_id));
}

std::vector<std::string> SegmentIdsToString(
    base::flat_set<SegmentId> segment_ids) {
  std::vector<std::string> result;
  for (SegmentId segment_id : segment_ids) {
    result.emplace_back(ToString(segment_id));
  }
  return result;
}

}  // namespace

SegmentInfoDatabase::SegmentInfoDatabase(
    std::unique_ptr<SegmentInfoProtoDb> database,
    std::unique_ptr<SegmentInfoCache> cache)
    : database_(std::move(database)), cache_(std::move(cache)) {}

SegmentInfoDatabase::~SegmentInfoDatabase() = default;

void SegmentInfoDatabase::Initialize(SuccessCallback callback) {
  database_->Init(
      leveldb_proto::CreateSimpleOptions(),
      base::BindOnce(&SegmentInfoDatabase::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SegmentInfoDatabase::OnMultipleSegmentInfoLoaded(
    std::unique_ptr<SegmentInfoList> segments_so_far,
    MultipleSegmentInfoCallback callback,
    bool success,
    std::unique_ptr<std::vector<proto::SegmentInfo>> all_infos) {
  if (success && all_infos) {
    for (auto& info : *all_infos.get()) {
      cache_->UpdateSegmentInfo(info.segment_id(), info);
      segments_so_far->emplace_back(
          std::make_pair(info.segment_id(), std::move(info)));
    }
  }

  std::move(callback).Run(std::move(segments_so_far));
}

void SegmentInfoDatabase::GetSegmentInfoForSegments(
    const base::flat_set<SegmentId>& segment_ids,
    MultipleSegmentInfoCallback callback) {
  base::flat_set<SegmentId> ids_needing_update;

  auto segments_so_far =
      cache_->GetSegmentInfoForSegments(segment_ids, ids_needing_update);

  if (ids_needing_update.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(segments_so_far)));
    return;
  }

  // Converting list of segment ids to string as per database requirement.
  std::vector<std::string> keys_to_fetch_from_db =
      SegmentIdsToString(ids_needing_update);

  database_->LoadEntriesWithFilter(
      base::BindRepeating(
          [](const std::vector<std::string>& key_dict, const std::string& key) {
            return base::Contains(key_dict, key);
          },
          keys_to_fetch_from_db),
      base::BindOnce(&SegmentInfoDatabase::OnMultipleSegmentInfoLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(segments_so_far),
                     std::move(callback)));
}

void SegmentInfoDatabase::GetSegmentInfo(SegmentId segment_id,
                                         SegmentInfoCallback callback) {
  std::pair<SegmentInfoCache::CachedItemState, absl::optional<SegmentInfo>>
      segment_info = cache_->GetSegmentInfo(segment_id);
  if (segment_info.first != SegmentInfoCache::CachedItemState::kNotCached) {
    std::move(callback).Run(std::move(segment_info.second));
    return;
  }

  database_->GetEntry(ToString(segment_id),
                      base::BindOnce(&SegmentInfoDatabase::OnGetSegmentInfo,
                                     weak_ptr_factory_.GetWeakPtr(), segment_id,
                                     std::move(callback)));
}

void SegmentInfoDatabase::OnGetSegmentInfo(
    SegmentId segment_id,
    SegmentInfoCallback callback,
    bool success,
    std::unique_ptr<proto::SegmentInfo> info) {
  cache_->UpdateSegmentInfo(segment_id, (success && info)
                                            ? absl::make_optional(*info)
                                            : absl::nullopt);
  std::move(callback).Run((success && info) ? absl::make_optional(*info)
                                            : absl::nullopt);
}

void SegmentInfoDatabase::UpdateSegment(
    SegmentId segment_id,
    absl::optional<proto::SegmentInfo> segment_info,
    SuccessCallback callback) {
  cache_->UpdateSegmentInfo(segment_id, segment_info);
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SegmentInfo>>>();
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();
  if (segment_info.has_value()) {
    entries_to_save->emplace_back(
        std::make_pair(ToString(segment_id), segment_info.value()));
  } else {
    keys_to_delete->emplace_back(ToString(segment_id));
  }
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_delete), std::move(callback));
}

void SegmentInfoDatabase::UpdateMultipleSegments(
    const SegmentInfoList& segments_to_update,
    const std::vector<proto::SegmentId>& segments_to_delete,
    SuccessCallback callback) {
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SegmentInfo>>>();
  auto entries_to_delete = std::make_unique<std::vector<std::string>>();
  for (auto& segment : segments_to_update) {
    const proto::SegmentId segment_id = segment.first;
    auto& segment_info = segment.second;

    // Updating the cache.
    cache_->UpdateSegmentInfo(segment_id, absl::make_optional(segment_info));

    // Determining entries to save for database.
    entries_to_save->emplace_back(
        std::make_pair(ToString(segment_id), std::move(segment_info)));
  }

  for (auto& segment_id : segments_to_delete) {
    entries_to_delete->emplace_back(ToString(segment_id));
  }

  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(entries_to_delete), std::move(callback));
}

void SegmentInfoDatabase::SaveSegmentResult(
    SegmentId segment_id,
    absl::optional<proto::PredictionResult> result,
    SuccessCallback callback) {
  GetSegmentInfo(
      segment_id,
      base::BindOnce(&SegmentInfoDatabase::OnGetSegmentInfoForUpdatingResults,
                     weak_ptr_factory_.GetWeakPtr(), result,
                     std::move(callback)));
}

void SegmentInfoDatabase::OnGetSegmentInfoForUpdatingResults(
    absl::optional<proto::PredictionResult> result,
    SuccessCallback callback,
    absl::optional<proto::SegmentInfo> segment_info) {
  // Ignore results if the metadata no longer exists.
  if (!segment_info.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  // Update results.
  if (result.has_value()) {
    segment_info->mutable_prediction_result()->CopyFrom(*result);
  } else {
    segment_info->clear_prediction_result();
  }
  cache_->UpdateSegmentInfo(segment_info->segment_id(), segment_info);
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SegmentInfo>>>();
  entries_to_save->emplace_back(std::make_pair(
      ToString(segment_info->segment_id()), std::move(segment_info.value())));
  database_->UpdateEntries(std::move(entries_to_save),
                           std::make_unique<std::vector<std::string>>(),
                           std::move(callback));
}

void SegmentInfoDatabase::OnDatabaseInitialized(
    SuccessCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  std::move(callback).Run(status == leveldb_proto::Enums::InitStatus::kOK);
}

}  // namespace segmentation_platform
