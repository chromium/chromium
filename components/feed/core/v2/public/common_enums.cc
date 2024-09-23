// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/common_enums.h"

#include <ostream>

namespace feed {

std::ostream& operator<<(std::ostream& out, FeedUserActionType value) {
  switch (value) {
    case FeedUserActionType::kTappedOnCard:
      return out << "kTappedOnCard";
    case FeedUserActionType::kShownCard_DEPRECATED:
      return out << "kShownCard_DEPRECATED";
    case FeedUserActionType::kTappedSendFeedback:
      return out << "kTappedSendFeedback";
    case FeedUserActionType::kTappedLearnMore:
      return out << "kTappedLearnMore";
    case FeedUserActionType::kTappedHideStory:
      return out << "kTappedHideStory";
    case FeedUserActionType::kTappedNotInterestedIn:
      return out << "kTappedNotInterestedIn";
    case FeedUserActionType::kTappedManageInterests:
      return out << "kTappedManageInterests";
    case FeedUserActionType::kTappedDownload:
      return out << "kTappedDownload";
    case FeedUserActionType::kTappedOpenInNewTab:
      return out << "kTappedOpenInNewTab";
    case FeedUserActionType::kOpenedContextMenu:
      return out << "kOpenedContextMenu";
    case FeedUserActionType::kOpenedFeedSurface:
      return out << "kOpenedFeedSurface";
    case FeedUserActionType::kTappedOpenInNewIncognitoTab:
      return out << "kTappedOpenInNewIncognitoTab";
    case FeedUserActionType::kEphemeralChange:
      return out << "kEphemeralChange";
    case FeedUserActionType::kEphemeralChangeRejected:
      return out << "kEphemeralChangeRejected";
    case FeedUserActionType::kTappedTurnOn:
      return out << "kTappedTurnOn";
    case FeedUserActionType::kTappedTurnOff:
      return out << "kTappedTurnOff";
    case FeedUserActionType::kTappedManageActivity:
      return out << "kTappedManageActivity";
    case FeedUserActionType::kAddedToReadLater:
      return out << "kAddedToReadLater";
    case FeedUserActionType::kClosedContextMenu:
      return out << "kClosedContextMenu";
    case FeedUserActionType::kEphemeralChangeCommited:
      return out << "kEphemeralChangeCommited";
    case FeedUserActionType::kOpenedDialog:
      return out << "kOpenedDialog";
    case FeedUserActionType::kClosedDialog:
      return out << "kClosedDialog";
    case FeedUserActionType::kShowSnackbar:
      return out << "kShowSnackbar";
    case FeedUserActionType::kOpenedNativeActionSheet:
      return out << "kOpenedNativeActionSheet";
    case FeedUserActionType::kOpenedNativeContextMenu:
      return out << "kOpenedNativeContextMenu";
    case FeedUserActionType::kClosedNativeContextMenu:
      return out << "kClosedNativeContextMenu";
    case FeedUserActionType::kOpenedNativePulldownMenu:
      return out << "kOpenedNativePulldownMenu";
    case FeedUserActionType::kClosedNativePulldownMenu:
      return out << "kClosedNativePulldownMenu";
    case FeedUserActionType::kTappedManageReactions:
      return out << "kTappedManageReactions";
    case FeedUserActionType::kShare:
      return out << "kShare";
    case FeedUserActionType::kTappedManageFollowing:
      return out << "kTappedManageFollowing";
    case FeedUserActionType::kTappedFollowOnManagementSurface:
      return out << "kTappedFollowOnManagementSurface";
    case FeedUserActionType::kTappedUnfollowOnManagementSurface:
      return out << "kTappedUnfollowOnManagementSurface";
    case FeedUserActionType::kTappedFollowOnFollowAccelerator:
      return out << "kTappedFollowOnFollowAccelerator";
    case FeedUserActionType::kTappedFollowTryAgainOnSnackbar:
      return out << "kTappedFollowTryAgainOnSnackbar";
    case FeedUserActionType::kTappedRefollowAfterUnfollowOnSnackbar:
      return out << "kTappedRefollowAfterUnfollowOnSnackbar";
    case FeedUserActionType::kTappedUnfollowTryAgainOnSnackbar:
      return out << "kTappedUnfollowTryAgainOnSnackbar";
    case FeedUserActionType::kTappedGoToFeedPostFollowActiveHelp:
      return out << "kTappedGoToFeedPostFollowActiveHelp";
    case FeedUserActionType::kTappedDismissPostFollowActiveHelp:
      return out << "kTappedDismissPostFollowActiveHelp";
    case FeedUserActionType::kTappedDiscoverFeedPreview:
      return out << "kTappedDiscoverFeedPreview";
    case FeedUserActionType::kOpenedAutoplaySettings:
      return out << "kOpenedAutoplaySettings";
    case FeedUserActionType::kTappedAddToReadingList:
      return out << "kTappedAddToReadingList";
    case FeedUserActionType::kTappedManage:
      return out << "kTappedManage";
    case FeedUserActionType::kTappedManageHidden:
      return out << "kTappedManageHidden";
    case FeedUserActionType::kTappedFollowButton:
      return out << "kTappedFollow";
    case FeedUserActionType::kDiscoverFeedSelected:
      return out << "kDiscoverFeedSelected";
    case FeedUserActionType::kFollowingFeedSelected:
      return out << "kFollowingFeedSelected";
    case FeedUserActionType::kTappedUnfollowButton:
      return out << "kTappedUnfollow";
    case FeedUserActionType::kShowFollowSucceedSnackbar:
      return out << "kShowFollowSucceedSnackbar";
    case FeedUserActionType::kShowFollowFailedSnackbar:
      return out << "kShowFollowFailedSnackbar";
    case FeedUserActionType::kShowUnfollowSucceedSnackbar:
      return out << "kShowUnfollowSucceedSnackbar";
    case FeedUserActionType::kShowUnfollowFailedSnackbar:
      return out << "kShowUnfollowFailedSnackbar";
    case FeedUserActionType::kTappedGoToFeedOnSnackbar:
      return out << "kTappedGoToFeedOnSnackbar";
    case FeedUserActionType::kFirstFollowSheetShown:
      return out << "kFirstFollowSheetShown";
    case FeedUserActionType::kFirstFollowSheetTappedGoToFeed:
      return out << "kFirstFollowSheetTappedGoToFeed";
    case FeedUserActionType::kFirstFollowSheetTappedGotIt:
      return out << "kFirstFollowSheetTappedGotIt";
    case FeedUserActionType::kFollowRecommendationIPHShown:
      return out << "kFollowRecommendationIPHShown";
    case FeedUserActionType::kTappedOpenInNewTabInGroup:
      return out << "kTappedOpenInNewTabInGroup";
    case FeedUserActionType::kFollowingFeedSelectedGroupByPublisher:
      return out << "kFollowingFeedSelectedGroupByPublisher";
    case FeedUserActionType::kFollowingFeedSelectedSortByLatest:
      return out << "kFollowingFeedSelectedSortByLatest";
    case FeedUserActionType::kTappedFollowOnRecommendationFollowAccelerator:
      return out << "kTappedFollowOnRecommendationFollowAccelerator";
    case FeedUserActionType::kTappedGotItFeedPostFollowActiveHelp:
      return out << "kTappedGotItFeedPostFollowActiveHelp";
    case FeedUserActionType::kTappedRefreshFollowingFeedOnSnackbar:
      return out << "kTappedRefreshFollowingFeedOnSnackbar";
    case FeedUserActionType::kNonSwipeManualRefresh:
      return out << "kNonSwipeManualRefresh";
  }
}

}  // namespace feed
