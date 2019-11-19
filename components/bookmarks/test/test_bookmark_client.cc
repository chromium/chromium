// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/test/test_bookmark_client.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"

namespace bookmarks {

TestBookmarkClient::TestBookmarkClient() {}

TestBookmarkClient::~TestBookmarkClient() {}

// static
std::unique_ptr<BookmarkModel> TestBookmarkClient::CreateModel() {
  return CreateModelWithClient(std::make_unique<TestBookmarkClient>());
}

// static
std::unique_ptr<BookmarkModel> TestBookmarkClient::CreateModelWithClient(
    std::unique_ptr<BookmarkClient> client) {
  BookmarkClient* client_ptr = client.get();
  std::unique_ptr<BookmarkModel> bookmark_model(
      new BookmarkModel(std::move(client)));
  std::unique_ptr<BookmarkLoadDetails> details =
      std::make_unique<BookmarkLoadDetails>(client_ptr);
  details->LoadManagedNode();
  details->CreateUrlIndex();
  bookmark_model->DoneLoading(std::move(details));
  return bookmark_model;
}

void TestBookmarkClient::SetManagedNodeToLoad(
    std::unique_ptr<BookmarkPermanentNode> managed_node) {
  managed_node_ = std::move(managed_node);
  // Keep a copy of the node in |unowned_managed_node_| for the accessor
  // functions.
  unowned_managed_node_ = managed_node_.get();
}

bool TestBookmarkClient::IsManagedNodeRoot(const BookmarkNode* node) {
  return unowned_managed_node_ == node;
}

bool TestBookmarkClient::IsAManagedNode(const BookmarkNode* node) {
  return node && node->HasAncestor(unowned_managed_node_);
}

bool TestBookmarkClient::IsPermanentNodeVisible(
    const BookmarkPermanentNode* node) {
  DCHECK(node->type() == BookmarkNode::BOOKMARK_BAR ||
         node->type() == BookmarkNode::OTHER_NODE ||
         node->type() == BookmarkNode::MOBILE || IsManagedNodeRoot(node));
  return node->type() != BookmarkNode::MOBILE && !IsManagedNodeRoot(node);
}

void TestBookmarkClient::RecordAction(const base::UserMetricsAction& action) {
}

LoadManagedNodeCallback TestBookmarkClient::GetLoadManagedNodeCallback() {
  return base::BindOnce(&TestBookmarkClient::LoadManagedNode,
                        std::move(managed_node_));
}

bool TestBookmarkClient::CanSetPermanentNodeTitle(
    const BookmarkNode* permanent_node) {
  return IsManagedNodeRoot(permanent_node);
}

bool TestBookmarkClient::CanSyncNode(const BookmarkNode* node) {
  return !IsAManagedNode(node);
}

bool TestBookmarkClient::CanBeEditedByUser(const BookmarkNode* node) {
  return !IsAManagedNode(node);
}

std::string TestBookmarkClient::EncodeBookmarkSyncMetadata() {
  return std::string();
}

void TestBookmarkClient::DecodeBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure) {}

// static
std::unique_ptr<BookmarkPermanentNode> TestBookmarkClient::LoadManagedNode(
    std::unique_ptr<BookmarkPermanentNode> managed_node,
    int64_t* next_id) {
  return managed_node;
}

}  // namespace bookmarks
