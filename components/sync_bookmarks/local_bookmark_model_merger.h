// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_MODEL_MERGER_H_
#define COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_MODEL_MERGER_H_

#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace sync_bookmarks {

class BookmarkModelView;

// Class responsible for implementing the merge algorithm that allows moving all
// bookmarks from the local BookmarkModel instance to the account one.
class LocalBookmarkModelMerger {
 public:
  // All parameters must not be null and must outlive this object.
  // The observer should itself take care.
  LocalBookmarkModelMerger(const BookmarkModelView* local_model,
                           BookmarkModelView* account_model);

  LocalBookmarkModelMerger(const LocalBookmarkModelMerger&) = delete;
  LocalBookmarkModelMerger& operator=(const LocalBookmarkModelMerger&) = delete;

  ~LocalBookmarkModelMerger();

  // Merges local bookmarks into the account model (both models injected in
  // constructor).
  void Merge();

 private:
  // Represents a pair of bookmarks, one in the source model and one in the
  // destination one, that have been matched by UUID. They are guaranteed to
  // have the same type and URL (if applicable).
  struct GuidMatch {
    raw_ptr<const bookmarks::BookmarkNode> local_node = nullptr;
    raw_ptr<const bookmarks::BookmarkNode> account_node = nullptr;
  };

  // Computes bookmark pairs that should be matched by UUID. Note that matches
  // may be incompatible, that is, if only one of the two is a folder.
  static std::unordered_map<base::Uuid, GuidMatch, base::UuidHash>
  FindGuidMatches(const BookmarkModelView* local_model,
                  const BookmarkModelView* account_model);

  // Merges a local and a account subtrees. The input nodes are two equivalent
  // local and remote nodes. This method tries to recursively match their
  // children.
  void MergeSubtree(const bookmarks::BookmarkNode* local_subtree_root,
                    const bookmarks::BookmarkNode* account_subtree_root);

  // Updates `account_node` to hold the same semantics as `local_node`, which
  // excludes the UUID (remains unchanged). `account_node` must not be a
  // BookmarkPermanentNode.
  void UpdateAccountNodeFromMatchingLocalNode(
      const bookmarks::BookmarkNode* local_node,
      const bookmarks::BookmarkNode* account_node);

  // Copies a bookmark node from the local model to the account model. It works
  // for both URL and folder nodes, but the latter case doesn't do anything
  // about children (if any), the caller is responsible for copying over the
  // children if desired.
  const bookmarks::BookmarkNode* CopyLocalNodeToAccountModel(
      const bookmarks::BookmarkNode* local_node,
      const bookmarks::BookmarkNode* account_parent);

  // If `account_node` has a local counterpart of the same UUID, returns the
  // corresponding local node, otherwise returns a nullptr. `account_node` must
  // not be null.
  const bookmarks::BookmarkNode* FindMatchingLocalNodeByUuid(
      const bookmarks::BookmarkNode* account_node) const;

  // If `local_node` has an account counterpart of the same UUID, returns the
  // corresponding account node, otherwise returns a nullptr.
  const bookmarks::BookmarkNode* FindMatchingAccountNodeByUuid(
      const bookmarks::BookmarkNode* local_node) const;

  const raw_ptr<const BookmarkModelView> local_model_;
  const raw_ptr<BookmarkModelView> account_model_;
  const std::unordered_map<base::Uuid, GuidMatch, base::UuidHash>
      uuid_to_match_map_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_MODEL_MERGER_H_
