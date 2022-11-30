// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_SCOPED_GROUP_BOOKMARK_ACTIONS_H_
#define COMPONENTS_BOOKMARKS_BROWSER_SCOPED_GROUP_BOOKMARK_ACTIONS_H_

#include "base/memory/raw_ptr.h"

namespace bookmarks {

class BookmarkModel;

// Scopes the grouping of a set of changes into one undoable action.
class ScopedGroupBookmarkActions {
 public:
  explicit ScopedGroupBookmarkActions(BookmarkModel* model);

  ScopedGroupBookmarkActions(const ScopedGroupBookmarkActions&) = delete;
  ScopedGroupBookmarkActions& operator=(const ScopedGroupBookmarkActions&) =
      delete;

  ~ScopedGroupBookmarkActions();

 private:
  raw_ptr<BookmarkModel> model_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_SCOPED_GROUP_BOOKMARK_ACTIONS_H_
