// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/test/test_bookmark_client.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_load_details.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/gfx/image/image.h"

namespace bookmarks {

TestBookmarkClient::TestBookmarkClient() = default;

TestBookmarkClient::~TestBookmarkClient() = default;

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
  details->index()->AddPath(details->other_folder_node());
  details->CreateUrlIndex();
  bookmark_model->DoneLoading(std::move(details));
  return bookmark_model;
}

BookmarkPermanentNode* TestBookmarkClient::EnableManagedNode() {
  managed_node_ = BookmarkPermanentNode::CreateManagedBookmarks(/*id=*/100);
  // Keep a copy of the node in |unowned_managed_node_| for the accessor
  // functions.
  unowned_managed_node_ = managed_node_.get();
  return unowned_managed_node_;
}

bool TestBookmarkClient::IsManagedNodeRoot(const BookmarkNode* node) {
  return unowned_managed_node_ == node;
}

bool TestBookmarkClient::IsAManagedNode(const BookmarkNode* node) {
  return node && node->HasAncestor(unowned_managed_node_.get());
}

bool TestBookmarkClient::SimulateFaviconLoaded(const GURL& page_url,
                                               const GURL& icon_url,
                                               const gfx::Image& image) {
  if (requests_per_page_url_[page_url].empty()) {
    return false;
  }

  favicon_base::FaviconImageCallback callback =
      std::move(requests_per_page_url_[page_url].front());
  requests_per_page_url_[page_url].pop_front();

  favicon_base::FaviconImageResult result;
  result.image = image;
  result.icon_url = icon_url;
  std::move(callback).Run(result);
  return true;
}

bool TestBookmarkClient::SimulateEmptyFaviconLoaded(const GURL& page_url) {
  if (requests_per_page_url_[page_url].empty()) {
    return false;
  }

  favicon_base::FaviconImageCallback callback =
      std::move(requests_per_page_url_[page_url].front());
  requests_per_page_url_[page_url].pop_front();

  std::move(callback).Run(favicon_base::FaviconImageResult());
  return true;
}

bool TestBookmarkClient::HasFaviconLoadTasks() const {
  return !requests_per_page_url_.empty();
}

bool TestBookmarkClient::IsPermanentNodeVisibleWhenEmpty(
    BookmarkNode::Type type) {
  switch (type) {
    case bookmarks::BookmarkNode::URL:
      NOTREACHED();
      return false;
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
    case bookmarks::BookmarkNode::OTHER_NODE:
      return true;
    case bookmarks::BookmarkNode::FOLDER:
    case bookmarks::BookmarkNode::MOBILE:
      return false;
  }

  NOTREACHED();
  return false;
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

base::CancelableTaskTracker::TaskId
TestBookmarkClient::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback,
    base::CancelableTaskTracker* tracker) {
  requests_per_page_url_[page_url].push_back(std::move(callback));
  return next_task_id_++;
}

// static
std::unique_ptr<BookmarkPermanentNode> TestBookmarkClient::LoadManagedNode(
    std::unique_ptr<BookmarkPermanentNode> managed_node,
    int64_t* next_id) {
  return managed_node;
}

}  // namespace bookmarks
