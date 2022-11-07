// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/execution_request.h"
#include "components/segmentation_platform/internal/execution/model_execution_status.h"

namespace segmentation_platform {

ModelExecutionResult::ModelExecutionResult(
    const ModelProvider::Request& inputs,
    const ModelProvider::Response& scores)
    : scores(std::move(scores)),
      status(ModelExecutionStatus::kSuccess),
      inputs(std::move(inputs)) {}

ModelExecutionResult::ModelExecutionResult(ModelExecutionStatus status)
    : status(status) {}

ModelExecutionResult::~ModelExecutionResult() = default;

ExecutionRequest::ExecutionRequest() = default;
ExecutionRequest::~ExecutionRequest() = default;

}  // namespace segmentation_platform
