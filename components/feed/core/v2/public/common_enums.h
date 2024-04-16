// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_COMMON_ENUMS_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_COMMON_ENUMS_H_

#include <iosfwd>

// Unlike most code from feed/core, these enums are used by both iOS and
// Android.
namespace feed {

// Values for the UMA ContentSuggestions.Feed.FeedSignInUI histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This must be kept in sync with
// FeedSignInUI in enums.xml.
enum class FeedSignInUI : int {
  // Histogram recorded when a sync half sheet is shown from Feed, when sign-in
  // UI is not supported.
  // kShowSyncHalfSheet = 0, (deprecated)
  // Histogram recorded when a sign-in only flow is shown from Feed.
  kShowSignInOnlyFlow = 1,
  // Histogram recorded when a service disabled toast is shown from Feed.
  kShowSignInDisableToast = 2,

  kMaxValue = kShowSignInDisableToast,
};

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
  kGoodVisit = 5,
  kMaxValue = kGoodVisit,
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
  // Action reported whenever the feed is visible.
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
  // Tapped the 'Following' option inside the Feed's 'Manage' interstitial.
  kTappedManageFollowing = 30,
  // User tapped to follow a web feed on the management surface.
  kTappedFollowOnManagementSurface = 31,
  // User tapped to unfollow a web feed on the management surface.
  kTappedUnfollowOnManagementSurface = 32,
  // User tapped to follow using the follow accelerator.
  kTappedFollowOnFollowAccelerator = 33,
  // User tapped to follow using the snackbar 'try again' option.
  kTappedFollowTryAgainOnSnackbar = 34,
  // User tapped to refollow using the snackbar, after successfully unfollowing.
  kTappedRefollowAfterUnfollowOnSnackbar = 35,
  // User tapped to unfollow using the snackbar 'try again' option.
  kTappedUnfollowTryAgainOnSnackbar = 36,
  // After following an active web feed, the user tapped to go to Following feed
  // using the post-follow help dialog.
  kTappedGoToFeedPostFollowActiveHelp = 37,
  // After following an active web feed, the user tapped to dismiss the
  // post-follow help dialog.
  kTappedDismissPostFollowActiveHelp = 38,
  // After long-pressing on the feed and seeing the preview, the user tapped
  // on the preview.
  kTappedDiscoverFeedPreview = 39,
  // User tapped "Settings" link to open feed autoplay settings.
  kOpenedAutoplaySettings = 40,
  // User tapped "Add to Reading List" in the context menu.
  kTappedAddToReadingList = 41,
  // User tapped "Manage" icon to open the manage intestitial.
  kTappedManage = 42,
  // User tapped "Hidden" in the manage intestitial.
  kTappedManageHidden = 43,
  // User tapped the "Follow" button on the main menu. (Android)
  // User tapped the "Follow" option on the context menu. (IOS)
  kTappedFollowButton = 44,
  // User tapped on the Discover feed from the feed header.
  kDiscoverFeedSelected = 45,
  // User tapped on the Following feed from the feed header.
  kFollowingFeedSelected = 46,
  // User tapped the "Unfollow" option on the context menu.
  kTappedUnfollowButton = 47,
  // User action caused a follow succeed snackbar to be shown. User action not
  // reported here. iOS only.
  kShowFollowSucceedSnackbar = 48,
  // User action caused a follow failed snackbar to be shown. User action not
  // reported here. iOS only.
  kShowFollowFailedSnackbar = 49,
  // User action caused a unfollow succeed snackbar to be shown. User action not
  // reported here. iOS only.
  kShowUnfollowSucceedSnackbar = 50,
  // User action caused a unfollow failed snackbar to be shown. User action not
  // reported here. iOS only.
  kShowUnfollowFailedSnackbar = 51,
  // User tapped to go to Following feed using the snackbar 'go to Following'
  // option.
  kTappedGoToFeedOnSnackbar = 52,

  // User tapped the Crow button in the context menu.
  // No longer used after the removal of Crow.
  // kTappedCrowButton = 53,

  // User action caused a first follow sheet to be shown. User action not
  // reported here. iOS only.
  kFirstFollowSheetShown = 54,
  // User tapped the "Go To Feed" button on the first follow sheet. (IOS)
  kFirstFollowSheetTappedGoToFeed = 55,
  // User tapped the "Got It" button on the first follow sheet. (IOS)
  kFirstFollowSheetTappedGotIt = 56,
  // Page load caused a Follow Recommendation IPH to be shown. User action not
  // reported here. iOS only.
  kFollowRecommendationIPHShown = 57,
  // User opened the article in a new tab in group from the back of card menu.
  kTappedOpenInNewTabInGroup = 58,
  // User selected the "Group by Publisher" Following feed sort type.
  kFollowingFeedSelectedGroupByPublisher = 59,
  // User selected the "Sort by Latest" Following feed sort type.
  kFollowingFeedSelectedSortByLatest = 60,
  // After following an active web feed, the user tapped on 'got it' to
  // close the post-follow help dialog.
  kTappedGotItFeedPostFollowActiveHelp = 61,
  // User tapped the follow accelerator which is presented after a user taps
  // on a recommendation that is in the feed.
  kTappedFollowOnRecommendationFollowAccelerator = 62,
  // User requested to refresh the Following feed using the post-follow snackbar
  // 'refresh' action.
  kTappedRefreshFollowingFeedOnSnackbar = 63,
  // User tapped on the "Continue" of the sign-in promote UI. iOS only.
  // Deprecated.
  // kTappedFeedSignInPromoUIContinue = 64,
  // User tapped on the "Cancel" of the sign-in promote UI. iOS only.
  // Deprecated.
  // kTappedFeedSignInPromoUICancel = 65,
  // User initiated non-swipe manual refresh of the feed.
  kNonSwipeManualRefresh = 66,

  kMaxValue = kNonSwipeManualRefresh,
};

// For testing and debugging only.
std::ostream& operator<<(std::ostream& out, FeedUserActionType value);

// Values for the UMA
// ContentSuggestions.Feed.WebFeed.RefreshContentOrder histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This must be kept in sync with
// FeedContentOrder in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.v2
enum class ContentOrder : int {
  // Content order is not specified.
  kUnspecified = 0,
  // Content is grouped by provider.
  kGrouped = 1,
  // Content is ungrouped, and arranged in reverse chronological order.
  kReverseChron = 2,

  kMaxValue = kReverseChron,
};

// Values for the UMA
// ContentSuggestions.Feed.WebFeed.SortType* histograms.
// These values are persisted to logs. Entries should never be reused.
// This must be kept in sync with FeedSortType in enums.xml
// TODO(crbug.com/40241876): should merge with ContentOrder.
enum class FeedSortType : int {
  // Sort Type unspecified.
  kUnspecifiedSortType = 0,
  // Content is grouped by publisher.
  kGroupedByPublisher = 1,
  // Content is ungrouped, and arranged in reverse chronological order.
  kSortedByLatest = 2,

  kMaxValue = kSortedByLatest,
};

// Values for the UMA ContentSuggestions.Feed.FeedSyncPromo histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This must be kept in sync with
// FeedSyncPromo in enums.xml.
enum class FeedSyncPromo : int {
  // Histogram recorded when a signed out user taps on the Feed sync promo
  // and the sync flow is shown.
  kShowSyncFlow = 0,
  // Histogram recorded when a signed out user taps on the Feed sync promo
  // and a service disabled toast is shown.
  kShowDisableToast = 1,

  kMaxValue = kShowDisableToast,
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_COMMON_ENUMS_H_
