// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_url_loader_throttle_provider.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "chromecast/common/activity_filtering_url_loader_throttle.h"
#include "chromecast/common/cast_url_loader_throttle.h"
#include "chromecast/common/identification_settings_manager.h"
#include "chromecast/renderer/cast_activity_url_filter_manager.h"
#include "chromecast/renderer/identification_settings_manager_store.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace chromecast {

CastURLLoaderThrottleProvider::CastURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType type,
    CastActivityUrlFilterManager* url_filter_manager,
    shell::IdentificationSettingsManagerStore* settings_manager_store)
    : type_(type),
      cast_activity_url_filter_manager_(url_filter_manager),
      settings_manager_store_(settings_manager_store) {
  DCHECK(cast_activity_url_filter_manager_);
  DCHECK(settings_manager_store_);
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
      settings_manager_store_(other.settings_manager_store_) {
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

  auto settings_manager =
      settings_manager_store_->GetSettingsManagerFromRenderFrameID(
          render_frame_id);
  if (settings_manager) {
    throttles.emplace_back(std::make_unique<CastURLLoaderThrottle>(
        settings_manager, std::string() /* session_id */));
  } else {
    LOG(WARNING) << "No settings manager found for render frame: "
                 << render_frame_id;
  }
  return throttles;
}

void CastURLLoaderThrottleProvider::SetOnline(bool is_online) {}

}  // namespace chromecast
