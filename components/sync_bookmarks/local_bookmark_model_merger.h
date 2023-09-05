// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_MODEL_MERGER_H_
#define COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_MODEL_MERGER_H_

#include "base/memory/raw_ptr.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace sync_bookmarks {

// Class responsible for implementing the merge algorithm that allows moving all
// bookmarks from the local BookmarkModel instance to the account one.
class LocalBookmarkModelMerger {
 public:
  // All parameters must not be null and must outlive this object.
  // The observer should itself take care.
  LocalBookmarkModelMerger(const bookmarks::BookmarkModel* local_model,
                           bookmarks::BookmarkModel* account_model);

  LocalBookmarkModelMerger(const LocalBookmarkModelMerger&) = delete;
  LocalBookmarkModelMerger& operator=(const LocalBookmarkModelMerger&) = delete;

  ~LocalBookmarkModelMerger();

  // Merges local bookmarks into the account model (both models injected in
  // constructor).
  void Merge();

 private:
  // Merges a local and a account subtrees. The input nodes are two equivalent
  // local and remote nodes. This method tries to recursively match their
  // children.
  void MergeSubtree(const bookmarks::BookmarkNode* local_subtree_root,
                    const bookmarks::BookmarkNode* account_subtree_root);

  // Copies a bookmark node from the local model to the account model. It works
  // for both URL and folder nodes, but the latter case doesn't do anything
  // about children (if any), the caller is responsible for copying over the
  // children if desired.
  const bookmarks::BookmarkNode* CopyLocalNodeToAccountModelWithNewUuid(
      const bookmarks::BookmarkNode* local_node,
      const bookmarks::BookmarkNode* account_parent);

  const raw_ptr<const bookmarks::BookmarkModel> local_model_;
  const raw_ptr<bookmarks::BookmarkModel> account_model_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_MODEL_MERGER_H_
