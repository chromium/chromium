// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_FEED_FEATURE_LIST_H_
#define COMPONENTS_FEED_FEED_FEATURE_LIST_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"

// TODO(crbug.com/40741748): Clean up feedv1 features.

namespace feed {

namespace switches {
// Specifies whether RssLinkReader is enabled.
inline constexpr char kEnableRssLinkReader[] = "enable-rss-link-reader";
}

COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
extern const char kFeedHeaderRemovalTreatmentParam[];
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
extern const char kFeedHeaderRemovalTreatmentValue1[];
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
extern const char kFeedHeaderRemovalTreatmentValue2[];

COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kInterestFeedV2);

// Use the new DiscoFeed endpoint.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kDiscoFeedEndpoint);

// Feature that enables xsurface to provide the metrics reporting state to an
// xsurface feed.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kXsurfaceMetricsReporting);

// Feature that shows placeholder cards instead of a loading spinner at first
// load.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedLoadingPlaceholder);

// Feature that allows tuning the size of the image memory cache. Value is a
// percentage of the maximum size calculated for the device.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedImageMemoryCacheSizePercentage);

// Feature that enables StAMP cards in the feed.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedStamp);

// Feature that provides the user assistance in discovering the web feed.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kWebFeedAwareness);

// Feature that provides the user assistance in using the web feed.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kWebFeedOnboarding);

// Feature that enables sorting by different heuristics in the web feed.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kWebFeedSort);

COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
bool IsCormorantEnabledForLocale(std::string country);

// Personalize feed for unsigned users.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kPersonalizeFeedUnsignedUsers);

// Returns the consent level needed to request a personalized feed.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
signin::ConsentLevel GetConsentLevelNeededForPersonalizedFeed();

// When enabled, no view cache is used.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedNoViewCache);

// When enabled, allow show sign in command to request a user signs in / syncs.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedShowSignInCommand);

// When enabled, depending on params selected, enable different
// performance-oriented features in Feed.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedPerformanceStudy);

// When enabled, allows the server to unilaterally alter capabilities sent
// by the client, primarily to retroactively work around bugs.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kSyntheticCapabilities);

// Feature that enables signed-out view demotion.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedSignedOutViewDemotion);

// Feature that enables UI update for Follow.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedFollowUiUpdate);

// Feature that enables refreshing feed when Chrome restarts.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kRefreshFeedOnRestart);

// Feature that enables feed containment.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedContainment);

// Kill-switch for the web feed feature.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kWebFeedKillSwitch);

// Feature that unmount RecyclerBinder on view detach to fix a memory leak.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedRecyclerBinderUnmountOnDetach);

// Feature that enables feed streaming.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedStreaming);

// Feature that removes feed header.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedHeaderRemoval);

// Feature that enables feed audio overviews.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kFeedAudioOverviews);

// Feature that enables opening Incognito windows.
COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
BASE_DECLARE_FEATURE(kAndroidOpenIncognitoAsWindow);

COMPONENT_EXPORT(COMPONENTS_FEED_FEATURE_LIST)
bool IsWebFeedEnabledForLocale(const std::string& country);

}  // namespace feed

#endif  // COMPONENTS_FEED_FEED_FEATURE_LIST_H_
