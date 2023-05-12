// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UNDO_DELEGATE_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UNDO_DELEGATE_H_

#include <memory>

namespace bookmarks {

class BookmarkModel;
class BookmarkNode;
class BookmarkUndoProvider;

// Delegate to handle bookmark change events in order to support undo when
// requested.
class BookmarkUndoDelegate {
 public:
  virtual ~BookmarkUndoDelegate() {}

  // Called when |node| was removed from |parent| at position |index|.
  virtual void OnBookmarkNodeRemoved(BookmarkModel* model,
                                     BookmarkUndoProvider* undo_provider,
                                     const BookmarkNode* parent,
                                     size_t index,
                                     std::unique_ptr<BookmarkNode> node) = 0;
};


}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UNDO_DELEGATE_H_
