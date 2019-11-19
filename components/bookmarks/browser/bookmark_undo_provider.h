// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UNDO_PROVIDER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UNDO_PROVIDER_H_

#include <memory>

namespace bookmarks {

class BookmarkNode;

// The interface for providing undo support.
class BookmarkUndoProvider {
 public:
  // Restores the previously removed |node| at |parent| in the specified
  // |index|.
  virtual void RestoreRemovedNode(const BookmarkNode* parent,
                                  size_t index,
                                  std::unique_ptr<BookmarkNode> node) = 0;

 protected:
  virtual ~BookmarkUndoProvider() {}
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UNDO_PROVIDER_H_
