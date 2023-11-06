// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"

#include "base/notreached.h"

namespace optimization_guide {

// static
OptimizationGuideModelExecutionError
OptimizationGuideModelExecutionError::FromHttpStatusCode(
    net::HttpStatusCode response_code) {
  switch (response_code) {
    case net::HTTP_BAD_REQUEST:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kInvalidRequest);
    case net::HTTP_UNAUTHORIZED:
    case net::HTTP_PROXY_AUTHENTICATION_REQUIRED:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kPermissionDenied);
    case net::HTTP_TOO_MANY_REQUESTS:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kRequestThrottled);
    default:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kGenericFailure);
  }
}

// static
OptimizationGuideModelExecutionError
OptimizationGuideModelExecutionError::FromModelExecutionError(
    ModelExecutionError error) {
  return OptimizationGuideModelExecutionError(error);
}

OptimizationGuideModelExecutionError::OptimizationGuideModelExecutionError(
    ModelExecutionError error)
    : error_(error) {}

OptimizationGuideModelExecutionError::ModelExecutionError
OptimizationGuideModelExecutionError::error() const {
  return error_;
}

bool OptimizationGuideModelExecutionError::transient() const {
  switch (error_) {
    case ModelExecutionError::kInvalidRequest:
    case ModelExecutionError::kPermissionDenied:
      return false;
    case ModelExecutionError::kRequestThrottled:
    case ModelExecutionError::kGenericFailure:
      return true;
    case ModelExecutionError::kUnknown:
      NOTREACHED();
      return true;
  }
}

}  // namespace optimization_guide
