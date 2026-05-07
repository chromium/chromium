// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/feed_feature_list.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/country_codes/country_codes.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/sync/base/features.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace feed {

// InterestFeedV2 takes precedence over InterestFeedContentSuggestions.
// InterestFeedV2 is cached in ChromeCachedFlags. If the default value here is
// changed, please update the cached one's default value in CachedFeatureFlags.
BASE_FEATURE(kInterestFeedV2, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDiscoFeedEndpoint, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kXsurfaceMetricsReporting, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedLoadingPlaceholder, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFeedImageMemoryCacheSizePercentage,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPersonalizeFeedUnsignedUsers, base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/40764861): Remove this helper, directly use kSignin instead.
signin::ConsentLevel GetConsentLevelNeededForPersonalizedFeed() {
  return signin::ConsentLevel::kSignin;
}

BASE_FEATURE(kFeedNoViewCache, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedPerformanceStudy, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyntheticCapabilities,
             "FeedSyntheticCapabilities",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedSignedOutViewDemotion, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRefreshFeedOnRestart, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedContainment, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFeedRecyclerBinderUnmountOnDetach,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedStreaming, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedAudioOverviews, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidOpenIncognitoAsWindow, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace feed
