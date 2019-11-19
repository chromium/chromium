// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

namespace {
// All platforms proxy for whether the simplified NTP is enabled.
bool IsSimplifiedNtpEnabled() {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif  // OS_ANDROID
}
}  // namespace

// Holds an experiment ID. So long as the feature is set through a server-side
// variations config, this feature should exist on the client. This ensures that
// the experiment ID is visible in chrome://snippets-internals.
const base::Feature kRemoteSuggestionsBackendFeature{
    "NTPRemoteSuggestionsBackend", base::FEATURE_DISABLED_BY_DEFAULT};

// Keep sorted, and keep nullptr at the end.
const base::Feature* const kAllFeatures[] = {
    &kArticleSuggestionsFeature, &kKeepPrefetchedContentSuggestions,
    &kNotificationsFeature, &kRemoteSuggestionsBackendFeature,
    &kOptionalImagesEnabledFeature};

const base::Feature kArticleSuggestionsFeature{
    "NTPArticleSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRemoteSuggestionsEmulateM58FetchingSchedule{
    "RemoteSuggestionsEmulateM58FetchingSchedule",
    base::FEATURE_DISABLED_BY_DEFAULT};

std::unique_ptr<CategoryRanker> BuildSelectedCategoryRanker(
    PrefService* pref_service,
    base::Clock* clock) {
  if (IsSimplifiedNtpEnabled()) {
    return std::make_unique<ConstantCategoryRanker>();
  }
  return std::make_unique<ClickBasedCategoryRanker>(pref_service, clock);
}

const base::Feature kNotificationsFeature = {"ContentSuggestionsNotifications",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const char kNotificationsPriorityParam[] = "priority";
const char kNotificationsTextParam[] = "text";
const char kNotificationsTextValuePublisher[] = "publisher";
const char kNotificationsTextValueSnippet[] = "snippet";
const char kNotificationsTextValueAndMore[] = "and_more";
const char kNotificationsKeepWhenFrontmostParam[] =
    "keep_notification_when_frontmost";
const char kNotificationsOpenToNTPParam[] = "open_to_ntp";
const char kNotificationsDailyLimit[] = "daily_limit";
const char kNotificationsIgnoredLimitParam[] = "ignored_limit";

const base::Feature kKeepPrefetchedContentSuggestions{
    "KeepPrefetchedContentSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOptionalImagesEnabledFeature{
    "NTPRemoteSuggestionsOptionalImages", base::FEATURE_ENABLED_BY_DEFAULT};

std::vector<const base::Feature*> GetAllFeatures() {
  // Skip the last feature as it's a nullptr.
  return std::vector<const base::Feature*>(
      kAllFeatures, kAllFeatures + base::size(kAllFeatures));
}

// Default referrer for the content suggestions.
const char kDefaultReferrerUrl[] =
    "https://www.googleapis.com/auth/chrome-content-suggestions";

// Provides ability to customize the referrer URL.
// When specifying a referrer through a field trial, it must contain a path.
// In case of default value above the path is empty, but it is specified.
const base::FeatureParam<std::string> kArticleSuggestionsReferrerURLParam{
    &kArticleSuggestionsFeature, "referrer_url", kDefaultReferrerUrl};

std::string GetContentSuggestionsReferrerURL() {
  return kArticleSuggestionsReferrerURLParam.Get();
}

}  // namespace ntp_snippets
