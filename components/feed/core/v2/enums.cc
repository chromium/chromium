// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/enums.h"

#include <ostream>
#include <string_view>

namespace feed {

// Included for debug builds only for reduced binary size.

std::ostream& operator<<(std::ostream& out, NetworkRequestType value) {
#ifndef NDEBUG
  switch (value) {
    case NetworkRequestType::kFeedQuery:
      return out << "kFeedQuery";
    case NetworkRequestType::kUploadActions:
      return out << "kUploadActions";
    case NetworkRequestType::kNextPage:
      return out << "kNextPage";
    case NetworkRequestType::kListWebFeeds:
      return out << "kListWebFeeds";
    case NetworkRequestType::kUnfollowWebFeed:
      return out << "kUnfollowWebFeed";
    case NetworkRequestType::kFollowWebFeed:
      return out << "kFollowWebFeed";
    case NetworkRequestType::kListRecommendedWebFeeds:
      return out << "kListRecommendedWebFeeds";
    case NetworkRequestType::kWebFeedListContents:
      return out << "kWebFeedListContents";
    case NetworkRequestType::kQueryInteractiveFeed:
      return out << "kQueryInteractiveFeed";
    case NetworkRequestType::kQueryBackgroundFeed:
      return out << "kQueryBackgroundFeed";
    case NetworkRequestType::kQueryNextPage:
      return out << "kQueryNextPage";
    case NetworkRequestType::kSingleWebFeedListContents:
      return out << "kSingleWebFeedListContents";
    case NetworkRequestType::kQueryWebFeed:
      return out << "kQueryWebFeed";
    case NetworkRequestType::kSupervisedFeed:
      return out << "kSupervisedFeed";
  }
#endif
  return out << (static_cast<int>(value));
}

std::ostream& operator<<(std::ostream& out, LoadStreamStatus value) {
#ifndef NDEBUG
  switch (value) {
    case LoadStreamStatus::kNoStatus:
      return out << "kNoStatus";
    case LoadStreamStatus::kLoadedFromStore:
      return out << "kLoadedFromStore";
    case LoadStreamStatus::kLoadedFromNetwork:
      return out << "kLoadedFromNetwork";
    case LoadStreamStatus::kFailedWithStoreError:
      return out << "kFailedWithStoreError";
    case LoadStreamStatus::kNoStreamDataInStore:
      return out << "kNoStreamDataInStore";
    case LoadStreamStatus::kModelAlreadyLoaded:
      return out << "kModelAlreadyLoaded";
    case LoadStreamStatus::kNoResponseBody:
      return out << "kNoResponseBody";
    case LoadStreamStatus::kProtoTranslationFailed:
      return out << "kProtoTranslationFailed";
    case LoadStreamStatus::kDataInStoreIsStale:
      return out << "kDataInStoreIsStale";
    case LoadStreamStatus::kDataInStoreIsStaleTimestampInFuture:
      return out << "kDataInStoreIsStaleTimestampInFuture";
    case LoadStreamStatus::
        kCannotLoadFromNetworkSupressedForHistoryDelete_DEPRECATED:
      return out
             << "kCannotLoadFromNetworkSupressedForHistoryDelete_DEPRECATED";
    case LoadStreamStatus::kCannotLoadFromNetworkOffline:
      return out << "kCannotLoadFromNetworkOffline";
    case LoadStreamStatus::kCannotLoadFromNetworkThrottled:
      return out << "kCannotLoadFromNetworkThrottled";
    case LoadStreamStatus::kLoadNotAllowedEulaNotAccepted:
      return out << "kLoadNotAllowedEulaNotAccepted";
    case LoadStreamStatus::kLoadNotAllowedArticlesListHidden:
      return out << "kLoadNotAllowedArticlesListHidden";
    case LoadStreamStatus::kCannotParseNetworkResponseBody:
      return out << "kCannotParseNetworkResponseBody";
    case LoadStreamStatus::kLoadMoreModelIsNotLoaded:
      return out << "kLoadMoreModelIsNotLoaded";
    case LoadStreamStatus::kLoadNotAllowedDisabledByEnterprisePolicy:
      return out << "kLoadNotAllowedDisabledByEnterprisePolicy";
    case LoadStreamStatus::kNetworkFetchFailed:
      return out << "kNetworkFetchFailed";
    case LoadStreamStatus::kCannotLoadMoreNoNextPageToken:
      return out << "kCannotLoadMoreNoNextPageToken";
    case LoadStreamStatus::kDataInStoreStaleMissedLastRefresh:
      return out << "kDataInStoreStaleMissedLastRefresh";
    case LoadStreamStatus::kLoadedStaleDataFromStoreDueToNetworkFailure:
      return out << "kLoadedStaleDataFromStoreDueToNetworkFailure";
    case LoadStreamStatus::kDataInStoreIsExpired:
      return out << "kDataInStoreIsExpired";
    case LoadStreamStatus::kDataInStoreIsForAnotherUser:
      return out << "kDataInStoreIsForAnotherUser";
    case LoadStreamStatus::kAbortWithPendingClearAll:
      return out << "kAbortWithPendingClearAll";
    case LoadStreamStatus::kAlreadyHaveUnreadContent:
      return out << "kAlreadyHaveUnreadContent";
    case LoadStreamStatus::kNotAWebFeedSubscriber:
      return out << "kNotAWebFeedSubscriber";
    case LoadStreamStatus::kAccountTokenFetchFailedWrongAccount:
      return out << "kAccountTokenFetchFailedWrongAccount";
    case LoadStreamStatus::kAccountTokenFetchTimedOut:
      return out << "kAccountTokenFetchTimedOut";
    case LoadStreamStatus::kNetworkFetchTimedOut:
      return out << "kNetworkFetchTimedOut";
    case LoadStreamStatus::kLoadNotAllowedDisabled:
      return out << "kLoadNotAllowedDisabled";
    case LoadStreamStatus::kLoadNotAllowedDisabledByDse:
      return out << "kLoadNotAllowedDisabledByDse";
  }
#else
  return out << (static_cast<int>(value));
#endif  // ifndef NDEBUG
}

bool IsLoadingSuccessfulAndFresh(LoadStreamStatus status) {
  switch (status) {
    case LoadStreamStatus::kLoadedFromStore:
    case LoadStreamStatus::kLoadedFromNetwork:
      return true;
    case LoadStreamStatus::kNoStatus:
    case LoadStreamStatus::kFailedWithStoreError:
    case LoadStreamStatus::kNoStreamDataInStore:
    case LoadStreamStatus::kModelAlreadyLoaded:
    case LoadStreamStatus::kNoResponseBody:
    case LoadStreamStatus::kProtoTranslationFailed:
    case LoadStreamStatus::kDataInStoreIsStale:
    case LoadStreamStatus::kDataInStoreIsStaleTimestampInFuture:
    case LoadStreamStatus::
        kCannotLoadFromNetworkSupressedForHistoryDelete_DEPRECATED:
    case LoadStreamStatus::kCannotLoadFromNetworkOffline:
    case LoadStreamStatus::kCannotLoadFromNetworkThrottled:
    case LoadStreamStatus::kLoadNotAllowedEulaNotAccepted:
    case LoadStreamStatus::kLoadNotAllowedArticlesListHidden:
    case LoadStreamStatus::kCannotParseNetworkResponseBody:
    case LoadStreamStatus::kLoadMoreModelIsNotLoaded:
    case LoadStreamStatus::kLoadNotAllowedDisabledByEnterprisePolicy:
    case LoadStreamStatus::kNetworkFetchFailed:
    case LoadStreamStatus::kCannotLoadMoreNoNextPageToken:
    case LoadStreamStatus::kDataInStoreStaleMissedLastRefresh:
    case LoadStreamStatus::kLoadedStaleDataFromStoreDueToNetworkFailure:
    case LoadStreamStatus::kDataInStoreIsExpired:
    case LoadStreamStatus::kDataInStoreIsForAnotherUser:
    case LoadStreamStatus::kAbortWithPendingClearAll:
    case LoadStreamStatus::kAlreadyHaveUnreadContent:
    case LoadStreamStatus::kNotAWebFeedSubscriber:

    case LoadStreamStatus::kAccountTokenFetchFailedWrongAccount:
    case LoadStreamStatus::kAccountTokenFetchTimedOut:
    case LoadStreamStatus::kNetworkFetchTimedOut:
    case LoadStreamStatus::kLoadNotAllowedDisabled:
    case LoadStreamStatus::kLoadNotAllowedDisabledByDse:
      return false;
  }
}

std::ostream& operator<<(std::ostream& out, UploadActionsStatus value) {
#ifndef NDEBUG
  switch (value) {
    case UploadActionsStatus::kNoStatus:
      return out << "kNoStatus";
    case UploadActionsStatus::kNoPendingActions:
      return out << "kNoPendingActions";
    case UploadActionsStatus::kFailedToStorePendingAction:
      return out << "kFailedToStorePendingAction";
    case UploadActionsStatus::kStoredPendingAction:
      return out << "kStoredPendingAction";
    case UploadActionsStatus::kUpdatedConsistencyToken:
      return out << "kUpdatedConsistencyToken";
    case UploadActionsStatus::kFinishedWithoutUpdatingConsistencyToken:
      return out << "kFinishedWithoutUpdatingConsistencyToken";
    case UploadActionsStatus::kAbortUploadForSignedOutUser:
      return out << "kAbortUploadForSignedOutUser";
    case UploadActionsStatus::kAbortUploadBecauseDisabled:
      return out << "kAbortUploadBecauseDisabled";
    case UploadActionsStatus::kAbortUploadForWrongUser:
      return out << "kAbortUploadForWrongUser";
    case UploadActionsStatus::kAbortUploadActionsWithPendingClearAll:
      return out << "kAbortUploadActionsWithPendingClearAll";
  }
#else
  return out << (static_cast<int>(value));
#endif  // ifndef NDEBUG
}

std::ostream& operator<<(std::ostream& out, UploadActionsBatchStatus value) {
#ifndef NDEBUG
  switch (value) {
    case UploadActionsBatchStatus::kNoStatus:
      return out << "kNoStatus";
    case UploadActionsBatchStatus::kFailedToUpdateStore:
      return out << "kFailedToUpdateStore";
    case UploadActionsBatchStatus::kFailedToUpload:
      return out << "kFailedToUpload";
    case UploadActionsBatchStatus::kFailedToRemoveUploadedActions:
      return out << "kFailedToRemoveUploadedActions";
    case UploadActionsBatchStatus::kExhaustedUploadQuota:
      return out << "kExhaustedUploadQuota";
    case UploadActionsBatchStatus::kAllActionsWereStale:
      return out << "kAllActionsWereStale";
    case UploadActionsBatchStatus::kSuccessfullyUploadedBatch:
      return out << "kSuccessfullyUploadedBatch";
  }
#else
  return out << (static_cast<int>(value));
#endif  // ifndef NDEBUG
}

std::ostream& operator<<(std::ostream& out, WebFeedRefreshStatus value) {
  switch (value) {
    case WebFeedRefreshStatus::kNoStatus:
      return out << "kNoStatus";
    case WebFeedRefreshStatus::kSuccess:
      return out << "kSuccess";
    case WebFeedRefreshStatus::kNetworkFailure:
      return out << "kNetworkFailure";
    case WebFeedRefreshStatus::kNetworkRequestThrottled:
      return out << "kNetworkRequestThrottled";
    case WebFeedRefreshStatus::kAbortFetchWebFeedPendingClearAll:
      return out << "kAbortFetchWebFeedPendingClearAll";
  }
}

std::string_view ToString(UserSettingsOnStart v) {
  switch (v) {
    case UserSettingsOnStart::kFeedNotEnabledByPolicy:
      return "FeedNotEnabledByPolicy";
    case UserSettingsOnStart::kFeedNotVisibleSignedOut:
      return "FeedNotVisibleSignedOut";
    case UserSettingsOnStart::kFeedNotVisibleSignedIn:
      return "FeedNotVisibleSignedIn";
    case UserSettingsOnStart::kSignedOut:
      return "SignedOut";
    case UserSettingsOnStart::kSignedInWaaOnDpOn:
      return "SignedInWaaOnDpOn";
    case UserSettingsOnStart::kSignedInWaaOnDpOff:
      return "SignedInWaaOnDpOff";
    case UserSettingsOnStart::kSignedInWaaOffDpOn:
      return "SignedInWaaOffDpOn";
    case UserSettingsOnStart::kSignedInWaaOffDpOff:
      return "SignedInWaaOffDpOff";
    case UserSettingsOnStart::kSignedInNoRecentData:
      return "SignedInNoRecentData";
    case UserSettingsOnStart::kFeedNotEnabled:
      return "FeedNotEnabled";
    case UserSettingsOnStart::kFeedNotEnabledByDse:
      return "FeedNotEnabledByDse";
  }
  return "Unknown";
}
std::ostream& operator<<(std::ostream& out, UserSettingsOnStart value) {
  return out << ToString(value);
}

}  // namespace feed
