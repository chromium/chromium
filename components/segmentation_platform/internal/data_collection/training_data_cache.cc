// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_cache.h"

#include "base/rand_util.h"
#include "base/time/time.h"

namespace segmentation_platform {

using TrainingData = proto::TrainingData;

TrainingDataCache::TrainingDataCache(SegmentInfoDatabase* segment_info_database)
    : segment_info_database_(segment_info_database) {}

TrainingDataCache::~TrainingDataCache() = default;

void TrainingDataCache::StoreInputs(SegmentId segment_id,
                                    ModelSource model_source,
                                    const TrainingData& data,
                                    bool save_to_db) {
  if (save_to_db) {
    // TODO (ritikagup@) : Add handling for default models, if required.
    segment_info_database_->SaveTrainingData(
        segment_id, model_source, std::move(data), base::DoNothing());
  } else {
    cache_[std::make_pair(segment_id, model_source)]
          [TrainingRequestId::FromUnsafeValue(data.request_id())] =
              std::move(data);
  }
}

void TrainingDataCache::GetInputsAndDelete(SegmentId segment_id,
                                           ModelSource model_source,
                                           TrainingRequestId request_id,
                                           TrainingDataCallback callback) {
  std::optional<TrainingData> result;
  if (cache_.contains(std::make_pair(segment_id, model_source)) &&
      cache_[std::make_pair(segment_id, model_source)].contains(request_id)) {
    // TrainingRequestId found from cache, return and delete the cache entry.
    auto& segment_data = cache_[std::make_pair(segment_id, model_source)];
    auto it = segment_data.find(request_id);
    CHECK(it != segment_data.end());
    result = std::move(it->second);
    segment_data.erase(it);
    std::move(callback).Run(result);
  } else {
    segment_info_database_->GetTrainingData(
        segment_id, model_source, request_id,
        /*delete_from_db=*/true, std::move(callback));
  }
}

std::optional<TrainingRequestId> TrainingDataCache::GetRequestId(
    SegmentId segment_id,
    ModelSource model_source) {
  // TODO(haileywang): Add a metric to record how many request at a given time
  // every time this function is triggered.
  std::optional<TrainingRequestId> request_id;
  auto it = cache_.find(std::make_pair(segment_id, model_source));
  if (it == cache_.end() or it->second.size() == 0) {
    return request_id;
  }
  return it->second.begin()->first;
}

TrainingRequestId TrainingDataCache::GenerateNextId() {
  return TrainingRequestId::FromUnsafeValue(base::RandUint64());
}

}  // namespace segmentation_platform
