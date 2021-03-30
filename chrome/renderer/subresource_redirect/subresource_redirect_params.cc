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

}  // namespace

url::Origin GetSubresourceRedirectOrigin() {
  auto lite_page_subresource_origin = base::GetFieldTrialParamValueByFeature(
      blink::features::kSubresourceRedirect, "lite_page_subresource_origin");
  if (lite_page_subresource_origin.empty())
    return url::Origin::Create(GURL("https://litepages.googlezip.net/"));
  return url::Origin::Create(GURL(lite_page_subresource_origin));
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

size_t GetFirstKSubresourceLimit() {
  return base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kSubresourceRedirect, "first_k_subresource_limit", 0);
}

base::TimeDelta GetRobotsRulesReceiveFirstKSubresourceTimeout() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kSubresourceRedirect,
          "robots_rules_receive_first_k_timeout_ms", 10));
}

int MaxRobotsRulesParsersCacheSize() {
  return base::GetFieldTrialParamByFeatureAsInt(
      blink::features::kSubresourceRedirect,
      "max_robots_rules_parsers_cache_size", 20);
}

bool ShouldRecordLoginRobotsUkmMetrics() {
  return base::GetFieldTrialParamByFeatureAsBool(
      blink::features::kSubresourceRedirect, "record_login_robots_ukm_metrics",
      true);
}

}  // namespace subresource_redirect
