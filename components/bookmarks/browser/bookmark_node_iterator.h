// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_ITERATOR_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_ITERATOR_H_

#include <stddef.h>

#include "base/containers/stack.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace bookmarks {

class BookmarkNode;

// Depth-first pre-order iterator over the descendants of a `BookmarkNode`
// (excluding `root` itself).
class BookmarkNodeIterator {
 public:
  using PruneCallback = base::RepeatingCallback<bool(const BookmarkNode*)>;

  // `root` must not be null. `root` and its descendants must outlive this
  // iterator and must not be structurally modified during traversal.
  explicit BookmarkNodeIterator(const BookmarkNode* root);

  // Same lifetime and non-null requirements on `root` as above. `prune` is an
  // optional predicate evaluated on each candidate node; if it returns true,
  // that node and all of its descendants are skipped.
  BookmarkNodeIterator(const BookmarkNode* root, PruneCallback prune);

  BookmarkNodeIterator(const BookmarkNodeIterator&) = delete;
  BookmarkNodeIterator& operator=(const BookmarkNodeIterator&) = delete;

  ~BookmarkNodeIterator();

  // Returns true if there are more descendants to visit.
  bool has_next() const { return !positions_.empty(); }

  // Returns the next descendant. Must only be called when `has_next()` is true.
  const BookmarkNode* Next();

 private:
  struct Position {
    raw_ptr<const BookmarkNode> node = nullptr;
    size_t index = 0;
  };

  void AdvanceToNextValidPosition();

  base::stack<Position> positions_;
  const PruneCallback prune_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_ITERATOR_H_
