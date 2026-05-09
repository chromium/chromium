// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/context_memory_error.h"

#include "base/notreached.h"

namespace personal_context {

ContextMemoryError::ContextMemoryError(ExecutionError error) : error_(error) {}

// static
ContextMemoryError ContextMemoryError::FromHttpStatusCode(
    net::HttpStatusCode response_code) {
  switch (response_code) {
    case net::HTTP_BAD_REQUEST:
      return ContextMemoryError(ExecutionError::kInvalidRequest);
    case net::HTTP_UNAUTHORIZED:
    case net::HTTP_PROXY_AUTHENTICATION_REQUIRED:
      return ContextMemoryError(ExecutionError::kPermissionDenied);
    case net::HTTP_TOO_MANY_REQUESTS:
      return ContextMemoryError(ExecutionError::kRequestThrottled);
    default:
      return ContextMemoryError(ExecutionError::kGenericFailure);
  }
}

ContextMemoryError::ExecutionError ContextMemoryError::error() const {
  return error_;
}

bool ContextMemoryError::transient() const {
  switch (error_) {
    case ExecutionError::kInvalidRequest:
    case ExecutionError::kPermissionDenied:
    case ExecutionError::kNonRetryableError:
      return false;
    case ExecutionError::kRequestThrottled:
    case ExecutionError::kGenericFailure:
    case ExecutionError::kRetryableError:
      return true;
    case ExecutionError::kUnknown:
      NOTREACHED();
  }
}

}  // namespace personal_context
