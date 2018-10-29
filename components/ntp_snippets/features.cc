// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/clock.h"
#include "components/ntp_snippets/category_rankers/click_based_category_ranker.h"
#include "components/ntp_snippets/category_rankers/constant_category_ranker.h"
#include "components/variations/variations_associated_data.h"

namespace ntp_snippets {

// Holds an experiment ID. So long as the feature is set through a server-side
// variations config, this feature should exist on the client. This ensures that
// the experiment ID is visible in chrome://snippets-internals.
const base::Feature kRemoteSuggestionsBackendFeature{
    "NTPRemoteSuggestionsBackend", base::FEATURE_DISABLED_BY_DEFAULT};

// Keep sorted, and keep nullptr at the end.
const base::Feature* const kAllFeatures[] = {
    &kArticleSuggestionsFeature,
    &kBookmarkSuggestionsFeature,
    &kBreakingNewsPushFeature,
    &kCategoryOrder,
    &kCategoryRanker,
    &kContentSuggestionsDebugLog,
    &kIncreasedVisibility,
    &kKeepPrefetchedContentSuggestions,
    &kNotificationsFeature,
    &kPublisherFaviconsFromNewServerFeature,
    &kRemoteSuggestionsBackendFeature};

const base::Feature kArticleSuggestionsFeature{
    "NTPArticleSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBookmarkSuggestionsFeature{
    "NTPBookmarkSuggestions", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kIncreasedVisibility{"NTPSnippetsIncreasedVisibility",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kBreakingNewsPushFeature{"BreakingNewsPush",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCategoryRanker{"ContentSuggestionsCategoryRanker",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPublisherFaviconsFromNewServerFeature{
    "ContentSuggestionsFaviconsFromNewServer",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRemoteSuggestionsEmulateM58FetchingSchedule{
    "RemoteSuggestionsEmulateM58FetchingSchedule",
    base::FEATURE_DISABLED_BY_DEFAULT};

const char kCategoryRankerParameter[] = "category_ranker";
const char kCategoryRankerConstantRanker[] = "constant";
const char kCategoryRankerClickBasedRanker[] = "click_based";

CategoryRankerChoice GetSelectedCategoryRanker(bool is_chrome_home_enabled) {
  std::string category_ranker_value =
      variations::GetVariationParamValueByFeature(kCategoryRanker,
                                                  kCategoryRankerParameter);

  if (category_ranker_value.empty()) {
    // Default, Enabled or Disabled.
    if (is_chrome_home_enabled) {
      return CategoryRankerChoice::CONSTANT;
    }
    return CategoryRankerChoice::CLICK_BASED;
  }
  if (category_ranker_value == kCategoryRankerConstantRanker) {
    return CategoryRankerChoice::CONSTANT;
  }
  if (category_ranker_value == kCategoryRankerClickBasedRanker) {
    return CategoryRankerChoice::CLICK_BASED;
  }

  LOG(DFATAL) << "The " << kCategoryRankerParameter << " parameter value is '"
              << category_ranker_value << "'";
  return CategoryRankerChoice::CONSTANT;
}

std::unique_ptr<CategoryRanker> BuildSelectedCategoryRanker(
    PrefService* pref_service,
    base::Clock* clock,
    bool is_chrome_home_enabled) {
  CategoryRankerChoice choice =
      ntp_snippets::GetSelectedCategoryRanker(is_chrome_home_enabled);

  switch (choice) {
    case CategoryRankerChoice::CONSTANT:
      return std::make_unique<ConstantCategoryRanker>();
    case CategoryRankerChoice::CLICK_BASED:
      return std::make_unique<ClickBasedCategoryRanker>(pref_service, clock);
  }
  return nullptr;
}

const base::Feature kCategoryOrder{"ContentSuggestionsCategoryOrder",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const char kCategoryOrderParameter[] = "category_order";
const char kCategoryOrderGeneral[] = "general";
const char kCategoryOrderEmergingMarketsOriented[] =
    "emerging_markets_oriented";

CategoryOrderChoice GetSelectedCategoryOrder() {
  if (!base::FeatureList::IsEnabled(kCategoryOrder)) {
    return CategoryOrderChoice::GENERAL;
  }

  std::string category_order_value =
      variations::GetVariationParamValueByFeature(kCategoryOrder,
                                                  kCategoryOrderParameter);

  if (category_order_value.empty()) {
    // Enabled with no parameters.
    return CategoryOrderChoice::EMERGING_MARKETS_ORIENTED;
  }
  if (category_order_value == kCategoryOrderGeneral) {
    return CategoryOrderChoice::GENERAL;
  }
  if (category_order_value == kCategoryOrderEmergingMarketsOriented) {
    return CategoryOrderChoice::EMERGING_MARKETS_ORIENTED;
  }

  LOG(DFATAL) << "The " << kCategoryOrderParameter << " parameter value is '"
              << category_order_value << "'";
  return CategoryOrderChoice::GENERAL;
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
    "KeepPrefetchedContentSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContentSuggestionsDebugLog{
    "ContentSuggestionsDebugLog", base::FEATURE_DISABLED_BY_DEFAULT};

std::vector<const base::Feature*> GetAllFeatures() {
  // Skip the last feature as it's a nullptr.
  return std::vector<const base::Feature*>(
      kAllFeatures, kAllFeatures + arraysize(kAllFeatures));
}

// Default referrer for the content suggestions.
const char kDefaultReferrerUrl[] = "https://discover.google.com/";

// Provides ability to customize the referrer URL.
// When specifying a referrer through a field trial, it must contain a path.
// In case of default value above the path is empty, but it is specified.
base::FeatureParam<std::string> kArticleSuggestionsReferrerURLParam{
    &kArticleSuggestionsFeature, "referrer_url", kDefaultReferrerUrl};

std::string GetContentSuggestionsReferrerURL() {
  return kArticleSuggestionsReferrerURLParam.Get();
}

}  // namespace ntp_snippets
