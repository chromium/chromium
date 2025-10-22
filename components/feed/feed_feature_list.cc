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

const char kFeedHeaderRemovalTreatmentParam[] = "treatment";
const char kFeedHeaderRemovalTreatmentValue1[] = "label";
const char kFeedHeaderRemovalTreatmentValue2[] = "none";

// InterestFeedV2 takes precedence over InterestFeedContentSuggestions.
// InterestFeedV2 is cached in ChromeCachedFlags. If the default value here is
// changed, please update the cached one's default value in CachedFeatureFlags.
BASE_FEATURE(kInterestFeedV2, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiscoFeedEndpoint, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kXsurfaceMetricsReporting, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedLoadingPlaceholder, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedImageMemoryCacheSizePercentage,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedStamp, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedAwareness, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedOnboarding, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedSort, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsCormorantEnabledForLocale(std::string country) {
  return IsWebFeedEnabledForLocale(country);
}

BASE_FEATURE(kPersonalizeFeedUnsignedUsers, base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/40764861): Remove this helper, directly use kSignin instead.
signin::ConsentLevel GetConsentLevelNeededForPersonalizedFeed() {
  return signin::ConsentLevel::kSignin;
}

BASE_FEATURE(kFeedNoViewCache, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedShowSignInCommand, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedPerformanceStudy, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyntheticCapabilities,
             "FeedSyntheticCapabilities",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedSignedOutViewDemotion, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedFollowUiUpdate, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRefreshFeedOnRestart, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedContainment, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebFeedKillSwitch, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedRecyclerBinderUnmountOnDetach,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedStreaming, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedHeaderRemoval, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedAudioOverviews, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidOpenIncognitoAsWindow, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsWebFeedEnabledForLocale(const std::string& country) {
  const std::vector<std::string> launched_countries = {"AU", "CA", "GB",
                                                       "NZ", "US", "ZA"};
  return base::Contains(launched_countries, country) &&
         !base::FeatureList::IsEnabled(kWebFeedKillSwitch);
}

}  // namespace feed
