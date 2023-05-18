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

// TODO(crbug.com/1165828): Clean up feedv1 features.

namespace feed {

BASE_DECLARE_FEATURE(kInterestFeedContentSuggestions);
BASE_DECLARE_FEATURE(kInterestFeedV2);
BASE_DECLARE_FEATURE(kInterestFeedV2Autoplay);
BASE_DECLARE_FEATURE(kInterestFeedV2Hearts);
BASE_DECLARE_FEATURE(kInterestFeedV2Scrolling);

extern const base::FeatureParam<std::string> kDisableTriggerTypes;
extern const base::FeatureParam<int> kTimeoutDurationSeconds;
extern const base::FeatureParam<bool> kThrottleBackgroundFetches;
extern const base::FeatureParam<bool> kOnlySetLastRefreshAttemptOnSuccess;

// Feature that allows the client to automatically dismiss the notice card based
// on the clicks and views on the notice card.
#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kInterestFeedNoticeCardAutoDismiss);
#endif

// Feature that allows users to keep up with and consume web content.
BASE_DECLARE_FEATURE(kWebFeed);

// Use the new DiscoFeed endpoint.
BASE_DECLARE_FEATURE(kDiscoFeedEndpoint);

// Feature that enables xsurface to provide the metrics reporting state to an
// xsurface feed.
BASE_DECLARE_FEATURE(kXsurfaceMetricsReporting);

// Feature that enables sticky header when users scroll down.
BASE_DECLARE_FEATURE(kFeedHeaderStickToTop);

// Feature that shows placeholder cards instead of a loading spinner at first
// load.
BASE_DECLARE_FEATURE(kFeedLoadingPlaceholder);

// Param allowing animations to be disabled when showing the placeholder on
// instant start.
extern const base::FeatureParam<bool>
    kEnableFeedLoadingPlaceholderAnimationOnInstantStart;

// Feature that allows tuning the size of the image memory cache. Value is a
// percentage of the maximum size calculated for the device.
BASE_DECLARE_FEATURE(kFeedImageMemoryCacheSizePercentage);

// Feature that enables showing a callout to help users return to the top of the
// feeds quickly.
BASE_DECLARE_FEATURE(kFeedBackToTop);

// When enabled, causes the server to send a Sync Promo Banner for the bottom of
// feed.
BASE_DECLARE_FEATURE(kFeedBottomSyncBanner);

// When enabled, shows a sign in bottom sheet when p13n actions on boc are used
// by signed out client.
BASE_DECLARE_FEATURE(kFeedBoCSigninInterstitial);

// Feature that enables StAMP cards in the feed.
BASE_DECLARE_FEATURE(kFeedStamp);

// Feature that provides the user assistance in discovering the web feed.
BASE_DECLARE_FEATURE(kWebFeedAwareness);

// Feature that provides the user assistance in using the web feed.
BASE_DECLARE_FEATURE(kWebFeedOnboarding);

// Feature that enables sorting by different heuristics in the web feed.
BASE_DECLARE_FEATURE(kWebFeedSort);

// Feature that causes the "open in new tab" menu item to appear on feed items
// on Start Surface.
BASE_DECLARE_FEATURE(kEnableOpenInNewTabFromStartSurfaceFeed);

// Feature that causes the WebUI version of the Feed to be enabled.
BASE_DECLARE_FEATURE(kWebUiFeed);
extern const base::FeatureParam<std::string> kWebUiFeedUrl;
extern const base::FeatureParam<bool> kWebUiDisableContentSecurityPolicy;

std::string GetFeedReferrerUrl();

// Personalize feed for unsigned users.
BASE_DECLARE_FEATURE(kPersonalizeFeedUnsignedUsers);

// Personalize feed for signed in users who haven't enabled sync.
BASE_DECLARE_FEATURE(kPersonalizeFeedNonSyncUsers);

// Returns the consent level needed to request a personalized feed.
signin::ConsentLevel GetConsentLevelNeededForPersonalizedFeed();

// Feature that enables tracking the acknowledgement state for the info cards.
BASE_DECLARE_FEATURE(kInfoCardAcknowledgementTracking);

// Feature that enables the Crow feature.
// Owned by the CwF team but located here until it makes sense to create a crow
// component, since it is being used in the feed component.
BASE_DECLARE_FEATURE(kShareCrowButton);

// When enabled, schedule a background refresh for a feed sometime after the
// last user engagement with that feed.
BASE_DECLARE_FEATURE(kFeedCloseRefresh);
// On each qualifying user engagement, schedule a background refresh this many
// minutes out.
extern const base::FeatureParam<int> kFeedCloseRefreshDelayMinutes;
// If true, schedule the refresh only when the user scrolls or interacts. If
// false, schedule only when the feed surface is opened to content.
extern const base::FeatureParam<bool> kFeedCloseRefreshRequireInteraction;

// When enabled, no view cache is used.
BASE_DECLARE_FEATURE(kFeedNoViewCache);

// When enabled, play the feed video via inline playback.
BASE_DECLARE_FEATURE(kFeedVideoInlinePlayback);

// When enabled, allow tagging experiments with only an experiment ID.
BASE_DECLARE_FEATURE(kFeedExperimentIDTagging);

// When enabled, allow show sign in command to request a user signs in / syncs.
BASE_DECLARE_FEATURE(kFeedShowSignInCommand);

// When enabled, depending on params selected, enable different
// performance-oriented features in Feed.
BASE_DECLARE_FEATURE(kFeedPerformanceStudy);

// When enabled, allows the server to unilaterally alter capabilities sent
// by the client, primarily to retroactively work around bugs.
BASE_DECLARE_FEATURE(kSyntheticCapabilities);

// Feature that enables Cormorant for users.
BASE_DECLARE_FEATURE(kCormorant);

// Feature that enables reporting feed user interaction reliability.
BASE_DECLARE_FEATURE(kFeedUserInteractionReliabilityReport);

// Feature that enables signed-out view demotion.
BASE_DECLARE_FEATURE(kFeedSignedOutViewDemotion);

// Feature that enables dynamic colors in the feed.
BASE_DECLARE_FEATURE(kFeedDynamicColors);

}  // namespace feed

#endif  // COMPONENTS_FEED_FEED_FEATURE_LIST_H_
