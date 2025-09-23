// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_INITIAL_ACCOUNT_BOOKMARK_DEDUPLICATOR_H_
#define COMPONENTS_SYNC_BOOKMARKS_INITIAL_ACCOUNT_BOOKMARK_DEDUPLICATOR_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace sync_bookmarks {

// A class responsible for removing local bookmark subtrees that are duplicates
// of subtrees in the account storage. This is used upon initial sync to avoid
// creating redundant bookmarks when a user signs in to a new device that may
// have pre-existing local bookmarks. The local copy of the duplicated subtree
// is the one that gets removed.
class InitialAccountBookmarkDeduplicator {
 public:
  // `bookmark_model` must not be null and must outlive this object.
  explicit InitialAccountBookmarkDeduplicator(
      bookmarks::BookmarkModel* bookmark_model);
  ~InitialAccountBookmarkDeduplicator();

  InitialAccountBookmarkDeduplicator(
      const InitialAccountBookmarkDeduplicator&) = delete;
  InitialAccountBookmarkDeduplicator& operator=(
      const InitialAccountBookmarkDeduplicator&) = delete;

  // Finds and removes bookmark subtrees from local storage that are duplicates
  // of subtrees in account storage.
  //
  // Removes direct children (and their descendants, if any) of the local
  // permanent nodes (e.g., the Bookmarks Bar). A direct child is removed if it
  // matches an account child under the same permanent parent (by UUID, URL, and
  // title), and the tree rooted at the local child is a subgraph of the tree
  // rooted at the account child.
  void Deduplicate();

  bool DoesAccountSubgraphContainLocalSubgraphForTest(
      const bookmarks::BookmarkNode* local_subtree_root,
      const bookmarks::BookmarkNode* account_subtree_root) const;

 private:
  // Returns true if the `local_subtree_root` matches the
  // `account_subtree_root`, and the tree rooted at the `local_subtree_root` is
  // a subgraph of the tree rooted at the `account_subtree_root`.
  bool DoesAccountSubgraphContainLocalSubgraph(
      const bookmarks::BookmarkNode* local_subtree_root,
      const bookmarks::BookmarkNode* account_subtree_root) const;

  // Removes an arbitrary set of nodes among siblings under `parent`.
  void RemoveChildrenAt(const bookmarks::BookmarkNode* parent,
                        const base::flat_set<size_t>& indices_to_remove);

  const raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_INITIAL_ACCOUNT_BOOKMARK_DEDUPLICATOR_H_
