// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace subresource_redirect {

namespace {

// Default timeout for the hints to be received from the time navigation starts.
const int64_t kHintsReceiveDefaultTimeoutSeconds = 5;

bool IsSubresourceRedirectEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect);
}
}  // namespace

url::Origin GetSubresourceRedirectOrigin() {
  auto lite_page_subresource_origin = base::GetFieldTrialParamValueByFeature(
      blink::features::kSubresourceRedirect, "lite_page_subresource_origin");
  if (lite_page_subresource_origin.empty())
    return url::Origin::Create(GURL("https://litepages.googlezip.net/"));
  return url::Origin::Create(GURL(lite_page_subresource_origin));
}

bool IsPublicImageHintsBasedCompressionEnabled() {
  return IsSubresourceRedirectEnabled() &&
         base::GetFieldTrialParamByFeatureAsBool(
             blink::features::kSubresourceRedirect,
             "enable_public_image_hints_based_compression", true);
}

bool ShouldCompressionServerRedirectSubresource() {
  return base::GetFieldTrialParamByFeatureAsBool(
      blink::features::kSubresourceRedirect,
      "enable_subresource_server_redirect", true);
}

base::TimeDelta GetCompressionRedirectTimeout() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kSubresourceRedirect, "subresource_redirect_timeout",
          5000));
}

int64_t GetHintsReceiveTimeout() {
  return base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kSubresourceRedirect, "hints_receive_timeout",
      kHintsReceiveDefaultTimeoutSeconds);
}

base::TimeDelta GetRobotsRulesReceiveTimeout() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kSubresourceRedirect, "robots_rules_receive_timeout",
          10));
}

}  // namespace subresource_redirect
