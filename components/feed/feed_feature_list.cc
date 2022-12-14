// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/feed_feature_list.h"
#include "base/time/time.h"
#include "components/feed/buildflags.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/sync/base/features.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace feed {

BASE_FEATURE(kInterestFeedContentSuggestions,
             "InterestFeedContentSuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);
// InterestFeedV2 takes precedence over InterestFeedContentSuggestions.
// InterestFeedV2 is cached in ChromeCachedFlags. If the default value here is
// changed, please update the cached one's default value in CachedFeatureFlags.
BASE_FEATURE(kInterestFeedV2,
             "InterestFeedV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kInterestFeedV2Autoplay,
             "InterestFeedV2Autoplay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInterestFeedV2Hearts,
             "InterestFeedV2Hearts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInterestFeedV2Scrolling,
             "InterestFeedV2Scrolling",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kInterestFeedV1ClicksAndViewsConditionalUpload,
             "InterestFeedV1ClickAndViewActionsConditionalUpload",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kInterestFeedV2ClicksAndViewsConditionalUpload,
             "InterestFeedV2ClickAndViewActionsConditionalUpload",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kInterestFeedNoticeCardAutoDismiss,
             "InterestFeedNoticeCardAutoDismiss",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kWebFeed, "WebFeed", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDiscoFeedEndpoint,
             "DiscoFeedEndpoint",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kXsurfaceMetricsReporting,
             "XsurfaceMetricsReporting",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kReliabilityLogging,
             "FeedReliabilityLogging",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeedHeaderStickToTop,
             "FeedHeaderStickToTop",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedInteractiveRefresh,
             "FeedInteractiveRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeedLoadingPlaceholder,
             "FeedLoadingPlaceholder",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool>
    kEnableFeedLoadingPlaceholderAnimationOnInstantStart{
        &kFeedLoadingPlaceholder, "enable_animation_on_instant_start", false};
BASE_FEATURE(kFeedImageMemoryCacheSizePercentage,
             "FeedImageMemoryCacheSizePercentage",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedClearImageMemoryCache,
             "FeedClearImageMemoryCache",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedBackToTop,
             "FeedBackToTop",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedStamp, "FeedStamp", base::FEATURE_DISABLED_BY_DEFAULT);

const char kDefaultReferrerUrl[] = "https://www.google.com/";

BASE_FEATURE(kWebFeedAwareness,
             "WebFeedAwareness",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedOnboarding,
             "WebFeedOnboarding",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedSort, "WebFeedSort", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableOpenInNewTabFromStartSurfaceFeed,
             "EnableOpenInNewTabFromStartSurfaceFeed",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebUiFeed, "FeedWebUi", base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kWebUiFeedUrl{
    &kWebUiFeed, "feedurl", "https://www.google.com/feed-api/following"};
const base::FeatureParam<bool> kWebUiDisableContentSecurityPolicy{
    &kWebUiFeed, "disableCsp", false};

std::string GetFeedReferrerUrl() {
  const base::Feature* feature = base::FeatureList::IsEnabled(kInterestFeedV2)
                                     ? &kInterestFeedV2
                                     : &kInterestFeedContentSuggestions;
  std::string referrer =
      base::GetFieldTrialParamValueByFeature(*feature, "referrer_url");
  return referrer.empty() ? kDefaultReferrerUrl : referrer;
}

BASE_FEATURE(kPersonalizeFeedUnsignedUsers,
             "PersonalizeFeedUnsignedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPersonalizeFeedNonSyncUsers,
             "PersonalizeFeedNonSyncUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

signin::ConsentLevel GetConsentLevelNeededForPersonalizedFeed() {
  if (!base::FeatureList::IsEnabled(kPersonalizeFeedNonSyncUsers))
    return signin::ConsentLevel::kSync;
  return signin::ConsentLevel::kSignin;
}

BASE_FEATURE(kInfoCardAcknowledgementTracking,
             "InfoCardAcknowledgementTracking",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShareCrowButton,
             "ShareCrowButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedCloseRefresh,
             "FeedCloseRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kFeedCloseRefreshDelayMinutes{
    &kFeedCloseRefresh, "delay_minutes", 30};
const base::FeatureParam<bool> kFeedCloseRefreshRequireInteraction{
    &kFeedCloseRefresh, "require_interaction", true};

BASE_FEATURE(kFeedNoViewCache,
             "FeedNoViewCache",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kFeedReplaceAll,
             "FeedReplaceAll",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedVideoInlinePlayback,
             "FeedVideoInlinePlayback",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientGoodVisits,
             "FeedClientGoodVisits",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kVisitTimeout{
    &kClientGoodVisits, "visit_timeout", base::Minutes(5)};

const base::FeatureParam<base::TimeDelta> kGoodTimeInFeed{
    &kClientGoodVisits, "good_time_in_feed", base::Minutes(1)};

const base::FeatureParam<base::TimeDelta> kLongOpenTime{
    &kClientGoodVisits, "long_open_time", base::Seconds(10)};

const base::FeatureParam<base::TimeDelta> kMinStableContentSliceVisibilityTime{
    &kClientGoodVisits, "min_stable_content_slice_visibility_time",
    base::Milliseconds(500)};

const base::FeatureParam<base::TimeDelta> kMaxStableContentSliceVisibilityTime{
    &kClientGoodVisits, "max_stable_content_slice_visibility_time",
    base::Seconds(30)};

const base::FeatureParam<double> kSliceVisibleExposureThreshold{
    &kClientGoodVisits, "slice_exposure_threshold", 0.5f};

const base::FeatureParam<double> kSliceVisibleCoverageThreshold{
    &kClientGoodVisits, "slice_coverage_threshold", 0.25f};
BASE_FEATURE(kFeedExperimentIDTagging,
             "FeedExperimentIDTagging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedShowSignInCommand,
             "FeedShowSignInCommand",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedPerformanceStudy,
             "FeedPerformanceStudy",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyntheticCapabilities,
             "FeedSyntheticCapabilities",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace feed
