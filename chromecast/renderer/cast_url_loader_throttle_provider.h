// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_
#define CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"

namespace chromecast {
class CastActivityUrlFilterManager;

namespace shell {
class IdentificationSettingsManagerStore;
}  // namespace shell

class CastURLLoaderThrottleProvider : public blink::URLLoaderThrottleProvider {
 public:
  CastURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType type,
      CastActivityUrlFilterManager* url_filter_manager,
      shell::IdentificationSettingsManagerStore* settings_manager_store);
  ~CastURLLoaderThrottleProvider() override;
  CastURLLoaderThrottleProvider& operator=(
      const CastURLLoaderThrottleProvider&) = delete;

  // blink::URLLoaderThrottleProvider implementation:
  std::unique_ptr<blink::URLLoaderThrottleProvider> Clone() override;
  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request) override;
  void SetOnline(bool is_online) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  CastURLLoaderThrottleProvider(const CastURLLoaderThrottleProvider& other);

  blink::URLLoaderThrottleProviderType type_;
  CastActivityUrlFilterManager* const cast_activity_url_filter_manager_;
  shell::IdentificationSettingsManagerStore* const settings_manager_store_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_URL_LOADER_THROTTLE_PROVIDER_H_
