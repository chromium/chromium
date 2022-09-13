// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_FEED_FEATURE_LIST_H_
#define COMPONENTS_FEED_FEED_FEATURE_LIST_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"

// TODO(crbug.com/1165828): Clean up feedv1 features.

namespace feed {

extern const base::Feature kInterestFeedContentSuggestions;
extern const base::Feature kInterestFeedV2;
extern const base::Feature kInterestFeedV2Autoplay;
extern const base::Feature kInterestFeedV2Hearts;
extern const base::Feature kInterestFeedV2Scrolling;

extern const base::FeatureParam<std::string> kDisableTriggerTypes;
extern const base::FeatureParam<int> kSuppressRefreshDurationMinutes;
extern const base::FeatureParam<int> kTimeoutDurationSeconds;
extern const base::FeatureParam<bool> kThrottleBackgroundFetches;
extern const base::FeatureParam<bool> kOnlySetLastRefreshAttemptOnSuccess;

// TODO(b/213622639): The following two features are obsolete and should be
// removed.
// Determines whether conditions should be reached before enabling the upload of
// click and view actions in the feed (e.g., the user needs to view X cards).
// For example, this is needed when the notice card is at the second position in
// the feed.
extern const base::Feature kInterestFeedV1ClicksAndViewsConditionalUpload;
extern const base::Feature kInterestFeedV2ClicksAndViewsConditionalUpload;

// Feature that allows the client to automatically dismiss the notice card based
// on the clicks and views on the notice card.
#if BUILDFLAG(IS_IOS)
extern const base::Feature kInterestFeedNoticeCardAutoDismiss;
#endif

// Feature that allows users to keep up with and consume web content.
extern const base::Feature kWebFeed;

// Use the new DiscoFeed endpoint.
extern const base::Feature kDiscoFeedEndpoint;

// Feature that enables xsurface to provide the metrics reporting state to an
// xsurface feed.
extern const base::Feature kXsurfaceMetricsReporting;

// Whether to log reliability events.
extern const base::Feature kReliabilityLogging;

// Feature that enables refreshing feeds triggered by the users.
extern const base::Feature kFeedInteractiveRefresh;

// Feature that shows placeholder cards instead of a loading spinner at first
// load.
extern const base::Feature kFeedLoadingPlaceholder;

// Param allowing animations to be disabled when showing the placeholder on
// instant start.
extern const base::FeatureParam<bool>
    kEnableFeedLoadingPlaceholderAnimationOnInstantStart;

// Feature that allows tuning the size of the image memory cache. Value is a
// percentage of the maximum size calculated for the device.
extern const base::Feature kFeedImageMemoryCacheSizePercentage;

// Feature that enables clearing the image memory cache when the feed is
// destroyed.
extern const base::Feature kFeedClearImageMemoryCache;

// Feature that enables showing a callout to help users return to the top of the
// feeds quickly.
extern const base::Feature kFeedBackToTop;

// Feature that enables StAMP cards in the feed.
extern const base::Feature kFeedStamp;

// Feature that provides the user assistance in discovering the web feed.
extern const base::Feature kWebFeedAwareness;

// Feature that provides the user assistance in using the web feed.
extern const base::Feature kWebFeedOnboarding;

// Feature that enables sorting by different heuristics in the web feed.
extern const base::Feature kWebFeedSort;

// Feature that causes the "open in new tab" menu item to appear on feed items
// on Start Surface.
extern const base::Feature kEnableOpenInNewTabFromStartSurfaceFeed;

// Feature that causes the WebUI version of the Feed to be enabled.
extern const base::Feature kWebUiFeed;
extern const base::FeatureParam<std::string> kWebUiFeedUrl;
extern const base::FeatureParam<bool> kWebUiDisableContentSecurityPolicy;

std::string GetFeedReferrerUrl();

// Personalize feed for unsigned users.
extern const base::Feature kPersonalizeFeedUnsignedUsers;

// Personalize feed for signed in users who haven't enabled sync.
extern const base::Feature kPersonalizeFeedNonSyncUsers;

// Returns the consent level needed to request a personalized feed.
signin::ConsentLevel GetConsentLevelNeededForPersonalizedFeed();

// Feature that enables tracking the acknowledgement state for the info cards.
extern const base::Feature kInfoCardAcknowledgementTracking;

// Feature that enables the Crow feature.
// Owned by the CwF team but located here until it makes sense to create a crow
// component, since it is being used in the feed component.
extern const base::Feature kShareCrowButton;

// Feature that when enabled completely removes all Feeds from chrome.
extern const base::Feature kIsAblated;

// When enabled, schedule a background refresh for a feed sometime after the
// last user engagement with that feed.
extern const base::Feature kFeedCloseRefresh;
// On each qualifying user engagement, schedule a background refresh this many
// minutes out.
extern const base::FeatureParam<int> kFeedCloseRefreshDelayMinutes;
// If true, schedule the refresh only when the user scrolls or interacts. If
// false, schedule only when the feed surface is opened to content.
extern const base::FeatureParam<bool> kFeedCloseRefreshRequireInteraction;

// When enabled, no view cache is used.
extern const base::Feature kFeedNoViewCache;
// When enabled, replace all items.
extern const base::Feature kFeedReplaceAll;

// When enabled, play the feed video via inline playback.
extern const base::Feature kFeedVideoInlinePlayback;

}  // namespace feed

#endif  // COMPONENTS_FEED_FEED_FEATURE_LIST_H_
