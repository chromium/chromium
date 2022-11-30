// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_H_

#include "base/callback_forward.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {
namespace proto {
class SegmentInfo;
}  // namespace proto

class ModelProvider;

// The ModelExecutionManager is used to own ModelProvider(s) that interact with
// optimization_guide, and not used for default model. All model updates are
// saved to database.
class ModelExecutionManager {
 public:
  virtual ~ModelExecutionManager() = default;

  // Disallow copy/assign.
  ModelExecutionManager(const ModelExecutionManager&) = delete;
  ModelExecutionManager& operator=(const ModelExecutionManager&) = delete;

  // Invoked whenever there are changes to the state of a segmentation model.
  // Will not be invoked unless the proto::SegmentInfo is valid.
  using SegmentationModelUpdatedCallback =
      base::RepeatingCallback<void(proto::SegmentInfo)>;

  virtual ModelProvider* GetProvider(proto::SegmentId segment_id) = 0;

 protected:
  ModelExecutionManager() = default;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTION_MANAGER_H_
