// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_types.h"

#include <utility>

namespace personal_context {

FetchContextResult::FetchContextResult() = default;

FetchContextResult::FetchContextResult(
    base::expected<const proto::Any /*response_metadata*/, ContextMemoryError>
        response)
    : response(std::move(response)) {}

FetchContextResult::FetchContextResult(FetchContextResult&& other) = default;

FetchContextResult::~FetchContextResult() = default;

FetchPiiEntitiesResult::FetchPiiEntitiesResult() = default;

FetchPiiEntitiesResult::FetchPiiEntitiesResult(
    base::expected<const proto::FetchPiiEntitiesResponse, ContextMemoryError>
        response)
    : response(std::move(response)) {}

FetchPiiEntitiesResult::FetchPiiEntitiesResult(FetchPiiEntitiesResult&& other) =
    default;

FetchPiiEntitiesResult::~FetchPiiEntitiesResult() = default;

}  // namespace personal_context
