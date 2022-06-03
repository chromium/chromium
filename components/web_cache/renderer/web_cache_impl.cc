// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_cache/renderer/web_cache_impl.h"

#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
#include "components/web_cache/public/features.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/web_cache.h"

namespace web_cache {

WebCacheImpl::WebCacheImpl() {
  // The cache implementation is a blink::MemoryCache, which already owns a
  // memory pressure listener. This listener is only enabled for low end devices
  // (with 512MB or less of RAM), so for this type of device this memory
  // pressure listener is redundant as we'll clear the cache twice. Preventing
  // this would require exposing the |kTrimWebCacheOnMemoryPressureOnly| as a
  // blink feature and this adds too much overhead for the sake of this
  // experiment. If this feature gets enabled by default this should be
  // optimized (the low end device check should be removed from
  // blink::MemoryCache and this listener should be removed).
  if (base::FeatureList::IsEnabled(kTrimWebCacheOnMemoryPressureOnly)) {
    memory_pressure_listener_.emplace(
        FROM_HERE,
        base::BindRepeating(
            [](WebCacheImpl* cache_impl,
               base::MemoryPressureListener::MemoryPressureLevel level) {
              if (level == base::MemoryPressureListener::MemoryPressureLevel::
                               MEMORY_PRESSURE_LEVEL_CRITICAL) {
                cache_impl->ClearCache(false);
              }
            },
            // Using unretained is safe because the memory pressure listener is
            // owned by this object and so this can't cause an use-after-free.
            base::Unretained(this)));
  }
}

WebCacheImpl::~WebCacheImpl() = default;

void WebCacheImpl::BindReceiver(
    mojo::PendingReceiver<mojom::WebCache> web_cache_receiver) {
  receivers_.Add(this, std::move(web_cache_receiver));
}

void WebCacheImpl::ExecutePendingClearCache() {
  switch (clear_cache_state_) {
    case kInit:
      clear_cache_state_ = kNavigate_Pending;
      break;
    case kNavigate_Pending:
      break;
    case kClearCache_Pending:
      blink::WebCache::Clear();
      clear_cache_state_ = kInit;
      break;
  }
}

void WebCacheImpl::SetCacheCapacity(uint64_t capacity64) {
  size_t capacity = base::checked_cast<size_t>(capacity64);

  blink::WebCache::SetCapacity(capacity);
}

void WebCacheImpl::ClearCache(bool on_navigation) {
  if (!on_navigation) {
    blink::WebCache::Clear();
    return;
  }

  switch (clear_cache_state_) {
    case kInit:
      clear_cache_state_ = kClearCache_Pending;
      break;
    case kNavigate_Pending:
      blink::WebCache::Clear();
      clear_cache_state_ = kInit;
      break;
    case kClearCache_Pending:
      break;
  }
}

}  // namespace web_cache
