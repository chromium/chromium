// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/feed_feature_list.h"
#include "base/containers/contains.h"
#include "base/time/time.h"
#include "components/country_codes/country_codes.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/sync/base/features.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace feed {

namespace switches {
// Specifies whether RssLinkReader is enabled.
const char kEnableRssLinkReader[] = "enable-rss-link-reader";
}  // namespace switches

// InterestFeedV2 takes precedence over InterestFeedContentSuggestions.
// InterestFeedV2 is cached in ChromeCachedFlags. If the default value here is
// changed, please update the cached one's default value in CachedFeatureFlags.
BASE_FEATURE(kInterestFeedV2,
             "InterestFeedV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kInterestFeedV2Hearts,
             "InterestFeedV2Hearts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInterestFeedV2Scrolling,
             "InterestFeedV2Scrolling",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kInterestFeedNoticeCardAutoDismiss,
             "InterestFeedNoticeCardAutoDismiss",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kDiscoFeedEndpoint,
             "DiscoFeedEndpoint",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kXsurfaceMetricsReporting,
             "XsurfaceMetricsReporting",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedLoadingPlaceholder,
             "FeedLoadingPlaceholder",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedImageMemoryCacheSizePercentage,
             "FeedImageMemoryCacheSizePercentage",
             base::FEATURE_DISABLED_BY_DEFAULT);
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kFeedBottomSyncStringRemoval,
             "FeedBottomSyncStringRemoval",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif
BASE_FEATURE(kFeedStamp, "FeedStamp", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedAwareness,
             "WebFeedAwareness",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedOnboarding,
             "WebFeedOnboarding",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedSort, "WebFeedSort", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsCormorantEnabledForLocale(std::string country) {
  return IsWebFeedEnabledForLocale(country);
}

BASE_FEATURE(kPersonalizeFeedUnsignedUsers,
             "PersonalizeFeedUnsignedUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/40764861): Remove this helper, directly use kSignin instead.
signin::ConsentLevel GetConsentLevelNeededForPersonalizedFeed() {
  return signin::ConsentLevel::kSignin;
}

BASE_FEATURE(kInfoCardAcknowledgementTracking,
             "InfoCardAcknowledgementTracking",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedNoViewCache,
             "FeedNoViewCache",
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

BASE_FEATURE(kFeedSignedOutViewDemotion,
             "FeedSignedOutViewDemotion",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedDynamicColors,
             "FeedDynamicColors",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedFollowUiUpdate,
             "FeedFollowUiUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRefreshFeedOnRestart,
             "RefreshFeedOnRestart",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedContainment,
             "FeedContainment",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedKillSwitch,
             "WebFeedKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedLowMemoryImprovement,
             "FeedLowMemoryImprovement",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsWebFeedEnabledForLocale(const std::string& country) {
  const std::vector<std::string> launched_countries = {"AU", "CA", "GB",
                                                       "NZ", "US", "ZA"};
  return base::Contains(launched_countries, country) &&
         !base::FeatureList::IsEnabled(kWebFeedKillSwitch);
}

}  // namespace feed
