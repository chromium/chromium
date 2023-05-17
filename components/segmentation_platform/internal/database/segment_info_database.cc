// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/segment_info_database.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/segmentation_platform/internal/logging.h"

#include <sstream>
#include <string>

namespace segmentation_platform {

namespace {

std::string ToString(SegmentId segment_id) {
  return base::NumberToString(static_cast<int>(segment_id));
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

void SegmentInfoDatabase::GetSegmentInfoForSegments(
    const base::flat_set<SegmentId>& segment_ids,
    MultipleSegmentInfoCallback callback) {
  auto segments_found = cache_->GetSegmentInfoForSegments(segment_ids);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(segments_found)));
}

void SegmentInfoDatabase::GetSegmentInfo(SegmentId segment_id,
                                         SegmentInfoCallback callback) {
  std::move(callback).Run(cache_->GetSegmentInfo(segment_id));
}

absl::optional<SegmentInfo> SegmentInfoDatabase::GetCachedSegmentInfo(
    SegmentId segment_id) {
  return cache_->GetSegmentInfo(segment_id);
}

void SegmentInfoDatabase::GetTrainingData(SegmentId segment_id,
                                          TrainingRequestId request_id,
                                          bool delete_from_db,
                                          TrainingDataCallback callback) {
  absl::optional<SegmentInfo> segment_info = cache_->GetSegmentInfo(segment_id);
  absl::optional<proto::TrainingData> result;

  // Ignore results if the metadata no longer exists.
  if (!segment_info.has_value()) {
    std::move(callback).Run(std::move(result));
    return;
  }

  const auto& info = segment_info.value();
  for (int i = 0; i < info.training_data_size(); i++) {
    if (info.training_data(i).request_id() == request_id.GetUnsafeValue()) {
      result = info.training_data(i);
      break;
    }
  }

  if (delete_from_db) {
    // Delete the training data from cache and then post update to delete from
    // database.
    for (int i = 0; i < segment_info->training_data_size(); i++) {
      if (segment_info->training_data(i).request_id() ==
          request_id.GetUnsafeValue()) {
        segment_info->mutable_training_data()->DeleteSubrange(i, 1);
      }
    }
    UpdateSegment(segment_id, std::move(segment_info), base::DoNothing());
  }

  // Notify the client with the result.
  std::move(callback).Run(std::move(result));
}

void SegmentInfoDatabase::UpdateSegment(
    SegmentId segment_id,
    absl::optional<proto::SegmentInfo> segment_info,
    SuccessCallback callback) {
  cache_->UpdateSegmentInfo(segment_id, segment_info);

  // The cache has been updated now. We can notify the client synchronously.
  std::move(callback).Run(/*success=*/true);

  // Now write to the database asyncrhonously.
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
                           std::move(keys_to_delete), base::DoNothing());
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

  // The cache has been updated now. We can notify the client synchronously.
  std::move(callback).Run(/*success=*/true);

  // Now write to the database asyncrhonously.
  for (auto& segment_id : segments_to_delete) {
    entries_to_delete->emplace_back(ToString(segment_id));
  }

  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(entries_to_delete), base::DoNothing());
}

void SegmentInfoDatabase::SaveSegmentResult(
    SegmentId segment_id,
    absl::optional<proto::PredictionResult> result,
    SuccessCallback callback) {
  auto segment_info = cache_->GetSegmentInfo(segment_id);

  // Ignore results if the metadata no longer exists.
  if (!segment_info.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  // Update results.
  if (result.has_value()) {
    VLOG(1) << "SaveSegmentResult: saving: "
            << segmentation_platform::PredictionResultToDebugString(
                   result.value())
            << " for segment id: " << proto::SegmentId_Name(segment_id);
    segment_info->mutable_prediction_result()->CopyFrom(*result);
  } else {
    VLOG(1) << "SaveSegmentResult: clearing prediction result for segment "
            << proto::SegmentId_Name(segment_id);
    segment_info->clear_prediction_result();
  }

  UpdateSegment(segment_id, std::move(segment_info), std::move(callback));
}

void SegmentInfoDatabase::SaveTrainingData(SegmentId segment_id,
                                           const proto::TrainingData& data,
                                           SuccessCallback callback) {
  auto segment_info = cache_->GetSegmentInfo(segment_id);

  // Ignore data if the metadata no longer exists.
  if (!segment_info.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  // Update training data.
  segment_info->add_training_data()->CopyFrom(data);

  UpdateSegment(segment_id, std::move(segment_info), std::move(callback));
}

void SegmentInfoDatabase::OnDatabaseInitialized(
    SuccessCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  bool success = (status == leveldb_proto::Enums::InitStatus::kOK);

  if (!success) {
    std::move(callback).Run(success);
    return;
  }

  // Initialize the cache by reading the database into the in-memory cache to
  // be accessed hereafter.
  database_->LoadEntries(base::BindOnce(&SegmentInfoDatabase::OnLoadAllEntries,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(callback)));
}

void SegmentInfoDatabase::OnLoadAllEntries(
    SuccessCallback callback,
    bool success,
    std::unique_ptr<std::vector<proto::SegmentInfo>> all_infos) {
  if (success) {
    // Add all the entries to the cache on startup.
    for (auto info : *all_infos.get()) {
      cache_->UpdateSegmentInfo(info.segment_id(), info);
    }
  }
  std::move(callback).Run(success);
}

}  // namespace segmentation_platform
