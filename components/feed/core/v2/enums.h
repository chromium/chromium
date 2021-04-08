// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_ENUMS_H_
#define COMPONENTS_FEED_CORE_V2_ENUMS_H_

#include <iosfwd>

namespace feed {

// One value for each network API method used by the feed.
enum class NetworkRequestType : int {
  kFeedQuery = 0,
  kUploadActions = 1,
  kNextPage = 2,
  kListWebFeeds = 3,
  kUnfollowWebFeed = 4,
  kFollowWebFeed = 5,
  kListRecommendedWebFeeds = 6,
};

// This must be kept in sync with FeedLoadStreamStatus in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.v2
enum class LoadStreamStatus {
  // Loading was not attempted.
  kNoStatus = 0,

  // Final loading statuses where loading succeeds. :
  kLoadedFromStore = 1,
  kLoadedFromNetwork = 2,
  kLoadedStaleDataFromStoreDueToNetworkFailure = 21,

  // Statuses where data is loaded from the persistent store, but is stale.
  kDataInStoreIsStale = 8,
  // The timestamp for stored data is in the future, so we're treating stored
  // data as it it is stale.
  kDataInStoreIsStaleTimestampInFuture = 9,
  kDataInStoreStaleMissedLastRefresh = 20,

  // Failure statuses where content is not loaded.
  kFailedWithStoreError = 3,
  kNoStreamDataInStore = 4,
  kModelAlreadyLoaded = 5,
  kNoResponseBody = 6,
  kProtoTranslationFailed = 7,
  kCannotLoadFromNetworkSupressedForHistoryDelete_DEPRECATED = 10,
  kCannotLoadFromNetworkOffline = 11,
  kCannotLoadFromNetworkThrottled = 12,
  kLoadNotAllowedEulaNotAccepted = 13,
  kLoadNotAllowedArticlesListHidden = 14,
  kCannotParseNetworkResponseBody = 15,
  kLoadMoreModelIsNotLoaded = 16,
  kLoadNotAllowedDisabledByEnterprisePolicy = 17,
  kNetworkFetchFailed = 18,
  kCannotLoadMoreNoNextPageToken = 19,
  kDataInStoreIsExpired = 22,
  kDataInStoreIsForAnotherUser = 23,
  kAbortWithPendingClearAll = 24,
  kMaxValue = kAbortWithPendingClearAll,
};

std::ostream& operator<<(std::ostream& out, LoadStreamStatus value);

// Keep this in sync with FeedUploadActionsStatus in enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feed.v2
enum class UploadActionsStatus {
  kNoStatus = 0,
  kNoPendingActions = 1,
  kFailedToStorePendingAction = 2,
  kStoredPendingAction = 3,
  kUpdatedConsistencyToken = 4,
  kFinishedWithoutUpdatingConsistencyToken = 5,
  kAbortUploadForSignedOutUser = 6,
  kAbortUploadBecauseDisabled = 7,
  kAbortUploadForWrongUser = 8,
  kAbortUploadActionsWithPendingClearAll = 9,
  kMaxValue = kAbortUploadActionsWithPendingClearAll,
};

// Keep this in sync with FeedUploadActionsBatchStatus in enums.xml.
enum class UploadActionsBatchStatus {
  kNoStatus = 0,
  kFailedToUpdateStore = 1,
  kFailedToUpload = 2,
  kFailedToRemoveUploadedActions = 3,
  kExhaustedUploadQuota = 4,
  kAllActionsWereStale = 5,
  kSuccessfullyUploadedBatch = 6,
  kMaxValue = kSuccessfullyUploadedBatch,
};

std::ostream& operator<<(std::ostream& out, UploadActionsStatus value);
std::ostream& operator<<(std::ostream& out, UploadActionsBatchStatus value);

// Status of updating recommended or subscribed web feeds.
enum class WebFeedRefreshStatus {
  kNoStatus = 0,
  kSuccess = 1,
  kNetworkFailure = 2,
  kNetworkRequestThrottled = 3,
  kAbortFetchWebFeedPendingClearAll = 4,
};
std::ostream& operator<<(std::ostream& out, WebFeedRefreshStatus value);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_ENUMS_H_
