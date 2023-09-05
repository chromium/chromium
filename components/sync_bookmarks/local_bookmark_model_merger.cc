// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/local_bookmark_model_merger.h"

#include <utility>

#include "base/check.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_bookmarks {

namespace {

bool NodesCompatibleForMatchByUuid(const bookmarks::BookmarkNode* node1,
                                   const bookmarks::BookmarkNode* node2) {
  CHECK_EQ(node1->uuid(), node2->uuid());

  if (node1->is_folder() != node2->is_folder()) {
    return false;
  }

  if (!node2->is_folder() && node1->url() != node2->url()) {
    return false;
  }

  // Note that the title isn't required to be equal, which also means that two
  // folders don't have additional requirements, if their UUIDs are equal.
  return true;
}

}  // namespace

LocalBookmarkModelMerger::LocalBookmarkModelMerger(
    const bookmarks::BookmarkModel* local_model,
    bookmarks::BookmarkModel* account_model)
    : local_model_(local_model),
      account_model_(account_model),
      uuid_to_match_map_(FindGuidMatches(local_model, account_model)) {}

LocalBookmarkModelMerger::~LocalBookmarkModelMerger() = default;

void LocalBookmarkModelMerger::Merge() {
  // Match up the roots and recursively.
  MergeSubtree(/*local_subtree_root=*/local_model_->root_node(),
               /*account_subtree_root=*/account_model_->root_node());
}

// static
std::unordered_map<base::Uuid,
                   LocalBookmarkModelMerger::GuidMatch,
                   base::UuidHash>
LocalBookmarkModelMerger::FindGuidMatches(
    const bookmarks::BookmarkModel* local_model,
    const bookmarks::BookmarkModel* account_model) {
  CHECK(local_model);
  CHECK(account_model);

  // Build a temporary lookup table for account UUIDs.
  std::unordered_map<base::Uuid, const bookmarks::BookmarkNode*, base::UuidHash>
      uuid_to_local_node_map;
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> local_iterator(
      local_model->root_node());
  while (local_iterator.has_next()) {
    const bookmarks::BookmarkNode* const node = local_iterator.Next();
    CHECK(node->uuid().is_valid());
    // Exclude managed nodes.
    if (!node->is_permanent_node() &&
        !local_model->client()->CanSyncNode(node)) {
      continue;
    }

    const bool success =
        uuid_to_local_node_map.emplace(node->uuid(), node).second;
    CHECK(success);
  }

  // Iterate through all account bookmarks to find matches by UUID.
  std::unordered_map<base::Uuid, LocalBookmarkModelMerger::GuidMatch,
                     base::UuidHash>
      uuid_to_match_map;
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> account_iterator(
      account_model->root_node());
  while (account_iterator.has_next()) {
    const bookmarks::BookmarkNode* const account_node = account_iterator.Next();
    CHECK(account_node->uuid().is_valid());

    // Exclude managed nodes (although the account model is not expected to have
    // them).
    if (!account_node->is_permanent_node() &&
        !account_model->client()->CanSyncNode(account_node)) {
      continue;
    }

    const auto local_it = uuid_to_local_node_map.find(account_node->uuid());
    if (local_it == uuid_to_local_node_map.end()) {
      // No match found by UUID.
      continue;
    }

    const bookmarks::BookmarkNode* const local_node = local_it->second;
    if (NodesCompatibleForMatchByUuid(account_node, local_node)) {
      const bool success = uuid_to_match_map
                               .emplace(account_node->uuid(),
                                        GuidMatch{local_node, account_node})
                               .second;
      CHECK(success);
    }
  }

  return uuid_to_match_map;
}

