// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_CACHE_RENDERER_WEB_CACHE_IMPL_H_
#define COMPONENTS_WEB_CACHE_RENDERER_WEB_CACHE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/web_cache/public/mojom/web_cache.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace web_cache {

// This class implements the Mojo interface mojom::WebCache.
class WebCacheImpl : public mojom::WebCache {
 public:
  WebCacheImpl();
  ~WebCacheImpl() override;

  void BindReceiver(mojo::PendingReceiver<mojom::WebCache> web_cache_receiver);

  // Needs to be called by RenderViews in case of navigations to execute
  // any 'clear cache' commands that were delayed until the next navigation.
  void ExecutePendingClearCache();

 private:
  enum State {
    kInit,
    kNavigate_Pending,
    kClearCache_Pending,
  };

  // mojom::WebCache methods:
  void SetCacheCapacity(uint64_t capacity) override;
  // If |on_navigation| is true, the clearing is delayed until the next
  // navigation event.
  void ClearCache(bool on_navigation) override;

  // Records status regarding the sequence of navigation event and
  // ClearCache(true) call, to ensure delayed 'clear cache' command always
  // get executed on navigation.
  State clear_cache_state_ = kInit;

  mojo::ReceiverSet<mojom::WebCache> receivers_;

  DISALLOW_COPY_AND_ASSIGN(WebCacheImpl);
};

}  // namespace web_cache

#endif  // COMPONENTS_WEB_CACHE_RENDERER_WEB_CACHE_IMPL_H_
