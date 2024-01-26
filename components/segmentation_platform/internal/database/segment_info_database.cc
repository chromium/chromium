// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/segment_info_database.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/segmentation_platform/internal/logging.h"
#include "components/segmentation_platform/internal/stats.h"

#include <string>

namespace segmentation_platform {

namespace {

std::string ToString(SegmentId segment_id, ModelSource model_source) {
  std::string prefix =
      (model_source == ModelSource::DEFAULT_MODEL_SOURCE ? "DEFAULT_" : "");
  return prefix + base::NumberToString(static_cast<int>(segment_id));
}

ModelSource GetModelSource(ModelSource model_source) {
  // If model source is not set in some segment info present in database, we
  // consider it to be from server models.
  if (model_source == ModelSource::UNKNOWN_MODEL_SOURCE) {
    model_source = ModelSource::SERVER_MODEL_SOURCE;
  }
  return model_source;
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

std::unique_ptr<SegmentInfoDatabase::SegmentInfoList>
SegmentInfoDatabase::GetSegmentInfoForBothModels(
    const base::flat_set<SegmentId>& segment_ids) {
  return cache_->GetSegmentInfoForBothModels(segment_ids);
}

void SegmentInfoDatabase::GetSegmentInfoForSegments(
    const base::flat_set<SegmentId>& segment_ids,
    MultipleSegmentInfoCallback callback) {
  auto segments_found = cache_->GetSegmentInfoForSegments(
      segment_ids, ModelSource::SERVER_MODEL_SOURCE);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(segments_found)));
}

void SegmentInfoDatabase::GetSegmentInfo(SegmentId segment_id,
                                         proto::ModelSource model_source,
                                         SegmentInfoCallback callback) {
  std::optional<SegmentInfo> result;
  const auto* cached = GetCachedSegmentInfo(segment_id, model_source);
  if (cached) {
    result = *cached;
  }
  std::move(callback).Run(std::move(result));
}

const SegmentInfo* SegmentInfoDatabase::GetCachedSegmentInfo(
    SegmentId segment_id,
    proto::ModelSource model_source) {
  return cache_->GetSegmentInfo(segment_id, model_source);
}

void SegmentInfoDatabase::GetTrainingData(SegmentId segment_id,
                                          ModelSource model_source,
                                          TrainingRequestId request_id,
                                          bool delete_from_db,
                                          TrainingDataCallback callback) {
  std::optional<proto::TrainingData> result;
  const auto* cached = cache_->GetSegmentInfo(segment_id, model_source);

  // Ignore results if the metadata no longer exists.
  if (!cached) {
    std::move(callback).Run(std::move(result));
    return;
  }
  SegmentInfo segment_info = *cached;  // Make a copy for update.

  for (int i = 0; i < segment_info.training_data_size(); i++) {
    if (segment_info.training_data(i).request_id() ==
        request_id.GetUnsafeValue()) {
      result = segment_info.training_data(i);
      break;
    }
  }

  if (delete_from_db) {
    // Delete the training data from cache and then post update to delete from
    // database.
    for (int i = 0; i < segment_info.training_data_size(); i++) {
      if (segment_info.training_data(i).request_id() ==
          request_id.GetUnsafeValue()) {
        segment_info.mutable_training_data()->DeleteSubrange(i, 1);
      }
    }
    UpdateSegment(segment_id, model_source, std::move(segment_info),
                  base::DoNothing());
  }

  // Notify the client with the result.
  std::move(callback).Run(std::move(result));
}

void SegmentInfoDatabase::UpdateSegment(
    SegmentId segment_id,
    ModelSource model_source,
    std::optional<proto::SegmentInfo> segment_info,
    SuccessCallback callback) {
  model_source = GetModelSource(model_source);
  cache_->UpdateSegmentInfo(segment_id, model_source, segment_info);

  // The cache has been updated now. We can notify the client synchronously.
  std::move(callback).Run(/*success=*/true);

  // Now write to the database asyncrhonously.
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SegmentInfo>>>();
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();
  if (segment_info.has_value()) {
    segment_info->set_model_source(model_source);
    entries_to_save->emplace_back(std::make_pair(
        ToString(segment_id, model_source), std::move(segment_info.value())));
  } else {
    keys_to_delete->emplace_back(ToString(segment_id, model_source));
  }
  database_->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_delete),
      base::BindOnce(&stats::RecordSegmentInfoDatabaseUpdateEntriesResult,
                     segment_id));
}

