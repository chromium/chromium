// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_TYPES_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_TYPES_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/proto/context_memory_service.pb.h"

namespace personal_context {

// Defines the result of a PersonalContextService::FetchContext operation.
struct FetchContextResult {
  FetchContextResult();
  explicit FetchContextResult(
      base::expected<const proto::Any /*response_metadata*/, ContextMemoryError>
          response);
  FetchContextResult(FetchContextResult&& other);
  ~FetchContextResult();

  // The server response, containing either the feature-specific metadata
  // (originally packed in an Any proto) or a ContextMemoryError.
  base::expected<const proto::Any /*response_metadata*/, ContextMemoryError>
      response;
};

// Callback for receiving the result of a FetchContext call.
using FetchContextCallback = base::OnceCallback<void(FetchContextResult)>;

// Optional parameters for PersonalContextService::FetchContext
struct ContextMemoryRequestOptions {
  // Sets the X-Server-Timeout header of the HTTP request
  std::optional<base::TimeDelta> request_timeout;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_TYPES_H_
