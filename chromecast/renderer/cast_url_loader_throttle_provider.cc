// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_url_loader_throttle_provider.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chromecast/common/activity_filtering_url_loader_throttle.h"
#include "chromecast/common/cors_exempt_headers.h"
#include "chromecast/renderer/cast_activity_url_filter_manager.h"
#include "chromecast/renderer/cast_url_rewrite_rules_store.h"
#include "components/url_rewrite/common/url_loader_throttle.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace chromecast {

CastURLLoaderThrottleProvider::CastURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType type,
    CastActivityUrlFilterManager* url_filter_manager,
    CastURLRewriteRulesStore* url_rewrite_rules_store)
    : type_(type),
      cast_activity_url_filter_manager_(url_filter_manager),
      url_rewrite_rules_store_(url_rewrite_rules_store) {
  DCHECK(cast_activity_url_filter_manager_);
  DCHECK(url_rewrite_rules_store_);
  DETACH_FROM_THREAD(thread_checker_);
}

CastURLLoaderThrottleProvider::~CastURLLoaderThrottleProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

CastURLLoaderThrottleProvider::CastURLLoaderThrottleProvider(
    const chromecast::CastURLLoaderThrottleProvider& other)
    : type_(other.type_),
      cast_activity_url_filter_manager_(
          other.cast_activity_url_filter_manager_),
      url_rewrite_rules_store_(other.url_rewrite_rules_store_) {
  DETACH_FROM_THREAD(thread_checker_);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
CastURLLoaderThrottleProvider::Clone() {
  return base::WrapUnique(new CastURLLoaderThrottleProvider(*this));
}

blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>>
CastURLLoaderThrottleProvider::CreateThrottles(
    int render_frame_id,
    const blink::WebURLRequest& request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;

  auto* activity_url_filter =
      cast_activity_url_filter_manager_->GetActivityUrlFilterForRenderFrameID(
          render_frame_id);
  if (activity_url_filter) {
    throttles.emplace_back(std::make_unique<ActivityFilteringURLLoaderThrottle>(
        activity_url_filter));
  }

  auto rules =
      url_rewrite_rules_store_->GetUrlRequestRewriteRules(render_frame_id);
  if (rules) {
    throttles.emplace_back(std::make_unique<url_rewrite::URLLoaderThrottle>(
        rules, base::BindRepeating(&IsCorsExemptHeader)));
  }

  return throttles;
}

void CastURLLoaderThrottleProvider::SetOnline(bool is_online) {}

}  // namespace chromecast
