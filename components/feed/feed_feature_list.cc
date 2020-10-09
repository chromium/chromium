// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/feed_feature_list.h"
#include "components/feed/buildflags.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace feed {

const base::Feature kInterestFeedContentSuggestions{
    "InterestFeedContentSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};
// InterestFeedV2 takes precedence over InterestFeedContentSuggestions.
// InterestFeedV2 is cached in ChromeCachedFlags. If the default value here is
// changed, please update the cached one's default value in CachedFeatureFlags.
const base::Feature kInterestFeedV2{"InterestFeedV2",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<std::string> kDisableTriggerTypes{
    &kInterestFeedContentSuggestions, "disable_trigger_types", ""};
const base::FeatureParam<int> kSuppressRefreshDurationMinutes{
    &kInterestFeedContentSuggestions, "suppress_refresh_duration_minutes", 30};
const base::FeatureParam<int> kTimeoutDurationSeconds{
    &kInterestFeedContentSuggestions, "timeout_duration_seconds", 30};
const base::FeatureParam<bool> kThrottleBackgroundFetches{
    &kInterestFeedContentSuggestions, "throttle_background_fetches", true};
const base::FeatureParam<bool> kOnlySetLastRefreshAttemptOnSuccess{
    &kInterestFeedContentSuggestions,
    "only_set_last_refresh_attempt_on_success", true};

const base::Feature kInterestFeedFeedback{"InterestFeedFeedback",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReportFeedUserActions{"ReportFeedUserActions",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Determines whether conditions should be reached before enabling the upload of
// click and view actions in the feed (e.g., the user needs to view X cards).
// For example, This is needed when the notice card is at the second position in
// the feed.
const base::Feature kInterestFeedV1ClicksAndViewsConditionalUpload{
    "InterestFeedV1ClickAndViewActionsConditionalUpload",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kInterestFeedV2ClicksAndViewsConditionalUpload{
    "InterestFeedV2ClickAndViewActionsConditionalUpload",
    base::FEATURE_DISABLED_BY_DEFAULT};

const char kDefaultReferrerUrl[] =
    "https://www.googleapis.com/auth/chrome-content-suggestions";

std::string GetFeedReferrerUrl() {
  const base::Feature* feature = base::FeatureList::IsEnabled(kInterestFeedV2)
                                     ? &kInterestFeedV2
                                     : &kInterestFeedContentSuggestions;
  std::string referrer =
      base::GetFieldTrialParamValueByFeature(*feature, "referrer_url");
  return referrer.empty() ? kDefaultReferrerUrl : referrer;
}

// Chrome can be built with or without v1 or v2.
// If both are built-in, use kInterestFeedV2 to decide whether v2 is used.
// Otherwise use the version available.
bool IsV2Enabled() {
#if BUILDFLAG(ENABLE_FEED_V1) && BUILDFLAG(ENABLE_FEED_V2)
  return base::FeatureList::IsEnabled(feed::kInterestFeedV2);
#elif BUILDFLAG(ENABLE_FEED_V1)
  return false;
#else
  return true;
#endif
}

bool IsV1Enabled() {
  return !IsV2Enabled();
}

}  // namespace feed
