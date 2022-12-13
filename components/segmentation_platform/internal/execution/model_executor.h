// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_H_

#include <utility>

#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"

namespace segmentation_platform {

struct ExecutionRequest;

// Class used to process features and execute the model.
class ModelExecutor {
 public:
  ModelExecutor() = default;
  virtual ~ModelExecutor() = default;

  ModelExecutor(const ModelExecutor&) = delete;
  ModelExecutor& operator=(const ModelExecutor&) = delete;

  // Called to execute a given model. This assumes that data has been collected
  // for long enough for each of the individual ML features.
  using ModelExecutionCallback = ExecutionRequest::ModelExecutionCallback;

  // Computes input features using `segment_info` and executes the model using
  // `model_provider`, and returns result.
  virtual void ExecuteModel(std::unique_ptr<ExecutionRequest> request) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_MODEL_EXECUTOR_H_
