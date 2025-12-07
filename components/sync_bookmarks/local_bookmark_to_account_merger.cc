// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/local_bookmark_to_account_merger.h"

#include <algorithm>
#include <cstddef>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/switches.h"
#include "ui/base/models/tree_node_iterator.h"
#include "url/gurl.h"

namespace sync_bookmarks {

namespace {

constexpr bookmarks::metrics::BookmarkEditSource kEditSourceForMetrics =
    bookmarks::metrics::BookmarkEditSource::kOther;

// Struct representing a subset of fields of BookmarkNode, such that two nodes
// with the same parent are considered a semantic match if the
// SiblingSemanticMatchKey value computed for them are equal.
struct SiblingSemanticMatchKey {
  // Bookmarked URL or nullopt for folders. This also means a URL node never
  // matches semantically with a folder.
  std::optional<GURL> url;
  // Title equality is required, but the fact that Sync used to truncate the
  // title to a maximum size is incorporated here (i.e. the truncated title is
  // represented here).
  std::string canonicalized_sync_title;
};

struct SiblingSemanticMatchKeyHash {
  size_t operator()(const SiblingSemanticMatchKey& key) const {
    return base::FastHash(key.canonicalized_sync_title) ^
           (key.url.has_value()
                ? base::FastHash(key.url->possibly_invalid_spec())
                : size_t(1));
  }
};

struct SiblingSemanticMatchKeyEquals {
  size_t operator()(const SiblingSemanticMatchKey& lhs,
                    const SiblingSemanticMatchKey& rhs) const {
    return lhs.url == rhs.url &&
           lhs.canonicalized_sync_title == rhs.canonicalized_sync_title;
  }
};

SiblingSemanticMatchKey GetSiblingSemanticMatchKeyForNode(
    const bookmarks::BookmarkNode* node) {
  SiblingSemanticMatchKey key;
  if (node->is_url()) {
    key.url = node->url();
  }
  key.canonicalized_sync_title =
      sync_bookmarks::FullTitleToLegacyCanonicalizedTitle(
          base::UTF16ToUTF8(node->GetTitle()));
  return key;
}

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

// Returns a vector with all user-editable permanent nodes, grouped in pairs
// where the first element is the local permanent node and the second one is
// the account counterpart.
std::vector<std::pair<raw_ptr<const bookmarks::BookmarkNode>,
                      raw_ptr<const bookmarks::BookmarkNode>>>
GetLocalAndAccountPermanentNodePairs(const bookmarks::BookmarkModel* model) {
  CHECK(model);
  return {{model->bookmark_bar_node(), model->account_bookmark_bar_node()},
          {model->other_node(), model->account_other_node()},
          {model->mobile_node(), model->account_mobile_node()}};
}

}  // namespace

LocalBookmarkToAccountMerger::LocalBookmarkToAccountMerger(
    bookmarks::BookmarkModel* model)
    : model_(model) {
  CHECK(model_);
  CHECK(model_->loaded());
  for (const auto& [local_permanent_node, account_permanent_node] :
       GetLocalAndAccountPermanentNodePairs(model)) {
    CHECK(local_permanent_node);
    CHECK(account_permanent_node);
  }
}

LocalBookmarkToAccountMerger::~LocalBookmarkToAccountMerger() = default;

void LocalBookmarkToAccountMerger::MoveAndMergeAllNodes() {
  MoveAndMergeInternal(/*child_ids_to_merge=*/std::nullopt);
}

void LocalBookmarkToAccountMerger::MoveAndMergeSpecificSubtrees(
    std::set<int64_t> permanent_folder_child_ids_to_merge) {
  MoveAndMergeInternal(std::move(permanent_folder_child_ids_to_merge));
}

void LocalBookmarkToAccountMerger::MoveAndMergeInternal(
    std::optional<std::set<int64_t>> permanent_folder_child_ids_to_merge) {
  // Notify UI intensive observers of BookmarkModel that we are about to make
  // potentially significant changes to it, so the updates may be batched. For
  // example, on Mac, the bookmarks bar displays animations when bookmark items
  // are added or deleted.
  model_->BeginExtensiveChanges();

  // Algorithm description:
  // Match up the roots and recursively do the following:
  // * For each local node for the current local parent node, either
  //   find an account node with equal UUID anywhere throughout the tree or find
  //   the best matching bookmark node under the corresponding account bookmark
  //   parent node using semantics. If the found node has the same UUID as a
  //   different local bookmark, it is not considered a semantics match, as
  //   UUID matching takes precedence.
  // * If no matching node is found, move the local node to account storage by
  //   appending it last.
  // * If a matching node is found, update the properties of it from the
  //   corresponding local node.
  //
  // The semantics best match algorithm uses folder title or bookmark title/url
  // to perform the primary match. If there are multiple match candidates it
  // selects the first one.
  for (const auto& [local_permanent_node, account_permanent_node] :
       GetLocalAndAccountPermanentNodePairs(model_)) {
    CHECK(local_permanent_node);
    CHECK(account_permanent_node);
    MoveOrMergeDescendants(
        /*local_subtree_root=*/local_permanent_node,
        /*account_subtree_root=*/account_permanent_node,
        permanent_folder_child_ids_to_merge);
  }

  // Any remaining local node may only be explained because
  // `permanent_folder_child_ids_to_merge` is non-empty and excluded such node.
  for (const auto& [local_permanent_node, unused] :
       GetLocalAndAccountPermanentNodePairs(model_)) {
    CHECK(std::ranges::all_of(
        local_permanent_node->children(),
        [&permanent_folder_child_ids_to_merge](const auto& child) {
          return permanent_folder_child_ids_to_merge.has_value() &&
                 !permanent_folder_child_ids_to_merge->contains(child->id());
        }));
  }

  model_->EndExtensiveChanges();
}

void LocalBookmarkToAccountMerger::RemoveChildrenAtForTesting(
    const bookmarks::BookmarkNode* parent,
    const std::vector<size_t>& indices_to_remove,
    const base::Location& location) {
  RemoveChildrenAt(parent, indices_to_remove, location);
}

void LocalBookmarkToAccountMerger::RemoveChildrenAt(
    const bookmarks::BookmarkNode* parent,
    const base::flat_set<size_t>& indices_to_remove,
    const base::Location& location) {
  CHECK(parent);

  if (indices_to_remove.empty()) {
    // Nothing to do.
    return;
  }

  const size_t num_children = parent->children().size();
  CHECK_LT(*indices_to_remove.rbegin(), num_children);

  // The single-removal case is trivial and needs no sophisticated optimization.
  if (indices_to_remove.size() == 1u) {
    const size_t index = *indices_to_remove.begin();
    const bookmarks::BookmarkNode* child = parent->children().at(index).get();
    model_->Remove(child, kEditSourceForMetrics, location);
    return;
  }

  // Removal of children one by one, even in reverse order, can theoretically
  // lead to quadratic behavior. However, A/B experiments via variations led to
  // the conclusion that this simple implementation isn't slower than more
  // sophisticated variants, even at high percentiles.
  for (size_t index : base::Reversed(indices_to_remove)) {
    const bookmarks::BookmarkNode* child = parent->children().at(index).get();
    model_->Remove(child, kEditSourceForMetrics, location);
  }
}

void LocalBookmarkToAccountMerger::MoveOrMergeDescendants(
    const bookmarks::BookmarkNode* local_subtree_root,
    const bookmarks::BookmarkNode* account_subtree_root,
    std::optional<std::set<int64_t>> local_child_ids_to_merge) {
  CHECK(local_subtree_root);
  CHECK(account_subtree_root);
  CHECK_EQ(account_subtree_root->is_folder(), local_subtree_root->is_folder());
  CHECK_EQ(account_subtree_root->is_permanent_node(),
           local_subtree_root->is_permanent_node());

  // Build a lookup table containing account nodes that might be matched by
  // semantics.
  std::unordered_map<SiblingSemanticMatchKey,
                     std::list<const bookmarks::BookmarkNode*>,
                     SiblingSemanticMatchKeyHash, SiblingSemanticMatchKeyEquals>
      account_node_candidates_for_semantic_match;
  for (const auto& account_child_ptr : account_subtree_root->children()) {
    const bookmarks::BookmarkNode* const account_child =
        account_child_ptr.get();

    // If a UUID match exists, it takes precedence over semantic matching.
    if (FindMatchingLocalNodeByUuid(account_child)) {
      continue;
    }

    // Permanent nodes must have matched by UUID.
    CHECK(!account_child->is_permanent_node());

    // Register the candidate while maintaining the original order.
    account_node_candidates_for_semantic_match
        [GetSiblingSemanticMatchKeyForNode(account_child)]
            .push_back(account_child);
  }

  // A list of indices of local children that need to be deleted. These are
  // populated in the loop below, and are correct for local_subtree_root
  // *after* the `Move()` operations from that loop, that is.
  std::vector<size_t> local_indices_to_remove;
  local_indices_to_remove.reserve(local_subtree_root->children().size());

  // If there are local child nodes, try to match them with account nodes.
  size_t i = 0;
  while (i < local_subtree_root->children().size()) {
    const bookmarks::BookmarkNode* const local_child =
        local_subtree_root->children()[i].get();
    CHECK(!local_child->is_permanent_node());

    if (local_child_ids_to_merge.has_value() &&
        !local_child_ids_to_merge->contains(local_child->id())) {
      // Skip this child if it's not in the list of IDs to merge.
      //
      // It's guaranteed that no ancestors of this node were selected as part of
      // the move operation (if they were then the recursive call into this
      // method would have `local_child_ids_to_merge` set to `nullopt`).
      ++i;
      continue;
    }

    // Try to match by UUID first.
    const bookmarks::BookmarkNode* matching_account_node =
        FindMatchingAccountNodeByUuid(local_child);

    if (!matching_account_node) {
      auto it = account_node_candidates_for_semantic_match.find(
          GetSiblingSemanticMatchKeyForNode(local_child));
      if (it != account_node_candidates_for_semantic_match.end() &&
          !it->second.empty()) {
        // Semantic match found.
        matching_account_node = it->second.front();
        // Avoid that the same candidate would match again.
        it->second.pop_front();
      }
    }

    if (matching_account_node) {
      // If a match was found, update the title and possible other fields.
      CHECK(!matching_account_node->is_permanent_node());
      UpdateAccountNodeFromMatchingLocalNode(local_child,
                                             matching_account_node);

      // Since nodes are matching, their full subtrees should be merged as well.
      //
      // Select all descendants for merging, as we don't have any use cases for
      // selecting the parent but only a subset of its descendants.
      MoveOrMergeDescendants(local_child, matching_account_node,
                             /*local_child_ids_to_merge=*/std::nullopt);

      // The node has been merged to account storage, so remove it from the
      // local parent.
      //
      // As a performance optimization, flag the node for removal at the end of
      // the loop, so that the removal can be done in bulk.
      local_indices_to_remove.push_back(i++);
    } else {
      // Before the entire local subtree is moved to account storage, iterate
      // descendants to find UUID matches. This is necessary because UUID-based
      // matches takes precedence over any ancestor having matched (by UUID or
      // otherwise).
      //
      // Similarly to the recursive call to `MoveOrMergeDescendants` above,
      // select all descendents for UUID merging/deletion, as this API doesn't
      // support selecting a parent and just a subset of its descendants.
      MergeAndDeleteDescendantsThatMatchByUuid(local_child);

      // Move the local node to account storage, along with all remaining
      // descendants that didn't match by UUID. Note that this can theoretically
      // lead to quadratic runtime complexity, but measurements with a large
      // number of bookmarks suggest the issue is not severe in practice and the
      // complexity of the improved algorithm is not worth the maintenance cost.
      model_->Move(local_child, account_subtree_root,
                   account_subtree_root->children().size());
    }
  }

  // Remove the local nodes that were flagged for removal above.
  RemoveChildrenAt(
      local_subtree_root,
      base::flat_set<size_t>(base::sorted_unique, local_indices_to_remove),
      FROM_HERE);
}

void LocalBookmarkToAccountMerger::MergeAndDeleteDescendantsThatMatchByUuid(
    const bookmarks::BookmarkNode* local_subtree_root) {
  CHECK(local_subtree_root);

  std::vector<size_t> indices_to_remove;

  for (size_t i = 0; i < local_subtree_root->children().size(); ++i) {
    const bookmarks::BookmarkNode* const local_child =
        local_subtree_root->children()[i].get();
    CHECK(!local_child->is_permanent_node());

    if (const bookmarks::BookmarkNode* matching_account_node =
            FindMatchingAccountNodeByUuid(local_child)) {
      CHECK(!matching_account_node->is_permanent_node());
      UpdateAccountNodeFromMatchingLocalNode(local_child,
                                             matching_account_node);
      indices_to_remove.push_back(i);

      // Since nodes are matching, their subtrees should be merged as well. In
      // this case the matching isn't restricted to UUID-based matching.
      MoveOrMergeDescendants(local_child, matching_account_node,
                             /*local_child_ids_to_merge=*/std::nullopt);
    } else {
      // Continue recursively to look for UUID-based matches.
      MergeAndDeleteDescendantsThatMatchByUuid(local_child);
    }
  }

  RemoveChildrenAt(
      local_subtree_root,
      base::flat_set<size_t>(base::sorted_unique, indices_to_remove),
      FROM_HERE);
}

void LocalBookmarkToAccountMerger::UpdateAccountNodeFromMatchingLocalNode(
    const bookmarks::BookmarkNode* local_node,
    const bookmarks::BookmarkNode* account_node) {
  CHECK(local_node);
  CHECK(account_node);
  CHECK(!local_node->is_permanent_node());
  CHECK(!account_node->is_permanent_node());

  // Update all fields, where no-op changes are handled well.
  // The meta-info map is intentionally excluded, since the desired behavior is
  // unclear.
  if (local_node->date_last_used() > account_node->date_last_used()) {
    model_->UpdateLastUsedTime(account_node, local_node->date_last_used(),
                               /*just_opened=*/false);
  }

  // For the title, use the local one.
  model_->SetTitle(account_node, local_node->GetTitle(), kEditSourceForMetrics);
}

const bookmarks::BookmarkNode*
LocalBookmarkToAccountMerger::FindMatchingLocalNodeByUuid(
    const bookmarks::BookmarkNode* account_node) const {
  CHECK(account_node);

  const bookmarks::BookmarkNode* const local_node = model_->GetNodeByUuid(
      account_node->uuid(),
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  if (local_node && NodesCompatibleForMatchByUuid(local_node, account_node) &&
      !model_->client()->IsNodeManaged(local_node)) {
    return local_node;
  }
  return nullptr;
}

const bookmarks::BookmarkNode*
LocalBookmarkToAccountMerger::FindMatchingAccountNodeByUuid(
    const bookmarks::BookmarkNode* local_node) const {
  CHECK(local_node);

  const bookmarks::BookmarkNode* const account_node = model_->GetNodeByUuid(
      local_node->uuid(),
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
  if (account_node && NodesCompatibleForMatchByUuid(local_node, account_node)) {
    return account_node;
  }
  return nullptr;
}

}  // namespace sync_bookmarks
