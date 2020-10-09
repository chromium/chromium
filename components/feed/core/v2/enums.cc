// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/enums.h"

#include <ostream>

namespace feed {

// Included for debug builds only for reduced binary size.

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
  }
#else
  return out << (static_cast<int>(value));
#endif  // ifndef NDEBUG
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

}  // namespace feed