void LocalBookmarkModelMerger::MergeSubtree(
    const bookmarks::BookmarkNode* local_subtree_root,
    const bookmarks::BookmarkNode* account_subtree_root) {
  CHECK(account_subtree_root);
  CHECK(local_subtree_root);
  CHECK_EQ(account_subtree_root->is_folder(), local_subtree_root->is_folder());
  CHECK_EQ(account_subtree_root->is_permanent_node(),
           local_subtree_root->is_permanent_node());

  // If there are local child nodes, try to match them with account nodes.
  for (const auto& local_child_ptr : local_subtree_root->children()) {
    const bookmarks::BookmarkNode* const local_child = local_child_ptr.get();

    // Special-case managed bookmarks, which don't need merging into the account
    // model.
    if (!local_model_->client()->CanSyncNode(local_child)) {
      continue;
    }

    // Try to match by UUID.
    const bookmarks::BookmarkNode* matching_account_node =
        FindMatchingAccountNodeByUuid(local_child);

    // TODO(crbug.com/1451511): Find matches by semantics if the UUID-based
    // matching failed.

    if (local_child->is_permanent_node()) {
      // Permanent nodes must have matched by UUID and don't need updating other
      // than recursively iterating their descendants.
      CHECK(matching_account_node);
      CHECK(matching_account_node->is_permanent_node());
    } else if (matching_account_node) {
      // If a match was found, update the title and possible other fields.
      CHECK(!matching_account_node->is_permanent_node());
      CHECK(NodesCompatibleForMatchByUuid(local_child, matching_account_node));
      UpdateAccountNodeFromMatchingLocalNode(local_child,
                                             matching_account_node);
    } else {
      // If no match found, create a corresponding account node, which gets
      // appended last.
      matching_account_node = CopyLocalNodeToAccountModelWithNewUuid(
          local_child, account_subtree_root);
      CHECK(matching_account_node);
    }

    // Since nodes are matching, their subtrees should be merged as well.
    MergeSubtree(local_child, matching_account_node);
  }
}

void LocalBookmarkModelMerger::UpdateAccountNodeFromMatchingLocalNode(
    const bookmarks::BookmarkNode* local_node,
    const bookmarks::BookmarkNode* account_node) {
  CHECK(local_node);
  CHECK(account_node);
  CHECK(!local_node->is_permanent_node());
  CHECK(!account_node->is_permanent_node());
  CHECK(NodesCompatibleForMatchByUuid(local_node, account_node));

  // Update all fields, where no-op changes are handled well.
  // The meta-info map is intentionally excluded, since the desired behavior is
  // unclear.
  if (local_node->date_last_used() > account_node->date_last_used()) {
    account_model_->UpdateLastUsedTime(
        account_node, local_node->date_last_used(), /*just_opened=*/false);
  }

  // For the title, use the local one.
  account_model_->SetTitle(account_node, local_node->GetTitle(),
                           bookmarks::metrics::BookmarkEditSource::kOther);
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

const bookmarks::BookmarkNode*
LocalBookmarkModelMerger::FindMatchingLocalNodeByUuid(
    const bookmarks::BookmarkNode* account_node) const {
  CHECK(account_node);

  const auto it = uuid_to_match_map_.find(account_node->uuid());
  if (it == uuid_to_match_map_.end()) {
    return nullptr;
  }

  const bookmarks::BookmarkNode* local_node = it->second.local_node;
  CHECK(local_node);
  CHECK_EQ(it->second.account_node, account_node);
  CHECK(NodesCompatibleForMatchByUuid(local_node, account_node));

  return local_node;
}

const bookmarks::BookmarkNode*
LocalBookmarkModelMerger::FindMatchingAccountNodeByUuid(
    const bookmarks::BookmarkNode* local_node) const {
  CHECK(local_node);

  const auto it = uuid_to_match_map_.find(local_node->uuid());
  if (it == uuid_to_match_map_.end()) {
    return nullptr;
  }

  const bookmarks::BookmarkNode* account_node = it->second.account_node;
  CHECK(account_node);
  CHECK_EQ(it->second.local_node, local_node);
  CHECK(NodesCompatibleForMatchByUuid(local_node, account_node));

  return account_node;
}

}  // namespace sync_bookmarks
