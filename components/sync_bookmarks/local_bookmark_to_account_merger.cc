// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/local_bookmark_to_account_merger.h"

#include <list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
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
    : model_(model), uuid_to_match_map_(FindGuidMatches(model)) {
  CHECK(model_);
  CHECK(model_->loaded());
  for (const auto& [local_permanent_node, account_permanent_node] :
       GetLocalAndAccountPermanentNodePairs(model)) {
    CHECK(local_permanent_node);
    CHECK(account_permanent_node);
  }
}

LocalBookmarkToAccountMerger::~LocalBookmarkToAccountMerger() = default;

void LocalBookmarkToAccountMerger::MoveAndMerge() {
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
  // * If no matching node is found, create a new bookmark node by appending it
  //   last.
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
    CopyOrMergeDescendants(
        /*local_subtree_root=*/local_permanent_node,
        /*account_subtree_root=*/account_permanent_node);
  }

  // Clear the UUID match map to avoid dangling pointers.
  uuid_to_match_map_.clear();

  // All local nodes have been copied to account storage and can be safely
  // removed.
  for (const auto& [local_permanent_node, unused] :
       GetLocalAndAccountPermanentNodePairs(model_)) {
    while (!local_permanent_node->children().empty()) {
      model_->RemoveLastChild(local_permanent_node, kEditSourceForMetrics,
                              FROM_HERE);
    }
  }

  model_->EndExtensiveChanges();
}

// static
std::unordered_map<base::Uuid,
                   LocalBookmarkToAccountMerger::GuidMatch,
                   base::UuidHash>
LocalBookmarkToAccountMerger::FindGuidMatches(
    const bookmarks::BookmarkModel* model) {
  CHECK(model);
  CHECK(model->loaded());

  std::unordered_map<base::Uuid, LocalBookmarkToAccountMerger::GuidMatch,
                     base::UuidHash>
      uuid_to_match_map;

  // Iterate through all local bookmarks to find matches by UUID.
  for (const auto& [local_permanent_node, unused] :
       GetLocalAndAccountPermanentNodePairs(model)) {
    CHECK(local_permanent_node);
    ui::TreeNodeIterator<const bookmarks::BookmarkNode> local_iterator(
        local_permanent_node);
    while (local_iterator.has_next()) {
      const bookmarks::BookmarkNode* const local_node = local_iterator.Next();
      CHECK(local_node->uuid().is_valid());

      const bookmarks::BookmarkNode* const account_node = model->GetNodeByUuid(
          local_node->uuid(),
          bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
      if (!account_node) {
        // No match found by UUID.
        continue;
      }

      if (NodesCompatibleForMatchByUuid(account_node, local_node)) {
        const bool success = uuid_to_match_map
                                 .emplace(account_node->uuid(),
                                          GuidMatch{local_node, account_node})
                                 .second;
        CHECK(success);
      }
    }
  }

  return uuid_to_match_map;
}

void LocalBookmarkToAccountMerger::CopyOrMergeDescendants(
    const bookmarks::BookmarkNode* local_subtree_root,
    const bookmarks::BookmarkNode* account_subtree_root) {
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

  // If there are local child nodes, try to match them with account nodes.
  for (const auto& local_child_ptr : local_subtree_root->children()) {
    const bookmarks::BookmarkNode* const local_child = local_child_ptr.get();
    CHECK(!local_child->is_permanent_node());

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
    } else {
      // If no match found, create a corresponding account node, which gets
      // appended last.
      matching_account_node =
          CopyLocalNodeToAccountModel(local_child, account_subtree_root);
      CHECK(matching_account_node);
    }

    // Since nodes are matching, their subtrees should be merged as well.
    CopyOrMergeDescendants(local_child, matching_account_node);
  }
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
LocalBookmarkToAccountMerger::CopyLocalNodeToAccountModel(
    const bookmarks::BookmarkNode* local_node,
    const bookmarks::BookmarkNode* account_parent) {
  CHECK(local_node);
  CHECK(!local_node->is_permanent_node());
  CHECK(account_parent);

  const size_t account_index = account_parent->children().size();

  // See if the same UUID can be carried over or a random one generated.
  const base::Uuid new_node_uuid =
      (model_->GetNodeByUuid(
           local_node->uuid(),
           bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes) !=
       nullptr)
          ? base::Uuid::GenerateRandomV4()
          : local_node->uuid();

  // Note that this function is not expected to copy children recursively. The
  // caller is responsible for dealing with children.
  return local_node->is_folder()
             ? model_->AddFolder(account_parent, account_index,
                                 local_node->GetTitle(),
                                 local_node->GetMetaInfoMap(),
                                 local_node->date_added(), new_node_uuid)
             : model_->AddURL(account_parent, account_index,
                              local_node->GetTitle(), local_node->url(),
                              local_node->GetMetaInfoMap(),
                              local_node->date_added(), new_node_uuid);
}

const bookmarks::BookmarkNode*
LocalBookmarkToAccountMerger::FindMatchingLocalNodeByUuid(
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
LocalBookmarkToAccountMerger::FindMatchingAccountNodeByUuid(
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
