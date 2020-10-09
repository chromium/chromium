// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_ENUMS_H_
#define COMPONENTS_FEED_CORE_V2_ENUMS_H_

#include <iosfwd>

#include "components/feed/core/common/enums.h"

namespace feed {

enum class NetworkRequestType : int {
  kFeedQuery = 0,
  kUploadActions = 1,
};

// This must be kept in sync with FeedLoadStreamStatus in enums.xml.
enum class LoadStreamStatus {
  // Loading was not attempted.
  kNoStatus = 0,
  kLoadedFromStore = 1,
  kLoadedFromNetwork = 2,
  kFailedWithStoreError = 3,
  kNoStreamDataInStore = 4,
  kModelAlreadyLoaded = 5,
  kNoResponseBody = 6,
  kProtoTranslationFailed = 7,
  kDataInStoreIsStale = 8,
  // The timestamp for stored data is in the future, so we're treating stored
  // data as it it is stale.
  kDataInStoreIsStaleTimestampInFuture = 9,
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
  kMaxValue = kCannotLoadMoreNoNextPageToken,
};

std::ostream& operator<<(std::ostream& out, LoadStreamStatus value);

// Keep this in sync with FeedUploadActionsStatus in enums.xml.
enum class UploadActionsStatus {
  kNoStatus = 0,
  kNoPendingActions = 1,
  kFailedToStorePendingAction = 2,
  kStoredPendingAction = 3,
  kUpdatedConsistencyToken = 4,
  kFinishedWithoutUpdatingConsistencyToken = 5,
  kAbortUploadForSignedOutUser = 6,
  kAbortUploadBecauseDisabled = 7,
  kMaxValue = kAbortUploadBecauseDisabled,
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

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_ENUMS_H_
