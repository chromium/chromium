// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_model_executor.h"

namespace optimization_guide {

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult() = default;

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult(
        base::expected<const StreamingResponse,
                       OptimizationGuideModelExecutionError> response,
        bool provided_by_on_device,
        std::unique_ptr<ModelQualityLogEntry> log_entry)
    : response(response),
      provided_by_on_device(provided_by_on_device),
      log_entry(std::move(log_entry)) {}

OptimizationGuideModelStreamingExecutionResult::
    ~OptimizationGuideModelStreamingExecutionResult() = default;

OptimizationGuideModelStreamingExecutionResult::
    OptimizationGuideModelStreamingExecutionResult(
        OptimizationGuideModelStreamingExecutionResult&& src) = default;

}  // namespace optimization_guide
