// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_MODEL_EXECUTION_ERROR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_MODEL_EXECUTION_ERROR_H_

#include "net/http/http_status_code.h"

namespace optimization_guide {

// Contains the error details of model execution.
class OptimizationGuideModelExecutionError {
 public:
  enum class ModelExecutionError {
    kUnknown,
    // The request was invalid.
    kInvalidRequest,
    // The request was throttled.
    kRequestThrottled,
    // User permission errors such as not signed-in or not allowed to execute
    // model.
    kPermissionDenied,
    // Other generic failures.
    kGenericFailure,

    // Insert new values before this line.
    kMaxValue = kGenericFailure
  };

  static OptimizationGuideModelExecutionError FromHttpStatusCode(
      net::HttpStatusCode response_code);

  static OptimizationGuideModelExecutionError FromModelExecutionError(
      ModelExecutionError error);

  ModelExecutionError error() const;

  // Returns whether the error is transient and may succeed if the request was
  // retried.
  bool transient() const;

 private:
  explicit OptimizationGuideModelExecutionError(ModelExecutionError error);

  const ModelExecutionError error_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_MODEL_EXECUTION_ERROR_H_
