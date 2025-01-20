// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_TO_ACCOUNT_MERGER_H_
#define COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_TO_ACCOUNT_MERGER_H_

#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"

namespace base {
class Location;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace sync_bookmarks {

// Class responsible for implementing the merge algorithm that allows moving all
// or some local bookmarks to become account bookmarks, with the ability to
// dedup data.
class LocalBookmarkToAccountMerger {
 public:
  // `model` must not be null and must outlive this object. It must also be
  // loaded and with existing permanent folders for account bookmarks.
  explicit LocalBookmarkToAccountMerger(bookmarks::BookmarkModel* model);

  LocalBookmarkToAccountMerger(const LocalBookmarkToAccountMerger&) = delete;
  LocalBookmarkToAccountMerger& operator=(const LocalBookmarkToAccountMerger&) =
      delete;

  ~LocalBookmarkToAccountMerger();

  // Merges all local bookmarks into account bookmarks.
  void MoveAndMergeAllNodes();

  // Merges a specified subset of local bookmarks into account bookmarks.
  //
  // `child_ids_to_merge` contains a list of `BookmarkNode::id()` values. Only
  // the immediate children of the permanent local folders with these IDs (and
  // their descendents) will be merged.
  //
  // Any IDs that aren't immediate children of the permanent local folders will
  // simply be ignored (i.e. the caller does not need to prune them).
  void MoveAndMergeSpecificSubtrees(
      std::set<int64_t> permanent_folder_child_ids_to_merge);

  // Exposed publicly for testing only.
  void RemoveChildrenAtForTesting(const bookmarks::BookmarkNode* parent,
                                  const std::vector<size_t>& indices_to_remove,
                                  const base::Location& location);

 private:
  // Removes an arbitrary set of nodes among siblings under `parent`, as
  // selected by `indices_to_remove`. `location` is used for logging purposes
  // and investigations.
  void RemoveChildrenAt(const bookmarks::BookmarkNode* parent,
                        const base::flat_set<size_t>& indices_to_remove,
                        const base::Location& location);

  // Internal implementation containing shared logic for `MoveAndMergeAllNodes`
  // and `MoveAndMergeSpecificSubtrees`. `child_ids_to_merge` is nullopt for
  // `MoveAndMergeAllNodes` and contains the list of IDs for
  // `MoveAndMergeSpecificSubtrees`.
  void MoveAndMergeInternal(
      std::optional<std::set<int64_t>> child_ids_to_merge);

  // Moves descendants under `local_subtree_root`, excluding the root itself, to
  // append them under `account_subtree_root`. This method tries to remove
  // duplicates by UUID or similarity, which results in merging nodes. Both
  // `local_subtree_root` and `account_subtree_root` must not be null.
  //
  // An optional `local_child_ids_to_merge` can be provided, containing a list
  // of `BookmarkNode::id()` values. If provided, only the immediate children of
  // `local_subtree_root` with these IDs (and their descendents) will be merged.
  // If not provided, all descendants of `local_subtree_root` will be merged.
  //
  // If this is a recursive call (i.e. the parent of `local_subtree_root` is
  // being moved), `local_child_ids_to_merge` must be nullopt.
  void MoveOrMergeDescendants(
      const bookmarks::BookmarkNode* local_subtree_root,
      const bookmarks::BookmarkNode* account_subtree_root,
      std::optional<std::set<int64_t>> local_child_ids_to_merge);

  // Moves descendants under `local_subtree_root` if they match an account node
  // by UUID. The remaining local nodes are left unmodified, i.e. those that
  // didn't match by UUID and their ancestors didn't either. This is useful when
  // a local subtree is about to be moved to account storage because its root
  // node didn't match any account node: descendants should move as part of this
  // subtree move, *except* those which have a UUID-based match, which should
  // take precedence.
  void MergeAndDeleteDescendantsThatMatchByUuid(
      const bookmarks::BookmarkNode* local_subtree_root);

  // Updates `account_node` to hold the same semantics as `local_node`, which
  // excludes the UUID (remains unchanged). `account_node` must not be a
  // BookmarkPermanentNode.
  void UpdateAccountNodeFromMatchingLocalNode(
      const bookmarks::BookmarkNode* local_node,
      const bookmarks::BookmarkNode* account_node);

  // If `account_node` has a local counterpart of the same UUID, returns the
  // corresponding local node, otherwise returns a nullptr. `account_node` must
  // not be null.
  const bookmarks::BookmarkNode* FindMatchingLocalNodeByUuid(
      const bookmarks::BookmarkNode* account_node) const;

  // If `local_node` has an account counterpart of the same UUID, returns the
  // corresponding account node, otherwise returns a nullptr.
  const bookmarks::BookmarkNode* FindMatchingAccountNodeByUuid(
      const bookmarks::BookmarkNode* local_node) const;

  const raw_ptr<bookmarks::BookmarkModel> model_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_LOCAL_BOOKMARK_TO_ACCOUNT_MERGER_H_
