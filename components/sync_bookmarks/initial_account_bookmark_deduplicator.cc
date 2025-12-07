// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/initial_account_bookmark_deduplicator.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/location.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"

namespace sync_bookmarks {

namespace {

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

InitialAccountBookmarkDeduplicator::InitialAccountBookmarkDeduplicator(
    bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model) {
  CHECK(bookmark_model_);
}

InitialAccountBookmarkDeduplicator::~InitialAccountBookmarkDeduplicator() =
    default;

void InitialAccountBookmarkDeduplicator::Deduplicate() {
  for (const auto& [local_permanent_node, account_permanent_node] :
       GetLocalAndAccountPermanentNodePairs(bookmark_model_)) {
    std::vector<size_t> indices_to_remove;
    for (size_t i = 0; i < local_permanent_node->children().size(); ++i) {
      const bookmarks::BookmarkNode* const local_child =
          local_permanent_node->children()[i].get();
      const bookmarks::BookmarkNode* account_child =
          bookmark_model_->GetNodeByUuid(
              local_child->uuid(),
              bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
      if (account_child && account_child->parent() == account_permanent_node &&
          DoesAccountSubgraphContainLocalSubgraph(local_child, account_child)) {
        indices_to_remove.push_back(i);
      }
    }
    RemoveChildrenAt(
        local_permanent_node,
        base::flat_set<size_t>(base::sorted_unique, indices_to_remove));
  }
}

bool InitialAccountBookmarkDeduplicator::
    DoesAccountSubgraphContainLocalSubgraphForTest(
        const bookmarks::BookmarkNode* local_subtree_root,
        const bookmarks::BookmarkNode* account_subtree_root) const {
  return DoesAccountSubgraphContainLocalSubgraph(local_subtree_root,
                                                 account_subtree_root);
}

bool InitialAccountBookmarkDeduplicator::
    DoesAccountSubgraphContainLocalSubgraph(
        const bookmarks::BookmarkNode* local_subtree_root,
        const bookmarks::BookmarkNode* account_subtree_root) const {
  if (local_subtree_root->type() != account_subtree_root->type()) {
    return false;
  }

  if (local_subtree_root->is_url()) {
    return local_subtree_root->url() == account_subtree_root->url();
  }

  if (local_subtree_root->GetTitle() != account_subtree_root->GetTitle()) {
    return false;
  }

  for (const auto& local_child : local_subtree_root->children()) {
    const bookmarks::BookmarkNode* account_child =
        bookmark_model_->GetNodeByUuid(
            local_child->uuid(),
            bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
    if (!account_child || account_child->parent() != account_subtree_root ||
        !DoesAccountSubgraphContainLocalSubgraph(local_child.get(),
                                                 account_child)) {
      return false;
    }
  }
  return true;
}

void InitialAccountBookmarkDeduplicator::RemoveChildrenAt(
    const bookmarks::BookmarkNode* parent,
    const base::flat_set<size_t>& indices_to_remove) {
  CHECK(parent);

  const size_t num_children = parent->children().size();
  CHECK(indices_to_remove.empty() ||
        *indices_to_remove.rbegin() < num_children);

  // Removal of children one by one, even in reverse order, can theoretically
  // lead to quadratic behavior. However, A/B experiments via variations led to
  // the conclusion that this simple implementation isn't slower than more
  // sophisticated variants, even at high percentiles.
  for (size_t index : base::Reversed(indices_to_remove)) {
    const bookmarks::BookmarkNode* child = parent->children().at(index).get();
    bookmark_model_->Remove(
        child, bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  }
}

}  // namespace sync_bookmarks
