// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_model_executor.h"

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

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult() = default;

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult(
        base::expected<const StreamingResponse,
                       OptimizationGuideModelExecutionError> response,
        bool provided_by_on_device,
        std::unique_ptr<ModelQualityLogEntry> log_entry,
        std::unique_ptr<proto::ModelExecutionInfo> execution_info)
    : response(response),
      provided_by_on_device(provided_by_on_device),
      log_entry(std::move(log_entry)),
      execution_info(std::move(execution_info)) {}

OptimizationGuideModelStreamingExecutionResult::
    ~OptimizationGuideModelStreamingExecutionResult() = default;

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult(
        OptimizationGuideModelStreamingExecutionResult&& src) = default;

}  // namespace optimization_guide
