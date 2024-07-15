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
    std::unique_ptr<TestBookmarkClient> client) {
  auto bookmark_model = std::make_unique<BookmarkModel>(std::move(client));
  bookmark_model->LoadEmptyForTest();
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

void TestBookmarkClient::SetIsSyncFeatureEnabledIncludingBookmarks(bool value) {
  is_sync_feature_enabled_including_bookmarks_for_uma = value;
}

void TestBookmarkClient::SetAccountBookmarkSyncMetadataAndScheduleWrite(
    const std::string& account_bookmark_sync_metadata) {
  account_bookmark_sync_metadata_ = account_bookmark_sync_metadata;
  account_bookmark_sync_metadata_save_closure_.Run();
}

LoadManagedNodeCallback TestBookmarkClient::GetLoadManagedNodeCallback() {
  return base::BindOnce(&TestBookmarkClient::LoadManagedNode,
                        std::move(managed_node_));
}

bool TestBookmarkClient::IsSyncFeatureEnabledIncludingBookmarks() {
  return is_sync_feature_enabled_including_bookmarks_for_uma;
}

bool TestBookmarkClient::CanSetPermanentNodeTitle(
    const BookmarkNode* permanent_node) {
  return IsManagedNodeRoot(permanent_node);
}

bool TestBookmarkClient::IsNodeManaged(const BookmarkNode* node) {
  return node && node->HasAncestor(unowned_managed_node_.get());
}

std::string TestBookmarkClient::EncodeLocalOrSyncableBookmarkSyncMetadata() {
  return std::string();
}

std::string TestBookmarkClient::EncodeAccountBookmarkSyncMetadata() {
  return account_bookmark_sync_metadata_;
}

void TestBookmarkClient::DecodeLocalOrSyncableBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure) {}

void TestBookmarkClient::DecodeAccountBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure) {
  account_bookmark_sync_metadata_ = metadata_str;
  account_bookmark_sync_metadata_save_closure_ = schedule_save_closure;
}

base::CancelableTaskTracker::TaskId
TestBookmarkClient::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback,
    base::CancelableTaskTracker* tracker) {
  requests_per_page_url_[page_url].push_back(std::move(callback));
  return next_task_id_++;
}

void TestBookmarkClient::OnBookmarkNodeRemovedUndoable(
    const BookmarkNode* parent,
    size_t index,
    std::unique_ptr<BookmarkNode> node) {}

// static
std::unique_ptr<BookmarkPermanentNode> TestBookmarkClient::LoadManagedNode(
    std::unique_ptr<BookmarkPermanentNode> managed_node,
    int64_t* next_id) {
  return managed_node;
}

}  // namespace bookmarks
