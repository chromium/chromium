// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SCOPED_BOOKMARK_EXTENSIVE_CHANGES_H_
#define COMPONENTS_SYNC_BOOKMARKS_SCOPED_BOOKMARK_EXTENSIVE_CHANGES_H_

#include "base/memory/raw_ref.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace sync_bookmarks {

// Scopes an extensive changes block on `bookmarks::BookmarkModel`, calling
// `BeginExtensiveChanges()` on construction and `EndExtensiveChanges()` on
// destruction. This notifies UI-intensive observers of `BookmarkModel` that
// potentially significant changes are about to happen, so updates may be
// batched (for example, on Mac, the bookmarks bar displays animations when
// bookmark items are added or deleted).
class ScopedBookmarkExtensiveChanges {
 public:
  explicit ScopedBookmarkExtensiveChanges(bookmarks::BookmarkModel& model);

  ScopedBookmarkExtensiveChanges(const ScopedBookmarkExtensiveChanges&) =
      delete;
  ScopedBookmarkExtensiveChanges& operator=(
      const ScopedBookmarkExtensiveChanges&) = delete;

  ~ScopedBookmarkExtensiveChanges();

 private:
  const raw_ref<bookmarks::BookmarkModel> model_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_SCOPED_BOOKMARK_EXTENSIVE_CHANGES_H_
