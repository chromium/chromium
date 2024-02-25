// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_load_details.h"

#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/url_index.h"

namespace bookmarks {

namespace {

// Number of top-level permanent folders excluding the managed node and account
// bookmarks.
constexpr size_t kNumDefaultTopLevelPermanentFolders = 3u;

}  // namespace

BookmarkLoadDetails::BookmarkLoadDetails()
    : titled_url_index_(std::make_unique<TitledUrlIndex>()),
      load_start_(base::TimeTicks::Now()) {
  // WARNING: do NOT add |client| as a member. Much of this code runs on another
  // thread, and |client_| is not thread safe, and/or may be destroyed before
  // this.
  root_node_ = std::make_unique<BookmarkNode>(
      /*id=*/0, base::Uuid::ParseLowercase(kRootNodeUuid), GURL());
  // WARNING: order is important here, various places assume the order is
  // constant (but can vary between embedders with the initial visibility
  // of permanent nodes).
  //
  // Zero is used as temporary ID for permanent nodes, until an actual value is
  // loaded from disk or new/default values are allocated in
  // `PopulateNodeIdsForLocalOrSyncablePermanentNodes()`.
  bb_node_ = static_cast<BookmarkPermanentNode*>(
      root_node_->Add(BookmarkPermanentNode::CreateBookmarkBar(/*id=*/0)));
  other_folder_node_ = static_cast<BookmarkPermanentNode*>(
      root_node_->Add(BookmarkPermanentNode::CreateOtherBookmarks(/*id=*/0)));
  mobile_folder_node_ = static_cast<BookmarkPermanentNode*>(
      root_node_->Add(BookmarkPermanentNode::CreateMobileBookmarks(/*id=*/0)));

  CHECK_EQ(kNumDefaultTopLevelPermanentFolders, root_node_->children().size());
}

BookmarkLoadDetails::~BookmarkLoadDetails() = default;

void BookmarkLoadDetails::AddAccountPermanentNodes(
    std::unique_ptr<BookmarkPermanentNode> account_bb_node,
    std::unique_ptr<BookmarkPermanentNode> account_other_folder_node,
    std::unique_ptr<BookmarkPermanentNode> account_mobile_folder_node) {
  CHECK(account_bb_node);
  CHECK(account_other_folder_node);
  CHECK(account_mobile_folder_node);
  CHECK(!account_bb_node_);
  CHECK(!account_other_folder_node_);
  CHECK(!account_mobile_folder_node_);

  // The order here is consistent with the one used for non-account permanent
  // folders, created in the constructor.
  account_bb_node_ = static_cast<BookmarkPermanentNode*>(
      root_node_->Add(std::move(account_bb_node)));
  account_other_folder_node_ = static_cast<BookmarkPermanentNode*>(
      root_node_->Add(std::move(account_other_folder_node)));
  account_mobile_folder_node_ = static_cast<BookmarkPermanentNode*>(
      root_node_->Add(std::move(account_mobile_folder_node)));
}

void BookmarkLoadDetails::PopulateNodeIdsForLocalOrSyncablePermanentNodes() {
  CHECK(bb_node_);
  CHECK(other_folder_node_);
  CHECK(mobile_folder_node_);

  // AddManagedNode() may only be called after this function.
  CHECK(!has_managed_node_);

  if (bb_node_->id() == 0) {
    bb_node_->set_id(max_id_++);
  }

  if (other_folder_node_->id() == 0) {
    other_folder_node_->set_id(max_id_++);
  }

  if (mobile_folder_node_->id() == 0) {
    mobile_folder_node_->set_id(max_id_++);
  }
}

void BookmarkLoadDetails::AddManagedNode(
    std::unique_ptr<BookmarkPermanentNode> managed_node) {
  CHECK(managed_node);
  CHECK(!has_managed_node_);

  // Ensure that `PopulateNodeIdsForLocalOrSyncablePermanentNodes` was invoked
  // before this function.
  CHECK_NE(bb_node_->id(), 0);
  CHECK_NE(other_folder_node_->id(), 0);
  CHECK_NE(mobile_folder_node_->id(), 0);

  has_managed_node_ = true;
  root_node_->Add(std::move(managed_node));
}

void BookmarkLoadDetails::CreateIndices() {
  local_or_syncable_uuid_index_.insert(root_node_.get());
  static_assert(kNumDefaultTopLevelPermanentFolders == 3u,
                "The code below assumes three permanent nodes");
  for (const auto& child : root_node_->children()) {
    if (child.get() == account_bb_node_ ||
        child.get() == account_other_folder_node_ ||
        child.get() == account_mobile_folder_node_) {
      // Use a dedicated index for account folders and desdendants.
      AddNodeToIndexRecursive(child.get(), account_uuid_index_);
    } else {
      AddNodeToIndexRecursive(child.get(), local_or_syncable_uuid_index_);
    }
  }

  url_index_ = base::MakeRefCounted<UrlIndex>(std::move(root_node_));
}

void BookmarkLoadDetails::ResetPermanentNodePointers() {
  bb_node_ = nullptr;
  other_folder_node_ = nullptr;
  mobile_folder_node_ = nullptr;
  account_bb_node_ = nullptr;
  account_other_folder_node_ = nullptr;
  account_mobile_folder_node_ = nullptr;
}

const BookmarkNode* BookmarkLoadDetails::RootNodeForTest() const {
  return url_index_ ? url_index_->root() : root_node_.get();
}

void BookmarkLoadDetails::AddNodeToIndexRecursive(BookmarkNode* node,
                                                  UuidIndex& uuid_index) {
  uuid_index.insert(node);
  if (node->is_url()) {
    if (node->url().is_valid()) {
      titled_url_index_->Add(node);
    }
  } else {
    titled_url_index_->AddPath(node);
    for (const auto& child : node->children()) {
      AddNodeToIndexRecursive(child.get(), uuid_index);
    }
  }
}

}  // namespace bookmarks
