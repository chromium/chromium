// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_url_loader_throttle_provider.h"

#include "base/feature_list.h"
#include "chromecast/common/cast_url_loader_throttle.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace chromecast {

CastURLLoaderThrottleProvider::CastURLLoaderThrottleProvider(
    content::URLLoaderThrottleProviderType type)
    : type_(type) {
  DETACH_FROM_THREAD(thread_checker_);
}

CastURLLoaderThrottleProvider::~CastURLLoaderThrottleProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

CastURLLoaderThrottleProvider::CastURLLoaderThrottleProvider(
    const chromecast::CastURLLoaderThrottleProvider& other)
    : type_(other.type_) {
  DETACH_FROM_THREAD(thread_checker_);
}

std::unique_ptr<content::URLLoaderThrottleProvider>
CastURLLoaderThrottleProvider::Clone() {
  return base::WrapUnique(new CastURLLoaderThrottleProvider(*this));
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CastURLLoaderThrottleProvider::CreateThrottles(
    int render_frame_id,
    const blink::WebURLRequest& request,
    content::ResourceType resource_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  return throttles;
}

void CastURLLoaderThrottleProvider::SetOnline(bool is_online) {}

}  // namespace chromecast
