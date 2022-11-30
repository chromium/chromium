// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_EXECUTION_STATUS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_EXECUTION_STATUS_H_

#include <string>

namespace optimization_guide {

// The status of a model execution. These values are logged to UMA histograms,
// do not change or reorder values. Make sure to update
// |OptimizationGuideExecutionStatus| in //tools/metrics/histograms/enums.xml.
enum class ExecutionStatus {
  // Status is unknown.
  kUnknown = 0,

  // Execution finished successfully.
  kSuccess = 1,

  // Execution is still pending.
  kPending = 2,

  // Execution failed because the model file is not available.
  kErrorModelFileNotAvailable = 3,

  // Execution failed because the model file could not be loaded into TFLite.
  kErrorModelFileNotValid = 4,

  // Execution failed because the input was empty or otherwise invalid.
  kErrorEmptyOrInvalidInput = 5,

  // Execution failed because of an unknown error.
  kErrorUnknown = 6,

  // Execution was cancelled. This can happen if the execution took too long to
  // finish and it was automatically cancelled after an experiment-controlled
  // timeout.
  kErrorCancelled = 7,

  kMaxValue = kErrorCancelled,
};

// Returns a string representation of |status|.
std::string ExecutionStatusToString(ExecutionStatus status);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_EXECUTION_STATUS_H_