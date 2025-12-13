// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_BOOKMARK_SYNC_ERROR_STATE_H_
#define COMPONENTS_SYNC_SERVICE_BOOKMARK_SYNC_ERROR_STATE_H_

namespace syncer {

class SyncError;

class BookmarkSyncErrorState {
 public:
  BookmarkSyncErrorState();
  ~BookmarkSyncErrorState();

  // Returns true if the error is a bookmark limit exceeded error and it hasn't
  // been acknowledged by the user yet.
  bool IsActionableError(const SyncError& error) const;

  // Acknowledges the bookmarks limit exceeded error, so it will not be
  // considered actionable again until the next browser restart.
  void AcknowledgeError();

 private:
  bool error_acknowledged_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_BOOKMARK_SYNC_ERROR_STATE_H_
