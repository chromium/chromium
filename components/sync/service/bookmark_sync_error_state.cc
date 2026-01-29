// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/bookmark_sync_error_state.h"

#include "components/sync/model/model_error.h"
#include "components/sync/service/sync_error.h"

namespace syncer {

// Returns true if the error is a bookmark limit exceeded error.
bool IsBookmarksLimitExceededError(const SyncError& error) {
  if (error.error_type() != SyncError::MODEL_ERROR ||
      !error.model_error_type().has_value()) {
    return false;
  }

  switch (error.model_error_type().value()) {
    case ModelError::Type::kBookmarksRemoteCountExceededLimitInitialMerge:
    case ModelError::Type::kBookmarksRemoteCountExceededLimitLastInitialMerge:
    case ModelError::Type::kBookmarksLocalCountExceededLimitOnSyncStart:
    case ModelError::Type::kBookmarksLocalCountExceededLimitOnUpdateReceived:
    case ModelError::Type::kBookmarksLocalCountExceededLimitNudgeForCommit:
      return true;
    default:
      return false;
  }
}

BookmarkSyncErrorState::BookmarkSyncErrorState() = default;
BookmarkSyncErrorState::~BookmarkSyncErrorState() = default;

bool BookmarkSyncErrorState::IsActionableError(const SyncError& error) const {
  return IsBookmarksLimitExceededError(error) && !error_acknowledged_;
}

void BookmarkSyncErrorState::AcknowledgeError() {
  error_acknowledged_ = true;
}

}  // namespace syncer