void SegmentInfoDatabase::UpdateMultipleSegments(
    const SegmentInfoList& segments_to_update,
    const std::vector<std::pair<proto::SegmentId, ModelSource>>&
        segments_to_delete,
    SuccessCallback callback) {
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SegmentInfo>>>();
  auto entries_to_delete = std::make_unique<std::vector<std::string>>();
  for (auto& segment : segments_to_update) {
    const proto::SegmentId segment_id = segment.first;
    const auto* segment_info = segment.second;
    ModelSource model_source = GetModelSource(segment_info->model_source());
    // Updating the cache.
    cache_->UpdateSegmentInfo(segment_id, model_source, *segment_info);

    // Determining entries to save for database.
    proto::SegmentInfo copy = *segment_info;
    if (!copy.has_model_source()) {
      copy.set_model_source(model_source);
    }
    entries_to_save->emplace_back(
        std::make_pair(ToString(segment_id, model_source), std::move(copy)));
  }

  // The cache has been updated now. We can notify the client synchronously.
  std::move(callback).Run(/*success=*/true);

  // TODO (ritikagup@) : Add handling for default models, if required.
  // Now write to the database asyncrhonously.
  for (auto& segment_id_and_model_source : segments_to_delete) {
    entries_to_delete->emplace_back(ToString(
        segment_id_and_model_source.first, segment_id_and_model_source.second));
  }

  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(entries_to_delete), base::DoNothing());
}

void SegmentInfoDatabase::SaveSegmentResult(
    SegmentId segment_id,
    ModelSource model_source,
    std::optional<proto::PredictionResult> result,
    SuccessCallback callback) {
  const auto* cached = cache_->GetSegmentInfo(segment_id, model_source);

  // Ignore results if the metadata no longer exists.
  if (!cached) {
    std::move(callback).Run(false);
    return;
  }
  SegmentInfo segment_info = *cached;  // Make a copy for update.

  // Update results.
  if (result.has_value()) {
    VLOG(1) << "SaveSegmentResult: saving: "
            << segmentation_platform::PredictionResultToDebugString(
                   result.value())
            << " for segment id: " << proto::SegmentId_Name(segment_id);
    segment_info.mutable_prediction_result()->Swap(&(*result));
  } else {
    VLOG(1) << "SaveSegmentResult: clearing prediction result for segment "
            << proto::SegmentId_Name(segment_id);
    segment_info.clear_prediction_result();
  }

  UpdateSegment(segment_id, model_source, std::move(segment_info),
                std::move(callback));
}

void SegmentInfoDatabase::SaveTrainingData(SegmentId segment_id,
                                           ModelSource model_source,
                                           const proto::TrainingData& data,
                                           SuccessCallback callback) {
  const auto* cached = cache_->GetSegmentInfo(segment_id, model_source);

  // Ignore results if the metadata no longer exists.
  if (!cached) {
    std::move(callback).Run(false);
    return;
  }
  SegmentInfo segment_info = *cached;  // Make a copy for update.

  // Update training data.
  segment_info.add_training_data()->CopyFrom(data);

  UpdateSegment(segment_id, model_source, std::move(segment_info),
                std::move(callback));
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
    for (auto& info : *all_infos.get()) {
      ModelSource model_source = GetModelSource(info.model_source());
      proto::SegmentId segment_id = info.segment_id();
      cache_->UpdateSegmentInfo(segment_id, model_source, std::move(info));
    }
  }
  std::move(callback).Run(success);
}

}  // namespace segmentation_platform
