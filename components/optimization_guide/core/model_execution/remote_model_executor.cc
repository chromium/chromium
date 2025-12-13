// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/remote_model_executor.h"

namespace optimization_guide {

OptimizationGuideModelExecutionResult::OptimizationGuideModelExecutionResult() =
    default;

OptimizationGuideModelExecutionResult::OptimizationGuideModelExecutionResult(
    OptimizationGuideModelExecutionResult&& other) = default;

OptimizationGuideModelExecutionResult::
    ~OptimizationGuideModelExecutionResult() = default;

OptimizationGuideModelExecutionResult::OptimizationGuideModelExecutionResult(
    base::expected<const proto::Any /*response_metadata*/,
                   OptimizationGuideModelExecutionError> response,
    std::unique_ptr<proto::ModelExecutionInfo> execution_info)
    : response(response), execution_info(std::move(execution_info)) {}

}  // namespace optimization_guide
