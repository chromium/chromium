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

//  static
OptimizationGuideModelExecutionError
OptimizationGuideModelExecutionError::FromModelExecutionServerError(
    proto::ErrorResponse error) {
  switch (error.error_state()) {
    case proto::ErrorState::ERROR_STATE_UNSPECIFIED:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kGenericFailure);
    case proto::ErrorState::ERROR_STATE_INTERNAL_SERVER_ERROR_RETRY:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kRetryableError);
    case proto::ErrorState::ERROR_STATE_INTERNAL_SERVER_ERROR_NO_RETRY:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kNonRetryableError);
    case proto::ErrorState::ERROR_STATE_UNSUPPORTED_LANGUAGE:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kUnsupportedLanguage);
    case proto::ErrorState::ERROR_STATE_FILTERED:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kFiltered);
    case proto::ErrorState::ERROR_STATE_REQUEST_THROTTLED:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kRequestThrottled);
    case proto::ErrorState::ERROR_STATE_DISABLED:
      return OptimizationGuideModelExecutionError(
          ModelExecutionError::kDisabled);
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
    case ModelExecutionError::kNonRetryableError:
    case ModelExecutionError::kUnsupportedLanguage:
    case ModelExecutionError::kFiltered:
    case ModelExecutionError::kDisabled:
      return false;
    case ModelExecutionError::kRequestThrottled:
    case ModelExecutionError::kGenericFailure:
    case ModelExecutionError::kRetryableError:
    case ModelExecutionError::kCancelled:
      return true;
    case ModelExecutionError::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return true;
  }
}

bool OptimizationGuideModelExecutionError::ShouldLogModelQuality() const {
  switch (error_) {
    case ModelExecutionError::kFiltered:
    case ModelExecutionError::kUnsupportedLanguage:
    case ModelExecutionError::kNonRetryableError:
    case ModelExecutionError::kRetryableError:
      return true;
    case ModelExecutionError::kInvalidRequest:
    case ModelExecutionError::kPermissionDenied:
    case ModelExecutionError::kDisabled:
    case ModelExecutionError::kRequestThrottled:
    case ModelExecutionError::kGenericFailure:
    case ModelExecutionError::kCancelled:
      return false;
    case ModelExecutionError::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

}  // namespace optimization_guide
