// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_CONTEXT_MEMORY_ERROR_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_CONTEXT_MEMORY_ERROR_H_

#include "net/http/http_status_code.h"

namespace personal_context {

// Contains the error details of the context memory request.
class ContextMemoryError {
 public:
  enum class ExecutionError {
    kUnknown = 0,
    // The request was invalid.
    kInvalidRequest = 1,
    // The request was throttled.
    kRequestThrottled = 2,
    // User permission errors such as not signed-in or not allowed
    kPermissionDenied = 3,
    // Other generic failures.
    kGenericFailure = 4,
    // Retryable error occurred in server.
    kRetryableError = 5,
    // Non-retryable error occurred in server.
    kNonRetryableError = 6,
    kMaxValue = kNonRetryableError,
  };

  static ContextMemoryError FromHttpStatusCode(
      net::HttpStatusCode response_code);
  ExecutionError error() const;
  // Returns whether the error is transient and may succeed if the request was
  // retried.
  bool transient() const;

 private:
  explicit ContextMemoryError(ExecutionError error);
  ExecutionError error_;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_CONTEXT_MEMORY_ERROR_H_
