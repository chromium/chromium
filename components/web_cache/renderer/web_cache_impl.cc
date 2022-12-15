// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_cache/renderer/web_cache_impl.h"

#include "third_party/blink/public/platform/web_cache.h"

namespace web_cache {

WebCacheImpl::WebCacheImpl() = default;
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
