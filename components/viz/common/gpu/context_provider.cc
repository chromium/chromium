// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/context_provider.h"

#include <utility>

namespace viz {

ContextProvider::ScopedContextLock::ScopedContextLock(
    ContextProvider* context_provider)
    : context_provider_(context_provider),
      context_lock_(*context_provider_->GetLock()) {
  busy_ = context_provider_->CacheController()->ClientBecameBusy();
}

ContextProvider::ScopedContextLock::~ScopedContextLock() {
  // Let ContextCacheController know we are no longer busy.
  context_provider_->CacheController()->ClientBecameNotBusy(std::move(busy_));
}
}  // namespace viz
