// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_RENDERER_CAST_RUNTIME_URL_LOADER_THROTTLE_PROVIDER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_RENDERER_CAST_RUNTIME_URL_LOADER_THROTTLE_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"

namespace chromecast {

class CastRuntimeContentRendererClient;

// A provider of URLLoaderThrottle's for renderer process.
class CastRuntimeURLLoaderThrottleProvider
    : public blink::URLLoaderThrottleProvider {
 public:
  CastRuntimeURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType type,
      CastRuntimeContentRendererClient* renderer_client);
  ~CastRuntimeURLLoaderThrottleProvider() override;
  CastRuntimeURLLoaderThrottleProvider& operator=(
      const CastRuntimeURLLoaderThrottleProvider&) = delete;

  // blink::URLLoaderThrottleProvider implementation:
  std::unique_ptr<blink::URLLoaderThrottleProvider> Clone() override;
  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request) override;
  void SetOnline(bool is_online) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  CastRuntimeURLLoaderThrottleProvider(
      const CastRuntimeURLLoaderThrottleProvider& other);

  blink::URLLoaderThrottleProviderType type_;
  CastRuntimeContentRendererClient* const renderer_client_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_RENDERER_CAST_RUNTIME_URL_LOADER_THROTTLE_PROVIDER_H_
