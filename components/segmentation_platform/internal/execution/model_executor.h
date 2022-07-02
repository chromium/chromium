// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_H_

#include <utility>

#include "base/bind.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"

namespace segmentation_platform {

struct ExecutionRequest;

// Class used to process features and execute the model.
class ModelExecutor {
 public:
  ModelExecutor() = default;
  virtual ~ModelExecutor() = default;

  ModelExecutor(ModelExecutor&) = delete;
  ModelExecutor& operator=(ModelExecutor&) = delete;

  // Called to execute a given model. This assumes that data has been collected
  // for long enough for each of the individual ML features.
  // The float value is only valid when ModelExecutionStatus == kSuccess.
  using ModelExecutionCallback =
      base::OnceCallback<void(const std::pair<float, ModelExecutionStatus>&)>;

  // Computes input features using `segment_info` and executes the model using
  // `model_provider`, and returns result.
  virtual void ExecuteModel(std::unique_ptr<ExecutionRequest> request) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_H_
