// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_
#define CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/public/renderer/url_loader_throttle_provider.h"

namespace chromecast {

class CastURLLoaderThrottleProvider
    : public content::URLLoaderThrottleProvider {
 public:
  explicit CastURLLoaderThrottleProvider(
      content::URLLoaderThrottleProviderType type);
  ~CastURLLoaderThrottleProvider() override;

  // content::URLLoaderThrottleProvider implementation:
  std::unique_ptr<content::URLLoaderThrottleProvider> Clone() override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request,
      content::ResourceType resource_type) override;
  void SetOnline(bool is_online) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  CastURLLoaderThrottleProvider(const CastURLLoaderThrottleProvider& other);

  content::URLLoaderThrottleProviderType type_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_ASSIGN(CastURLLoaderThrottleProvider);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_
