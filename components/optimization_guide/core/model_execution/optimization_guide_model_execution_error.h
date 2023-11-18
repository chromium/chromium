// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_MODEL_EXECUTION_ERROR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_MODEL_EXECUTION_ERROR_H_

#include "components/optimization_guide/proto/model_execution.pb.h"
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
    // Retryable error occurred in server.
    kRetryableError,
    // Non-retryable error occurred in server.
    kNonRetryableError,
    // Unsupported language.
    kUnsupportedLanguage,
    // Request was filtered.
    kFiltered,
    // Response was disabled.
    kDisabled,
    // The request was cancelled.
    kCancelled,

    // Insert new values before this line.
    kMaxValue = kCancelled
  };

  static OptimizationGuideModelExecutionError FromHttpStatusCode(
      net::HttpStatusCode response_code);

  static OptimizationGuideModelExecutionError FromModelExecutionServerError(
      proto::ErrorResponse error);

  static OptimizationGuideModelExecutionError FromModelExecutionError(
      ModelExecutionError error);

  ModelExecutionError error() const;

  // Returns whether the error is transient and may succeed if the request was
  // retried.
  bool transient() const;

  // Returns whether model quality log entry should be added for the error.
  bool ShouldLogModelQuality() const;

 private:
  explicit OptimizationGuideModelExecutionError(ModelExecutionError error);

  const ModelExecutionError error_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_MODEL_EXECUTION_ERROR_H_
