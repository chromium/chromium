// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_FEED_FEATURE_LIST_H_
#define COMPONENTS_FEED_FEED_FEATURE_LIST_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"

// TODO(crbug.com/40741748): Clean up feedv1 features.

namespace feed {

namespace switches {
extern const char kEnableRssLinkReader[];
}

BASE_DECLARE_FEATURE(kInterestFeedV2);
BASE_DECLARE_FEATURE(kInterestFeedV2Hearts);
BASE_DECLARE_FEATURE(kInterestFeedV2Scrolling);

// Feature that allows the client to automatically dismiss the notice card based
// on the clicks and views on the notice card.
#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kInterestFeedNoticeCardAutoDismiss);
#endif

// Use the new DiscoFeed endpoint.
BASE_DECLARE_FEATURE(kDiscoFeedEndpoint);

// Feature that enables xsurface to provide the metrics reporting state to an
// xsurface feed.
BASE_DECLARE_FEATURE(kXsurfaceMetricsReporting);

// Feature that shows placeholder cards instead of a loading spinner at first
// load.
BASE_DECLARE_FEATURE(kFeedLoadingPlaceholder);

// Feature that allows tuning the size of the image memory cache. Value is a
// percentage of the maximum size calculated for the device.
BASE_DECLARE_FEATURE(kFeedImageMemoryCacheSizePercentage);

#if BUILDFLAG(IS_ANDROID)
// When enabled, causes the server to restrig the Sync Promo Banner for the
// bottom of Feed to a Signin Promo.
BASE_DECLARE_FEATURE(kFeedBottomSyncStringRemoval);
#endif

// Feature that enables StAMP cards in the feed.
BASE_DECLARE_FEATURE(kFeedStamp);

// Feature that provides the user assistance in discovering the web feed.
BASE_DECLARE_FEATURE(kWebFeedAwareness);

// Feature that provides the user assistance in using the web feed.
BASE_DECLARE_FEATURE(kWebFeedOnboarding);

// Feature that enables sorting by different heuristics in the web feed.
BASE_DECLARE_FEATURE(kWebFeedSort);

bool IsCormorantEnabledForLocale(std::string country);

// Personalize feed for unsigned users.
BASE_DECLARE_FEATURE(kPersonalizeFeedUnsignedUsers);

// Returns the consent level needed to request a personalized feed.
signin::ConsentLevel GetConsentLevelNeededForPersonalizedFeed();

// Feature that enables tracking the acknowledgement state for the info cards.
BASE_DECLARE_FEATURE(kInfoCardAcknowledgementTracking);

// When enabled, no view cache is used.
BASE_DECLARE_FEATURE(kFeedNoViewCache);

// When enabled, allow show sign in command to request a user signs in / syncs.
BASE_DECLARE_FEATURE(kFeedShowSignInCommand);

// When enabled, depending on params selected, enable different
// performance-oriented features in Feed.
BASE_DECLARE_FEATURE(kFeedPerformanceStudy);

// When enabled, allows the server to unilaterally alter capabilities sent
// by the client, primarily to retroactively work around bugs.
BASE_DECLARE_FEATURE(kSyntheticCapabilities);

// Feature that enables signed-out view demotion.
BASE_DECLARE_FEATURE(kFeedSignedOutViewDemotion);

// Feature that enables dynamic colors in the feed.
BASE_DECLARE_FEATURE(kFeedDynamicColors);

// Feature that enables UI update for Follow.
BASE_DECLARE_FEATURE(kFeedFollowUiUpdate);

// Feature that enables refreshing feed when Chrome restarts.
BASE_DECLARE_FEATURE(kRefreshFeedOnRestart);

// Feature that enables feed containment.
BASE_DECLARE_FEATURE(kFeedContainment);

// Kill-switch for the web feed feature.
BASE_DECLARE_FEATURE(kWebFeedKillSwitch);

// Feature that enables feed low-memory improvement.
BASE_DECLARE_FEATURE(kFeedLowMemoryImprovement);

bool IsWebFeedEnabledForLocale(const std::string& country);

}  // namespace feed

#endif  // COMPONENTS_FEED_FEED_FEATURE_LIST_H_
