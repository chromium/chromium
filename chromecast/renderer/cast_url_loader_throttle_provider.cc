// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_url_loader_throttle_provider.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chromecast/common/activity_filtering_url_loader_throttle.h"
#include "chromecast/renderer/cast_activity_url_filter_manager.h"
#include "components/url_rewrite/common/url_loader_throttle.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace chromecast {

CastURLLoaderThrottleProvider::CastURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType type,
    CastActivityUrlFilterManager* url_filter_manager)
    : type_(type), cast_activity_url_filter_manager_(url_filter_manager) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CastURLLoaderThrottleProvider::~CastURLLoaderThrottleProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CastURLLoaderThrottleProvider::CastURLLoaderThrottleProvider(
    const chromecast::CastURLLoaderThrottleProvider& other)
    : type_(other.type_),
      cast_activity_url_filter_manager_(
          other.cast_activity_url_filter_manager_) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
CastURLLoaderThrottleProvider::Clone() {
  return base::WrapUnique(new CastURLLoaderThrottleProvider(*this));
}

blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>
CastURLLoaderThrottleProvider::CreateThrottles(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    const network::ResourceRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;

  if (cast_activity_url_filter_manager_ && local_frame_token.has_value()) {
    auto* activity_url_filter = cast_activity_url_filter_manager_
                                    ->GetActivityUrlFilterForRenderFrameToken(
                                        local_frame_token.value());
    if (activity_url_filter) {
      throttles.emplace_back(
          std::make_unique<ActivityFilteringURLLoaderThrottle>(
              activity_url_filter));
    }
  }

  return throttles;
}

void CastURLLoaderThrottleProvider::SetOnline(bool is_online) {}

}  // namespace chromecast
