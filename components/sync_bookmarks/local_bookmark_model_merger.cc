// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/local_bookmark_model_merger.h"

#include <utility>

#include "base/check.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_bookmarks {

LocalBookmarkModelMerger::LocalBookmarkModelMerger(
    const bookmarks::BookmarkModel* local_model,
    bookmarks::BookmarkModel* account_model)
    : local_model_(local_model), account_model_(account_model) {}

LocalBookmarkModelMerger::~LocalBookmarkModelMerger() = default;

void LocalBookmarkModelMerger::Merge() {
  // Match up the roots and recursively.
  MergeSubtree(/*local_subtree_root=*/local_model_->root_node(),
               /*account_subtree_root=*/account_model_->root_node());
}

void LocalBookmarkModelMerger::MergeSubtree(
    const bookmarks::BookmarkNode* local_subtree_root,
    const bookmarks::BookmarkNode* account_subtree_root) {
  CHECK(account_subtree_root);
  CHECK(local_subtree_root);
  CHECK_EQ(account_subtree_root->is_folder(), local_subtree_root->is_folder());

  // Special-case managed bookmarks, which don't need merging into the account
  // model.
  if (!local_model_->client()->CanSyncNode(local_subtree_root)) {
    return;
  }

  // If there are local child nodes, try to match them with account nodes.
  for (const auto& local_child_ptr : local_subtree_root->children()) {
    const bookmarks::BookmarkNode* const local_child = local_child_ptr.get();

    // TODO(crbug.com/1451511): Find matches by UUID and by semantics.

    // Special-case permanent folders, which already exists in both models.
    if (local_child->is_permanent_node()) {
      for (const auto& account_child_ptr : account_subtree_root->children()) {
        if (account_child_ptr->uuid() == local_child->uuid()) {
          CHECK(account_child_ptr->is_permanent_node());
          MergeSubtree(local_child, account_child_ptr.get());
          break;
        }
      }
      continue;
    }

    // Create a corresponding account node, which gets appended last.
    const bookmarks::BookmarkNode* account_node =
        CopyLocalNodeToAccountModelWithNewUuid(local_child,
                                               account_subtree_root);
    CHECK(account_node);

    MergeSubtree(local_child, account_node);
  }
}

const bookmarks::BookmarkNode*
LocalBookmarkModelMerger::CopyLocalNodeToAccountModelWithNewUuid(
    const bookmarks::BookmarkNode* local_node,
    const bookmarks::BookmarkNode* account_parent) {
  CHECK(local_node);
  CHECK(!local_node->is_permanent_node());
  CHECK(account_parent);

  // TODO(crbug.com/1451511): Reuse local UUID when possible.
  const size_t account_index = account_parent->children().size();

  // Note that this function is not expected to copy children recursively. The
  // caller is responsible for dealing with children.
  return local_node->is_folder()
             ? account_model_->AddFolder(
                   account_parent, account_index, local_node->GetTitle(),
                   local_node->GetMetaInfoMap(), local_node->date_added())
             : account_model_->AddURL(account_parent, account_index,
                                      local_node->GetTitle(), local_node->url(),
                                      local_node->GetMetaInfoMap(),
                                      local_node->date_added());
}

}  // namespace sync_bookmarks
