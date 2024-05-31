// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_
#define CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"

namespace chromecast {
class CastActivityUrlFilterManager;

class CastURLLoaderThrottleProvider : public blink::URLLoaderThrottleProvider {
 public:
  CastURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType type,
      CastActivityUrlFilterManager* url_filter_manager);
  ~CastURLLoaderThrottleProvider() override;
  CastURLLoaderThrottleProvider& operator=(
      const CastURLLoaderThrottleProvider&) = delete;

  // blink::URLLoaderThrottleProvider implementation:
  std::unique_ptr<blink::URLLoaderThrottleProvider> Clone() override;
  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      const network::ResourceRequest& request) override;
  void SetOnline(bool is_online) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  CastURLLoaderThrottleProvider(const CastURLLoaderThrottleProvider& other);

  blink::URLLoaderThrottleProviderType type_;
  CastActivityUrlFilterManager* const cast_activity_url_filter_manager_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_
