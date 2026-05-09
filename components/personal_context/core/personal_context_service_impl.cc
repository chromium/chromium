// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_service_impl.h"

#include "base/functional/callback.h"

namespace personal_context {

PersonalContextServiceImpl::PersonalContextServiceImpl() = default;

PersonalContextServiceImpl::~PersonalContextServiceImpl() = default;

void PersonalContextServiceImpl::FetchContext(
    proto::ContextMemoryFeature feature,
    const google::protobuf::MessageLite& request_metadata,
    const ContextMemoryRequestOptions& options,
    FetchContextCallback callback) {
  // TODO(b/509659074): Implement FetchContext.
}

}  // namespace personal_context
