// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/data_collection/training_data_cache.h"

#include "base/time/time.h"

namespace segmentation_platform {

using proto::SegmentId;
using TrainingData = proto::TrainingData;

TrainingDataCache::TrainingDataCache() = default;

TrainingDataCache::~TrainingDataCache() = default;

void TrainingDataCache::StoreInputs(SegmentId segment_id,
                                    RequestId request_id,
                                    const std::vector<float>& inputs) {
  TrainingData training_data;
  for (const auto& input : inputs) {
    training_data.add_inputs(input);
  }
  training_data.set_decision_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  cache[segment_id][request_id] = std::move(training_data);
}

absl::optional<TrainingData> TrainingDataCache::GetInputsAndDelete(
    SegmentId segment_id,
    RequestId request_id) {
  absl::optional<TrainingData> result;
  if (cache.find(segment_id) != cache.end() &&
      cache[segment_id].find(request_id) != cache[segment_id].end()) {
    // Delete the requestID from cache.
    auto it = cache[segment_id].find(request_id);
    result = std::move(it->second);
    cache[segment_id].erase(it);
  }
  return result;
}

}  // namespace segmentation_platform
