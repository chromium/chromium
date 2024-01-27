// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_MANAGER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {
namespace proto {
class SegmentInfo;
}  // namespace proto

class ModelProvider;

// The ModelExecutionManager is used to own ModelProvider(s) that interact with
// optimization_guide. All model updates are saved to database.
class ModelManager {
 public:
  virtual ~ModelManager() = default;

  // Disallow copy/assign.
  ModelManager(const ModelManager&) = delete;
  ModelManager& operator=(const ModelManager&) = delete;

  // Invoked whenever there are changes to the state of a segmentation model.
  // Will not be invoked unless the proto::SegmentInfo is valid.
  using SegmentationModelUpdatedCallback =
      base::RepeatingCallback<void(proto::SegmentInfo,
                                   /*old_version*/ std::optional<int64_t>)>;

  virtual void Initialize() = 0;

  virtual ModelProvider* GetModelProvider(proto::SegmentId segment_id,
                                          proto::ModelSource model_source) = 0;

  // For tests:
  virtual void SetSegmentationModelUpdatedCallbackForTesting(
      SegmentationModelUpdatedCallback model_updated_callback) = 0;

 protected:
  ModelManager() = default;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_MANAGER_H_
