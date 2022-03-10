// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_H_

#include <utility>

#include "base/callback_forward.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"

namespace segmentation_platform {
namespace proto {
class SegmentInfo;
}  // namespace proto

// The ModelExecutionManager is the core class for interacting with the ML
// framework. The only requirement is to pass in the segment ID to execute the
// model for, and a callback will be posted with the result, once the
// calculation has finished.
class ModelExecutionManager {
 public:
  virtual ~ModelExecutionManager() = default;

  // Disallow copy/assign.
  ModelExecutionManager(const ModelExecutionManager&) = delete;
  ModelExecutionManager& operator=(const ModelExecutionManager&) = delete;

  // The float value is only valid when ModelExecutionStatus == kSuccess.
  using ModelExecutionCallback =
      base::OnceCallback<void(const std::pair<float, ModelExecutionStatus>&)>;

  // Invoked whenever there are changes to the state of a segmentation model.
  // Will not be invoked unless the proto::SegmentInfo is valid.
  using SegmentationModelUpdatedCallback =
      base::RepeatingCallback<void(proto::SegmentInfo)>;

  // Called to execute a given model. This assumes that data has been collected
  // for long enough for each of the individual ML features.
  virtual void ExecuteModel(const proto::SegmentInfo& segment_info,
                            ModelExecutionCallback callback) = 0;

 protected:
  ModelExecutionManager() = default;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_H_
