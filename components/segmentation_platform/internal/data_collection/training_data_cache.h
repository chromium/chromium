// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_CACHE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_CACHE_H_

#include "base/containers/flat_map.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/trigger.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

// TrainingDataCache stores training data that is currently in the observation
// period.
class TrainingDataCache {
 public:
  using TrainingDataCallback = SegmentInfoDatabase::TrainingDataCallback;

  explicit TrainingDataCache(SegmentInfoDatabase* segment_info_database);
  ~TrainingDataCache();

  // Stores the inputs for a segment given a request ID.
  void StoreInputs(proto::SegmentId segment_id,
                   const proto::TrainingData& data,
                   bool save_to_db = false);

  // Retrieves and deletes the training data from the cache for a segment given
  // the segment ID and the request ID. Returns nullopt when the associated
  // request ID is not found.
  void GetInputsAndDelete(proto::SegmentId segment_id,
                          TrainingRequestId request_id,
                          TrainingDataCallback callback);

  // Retrieves the first request ID given a segment ID. Returns nullopt when no
  // request ID found. This is used when uma histogram triggering happens and
  // only segment ID is available.
  // Note: The earliest ID created by this cache will be returned first.
  absl::optional<TrainingRequestId> GetRequestId(proto::SegmentId segment_id);

  TrainingRequestId GenerateNextId();

 private:
  const raw_ptr<SegmentInfoDatabase, DanglingUntriaged> segment_info_database_;
  TrainingRequestId::Generator request_id_generator;
  base::flat_map<proto::SegmentId,
                 base::flat_map<TrainingRequestId, proto::TrainingData>>
      cache;
  base::WeakPtrFactory<TrainingDataCache> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_CACHE_H_
