// Copyright 2015 The Chromium Authors. All rights reserved.
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

  // Sets the provider that will do the undo work.
  virtual void SetUndoProvider(BookmarkUndoProvider* provider) = 0;

  // Called when |node| was removed from |parent| at position |index|.
  virtual void OnBookmarkNodeRemoved(BookmarkModel* model,
                                     const BookmarkNode* parent,
                                     size_t index,
                                     std::unique_ptr<BookmarkNode> node) = 0;
};


}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UNDO_DELEGATE_H_
