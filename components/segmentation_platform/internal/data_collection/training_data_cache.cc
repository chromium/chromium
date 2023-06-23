// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_cache.h"

#include "base/time/time.h"

namespace segmentation_platform {

using proto::SegmentId;
using TrainingData = proto::TrainingData;

TrainingDataCache::TrainingDataCache(SegmentInfoDatabase* segment_info_database)
    : segment_info_database_(segment_info_database) {}

TrainingDataCache::~TrainingDataCache() = default;

void TrainingDataCache::StoreInputs(proto::SegmentId segment_id,
                                    const proto::TrainingData& data,
                                    bool save_to_db) {
  if (save_to_db) {
    // TODO (ritikagup@) : Add handling for default models, if required.
    segment_info_database_->SaveTrainingData(
        segment_id, proto::ModelSource::SERVER_MODEL_SOURCE, std::move(data),
        base::DoNothing());
  } else {
    cache[segment_id][TrainingRequestId::FromUnsafeValue(data.request_id())] =
        std::move(data);
  }
}

void TrainingDataCache::GetInputsAndDelete(SegmentId segment_id,
                                           TrainingRequestId request_id,
                                           TrainingDataCallback callback) {
  absl::optional<TrainingData> result;
  if (cache.contains(segment_id) && cache[segment_id].contains(request_id)) {
    // TrainingRequestId found from cache, return and delete the cache entry.
    auto& segment_data = cache[segment_id];
    auto it = segment_data.find(request_id);
    result = std::move(it->second);
    segment_data.erase(it);
    std::move(callback).Run(result);
  } else {
    // TODO (ritikagup@) : Add handling for default models, if required.
    segment_info_database_->GetTrainingData(
        segment_id, proto::ModelSource::SERVER_MODEL_SOURCE, request_id,
        /*delete_from_db=*/true, std::move(callback));
  }
}

absl::optional<TrainingRequestId> TrainingDataCache::GetRequestId(
    proto::SegmentId segment_id) {
  // TODO(haileywang): Add a metric to record how many request at a given time
  // every time this function is triggered.
  absl::optional<TrainingRequestId> request_id;
  auto it = cache.find(segment_id);
  if (it == cache.end() or it->second.size() == 0) {
    return request_id;
  }
  return it->second.begin()->first;
}

TrainingRequestId TrainingDataCache::GenerateNextId() {
  return request_id_generator.GenerateNextId();
}

}  // namespace segmentation_platform
