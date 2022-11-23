// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_CACHE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_CACHE_H_

#include "base/containers/flat_map.h"
#include "base/types/id_type.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

// TrainingDataCache stores training data that is currently in the observation
// period.
class TrainingDataCache {
 public:
  // TODO(haileywang): Move this to
  // components/segmentation_platform/public/types/basic_types.h. Consider
  // evolving this into a struct that has a request type and ID.
  using RequestId = base::IdType64<class RequestIdTag>;
  TrainingDataCache();
  ~TrainingDataCache();

  // Stores the inputs for a segment given a request ID.
  void StoreInputs(proto::SegmentId segment_id,
                   RequestId request_id,
                   const ModelProvider::Request& inputs);

  // Retrieves and deletes the inputs for a segment given a request ID from the
  // cache. Returns nullopt when the associated request ID is not found.
  absl::optional<proto::TrainingData> GetInputsAndDelete(
      proto::SegmentId segment_id,
      RequestId request_id);

  // Retrieves the first request ID given a segment ID. Returns nullopt when no
  // request ID found. This is used when uma histogram triggering happens and
  // only segment ID is available.
  // Note: The earliest ID created by this cache will be returned first.
  absl::optional<RequestId> GetRequestId(proto::SegmentId segment_id);

  TrainingDataCache::RequestId GenerateNextId();

 private:
  RequestId::Generator request_id_generator;
  base::flat_map<proto::SegmentId,
                 base::flat_map<RequestId, proto::TrainingData>>
      cache;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATA_COLLECTION_TRAINING_DATA_CACHE_H_
