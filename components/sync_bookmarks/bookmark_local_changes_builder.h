// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_CHANGES_BUILDER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_CHANGES_BUILDER_H_

#include "base/memory/raw_ptr.h"
#include "components/sync/engine/commit_and_get_updates_types.h"

namespace sync_bookmarks {

class BookmarkModelView;
class SyncedBookmarkTracker;

class BookmarkLocalChangesBuilder {
 public:
  // |bookmark_tracker| and |bookmark_model| must not be null and must outlive
  // this object.
  BookmarkLocalChangesBuilder(SyncedBookmarkTracker* bookmark_tracker,
                              BookmarkModelView* bookmark_model);

  BookmarkLocalChangesBuilder(const BookmarkLocalChangesBuilder&) = delete;
  BookmarkLocalChangesBuilder& operator=(const BookmarkLocalChangesBuilder&) =
      delete;

  // Builds the commit requests list.
  syncer::CommitRequestDataList BuildCommitRequests(size_t max_entries) const;

 private:
  const raw_ptr<SyncedBookmarkTracker> bookmark_tracker_;
  const raw_ptr<BookmarkModelView> bookmark_model_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_LOCAL_CHANGES_BUILDER_H_
