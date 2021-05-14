// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_COMMON_ENUMS_H_
#define COMPONENTS_FEED_CORE_V2_COMMON_ENUMS_H_

// Unlike most code from feed/core, these enums are used by both iOS and
// Android.
namespace feed {

// Values for the UMA ContentSuggestions.Feed.EngagementType
// histogram. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. This must be kept
// in sync with FeedEngagementType in enums.xml.
enum class FeedEngagementType {
  kFeedEngaged = 0,
  kFeedEngagedSimple = 1,
  kFeedInteracted = 2,
  kDeprecatedFeedScrolled = 3,
  kFeedScrolled = 4,
  kMaxValue = kFeedScrolled,
};

// Values for the UMA ContentSuggestions.Feed.UserActions
// histogram. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. This must be kept
// in sync with FeedUserActionType in enums.xml.
// Note: Most of these have a corresponding UserMetricsAction reported here.
// Exceptions are described below.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.v2
enum class FeedUserActionType {
  // User tapped on card, opening the article in the same tab.
  kTappedOnCard = 0,
  // DEPRECATED: This was never reported.
  kShownCard_DEPRECATED = 1,
  // User tapped on 'Send Feedback' in the back of card menu.
  kTappedSendFeedback = 2,
  // Discover feed header menu 'Learn More' tapped.
  kTappedLearnMore = 3,
  // User Tapped Hide Story in the back of card menu.
  kTappedHideStory = 4,
  // User Tapped  Not Interested In X in the back of card menu.
  kTappedNotInterestedIn = 5,
  // Discover feed header menu 'Manage Interests' tapped.
  kTappedManageInterests = 6,
  kTappedDownload = 7,
  // User opened the article in a new tab from the back of card menu.
  kTappedOpenInNewTab = 8,
  // User opened the back of card menu.
  kOpenedContextMenu = 9,
  // User action not reported here. See Suggestions.SurfaceVisible.
  kOpenedFeedSurface = 10,
  // User opened the article in an incognito tab from the back of card menu.
  kTappedOpenInNewIncognitoTab = 11,
  // Ephemeral change, likely due to hide story or not interested in.
  kEphemeralChange = 12,
  // Ephemeral change undone, likely due to pressing 'undo' on the snackbar.
  kEphemeralChangeRejected = 13,
  // Discover feed visibility toggled from header menu.
  kTappedTurnOn = 14,
  kTappedTurnOff = 15,
  // Discover feed header menu 'Manage Activity' tapped.
  kTappedManageActivity = 16,
  // User added article to 'Read Later' list. iOS only.
  kAddedToReadLater = 17,
  // User closed the back of card menu.
  kClosedContextMenu = 18,
  // Ephemeral change committed, likely due to dismissing an 'undo' snackbar.
  kEphemeralChangeCommited = 19,
  // User opened a Dialog. e.g. Report content Dialog. User action not reported
  // here. iOS only.
  kOpenedDialog = 20,
  // User closed a Dialog. e.g. Report content Dialog. iOS only.
  kClosedDialog = 21,
  // User action caused a snackbar to be shown. User action not reported here.
  // iOS only.
  kShowSnackbar = 22,
  // User opened back of card menu in the native action sheet. iOS only.
  kOpenedNativeActionSheet = 23,
  // User opened back of card menu in the native context menu. iOS only.
  kOpenedNativeContextMenu = 24,
  // User closed back of card menu in the native context menu. iOS only.
  kClosedNativeContextMenu = 25,
  // User opened back of card menu in the native pull-down menu. iOS only.
  kOpenedNativePulldownMenu = 26,
  // User closed back of card menu in the native pull-down menu. iOS only.
  kClosedNativePulldownMenu = 27,
  // User tapped feed header menu item 'Manage reactions'.
  kTappedManageReactions = 28,
  // User tapped on share.
  kShare = 29,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = kShare,
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_COMMON_ENUMS_H_
