// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/lite_video/lite_video_util.h"

#include "chrome/common/chrome_features.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"

namespace lite_video {

bool IsLiteVideoEnabled() {
  return base::FeatureList::IsEnabled(features::kLiteVideo) &&
         blink::WebNetworkStateNotifier::SaveDataEnabled();
}

bool ShouldDisableLiteVideoForCacheControlNoTransform() {
  return base::GetFieldTrialParamByFeatureAsBool(
      ::features::kLiteVideo, "disable_for_cache_control_no_transform", false);
}

bool ShouldThrottleLiteVideoMissingContentLength() {
  return base::GetFieldTrialParamByFeatureAsBool(
      ::features::kLiteVideo, "throttle_missing_content_length", false);
}

size_t GetMaxActiveThrottles() {
  return base::GetFieldTrialParamByFeatureAsInt(::features::kLiteVideo,
                                                "max_active_throttles", 50);
}

}  // namespace lite_video
