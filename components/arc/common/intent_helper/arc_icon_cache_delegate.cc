// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/intent_helper/arc_icon_cache_delegate.h"

#include "base/logging.h"

namespace arc {

namespace {
ArcIconCacheDelegateProvider* g_delegate_provider = nullptr;
}

// static
ArcIconCacheDelegate* ArcIconCacheDelegate::GetInstance() {
  if (!g_delegate_provider) {
    return nullptr;
  }
  return g_delegate_provider->GetInstance();
}

ArcIconCacheDelegate::~ArcIconCacheDelegate() = default;

ArcIconCacheDelegateProvider::ArcIconCacheDelegateProvider(
    ArcIconCacheDelegate* delegate)
    : delegate_(delegate) {
  if (g_delegate_provider) {
    LOG(ERROR) << "Overwriting g_delegate_provider. "
               << "This should not happend except for testing.";
  }
  g_delegate_provider = this;
}

ArcIconCacheDelegateProvider::~ArcIconCacheDelegateProvider() {
  if (g_delegate_provider != this) {
    LOG(ERROR) << "g_delegate_provider was not properly set. "
               << "This should not happend except for testing.";
  }
  g_delegate_provider = nullptr;
}

ArcIconCacheDelegate* ArcIconCacheDelegateProvider::GetInstance() {
  return delegate_;
}

}  // namespace arc
