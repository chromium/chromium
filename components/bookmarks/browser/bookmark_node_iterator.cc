// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_iterator.h"

#include <utility>

#include "base/check.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks {

BookmarkNodeIterator::BookmarkNodeIterator(const BookmarkNode* root)
    : BookmarkNodeIterator(root, PruneCallback()) {}

BookmarkNodeIterator::BookmarkNodeIterator(const BookmarkNode* root,
                                           PruneCallback prune)
    : prune_(std::move(prune)) {
  CHECK(root);
  if (!root->children().empty()) {
    positions_.push({root, 0});
    AdvanceToNextValidPosition();
  }
}

BookmarkNodeIterator::~BookmarkNodeIterator() = default;

const BookmarkNode* BookmarkNodeIterator::Next() {
  CHECK(has_next());

  const BookmarkNode* result =
      positions_.top().node->children()[positions_.top().index].get();
  ++positions_.top().index;

  if (!result->children().empty()) {
    positions_.push({result, 0});
  }

  AdvanceToNextValidPosition();
  return result;
}

void BookmarkNodeIterator::AdvanceToNextValidPosition() {
  while (!positions_.empty()) {
    Position& top = positions_.top();
    if (top.index >= top.node->children().size()) {
      positions_.pop();
    } else if (!prune_.is_null() &&
               prune_.Run(top.node->children()[top.index].get())) {
      ++top.index;
    } else {
      break;
    }
  }
}

}  // namespace bookmarks
