// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_merged_surface_view.h"

#include "base/check.h"
#include "base/uuid.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "url/gurl.h"

BookmarkMergedSurfaceView::BookmarkMergedSurfaceView(
    BookmarkMergedSurfaceService* service)
    : service_(service),
      synthetic_root_node_(std::make_unique<bookmarks::BookmarkNode>(
          /*id=*/0,
          base::Uuid::GenerateRandomV4(),
          GURL())) {
  CHECK(service_);
}

BookmarkMergedSurfaceView::~BookmarkMergedSurfaceView() = default;

void BookmarkMergedSurfaceView::AddObserver(
    bookmarks::BookmarkModelObserver* observer) {
  service_->bookmark_model()->AddObserver(observer);
}

void BookmarkMergedSurfaceView::RemoveObserver(
    bookmarks::BookmarkModelObserver* observer) {
  service_->bookmark_model()->RemoveObserver(observer);
}

bool BookmarkMergedSurfaceView::IsDoingExtensiveChanges() const {
  return service_->bookmark_model()->IsDoingExtensiveChanges();
}

const bookmarks::BookmarkNode* BookmarkMergedSurfaceView::GetRootNode() const {
  return synthetic_root_node_.get();
}

std::vector<const bookmarks::BookmarkNode*>
BookmarkMergedSurfaceView::GetChildren(
    const bookmarks::BookmarkNode* parent) const {
  CHECK(parent != nullptr);
  CHECK(parent->is_folder());

  if (parent == synthetic_root_node_.get()) {
    std::vector<const bookmarks::BookmarkNode*> children;
    for (const auto& folder : {BookmarkParentFolder::BookmarkBarFolder(),
                               BookmarkParentFolder::OtherFolder(),
                               BookmarkParentFolder::MobileFolder()}) {
      const bookmarks::BookmarkNode* node =
          service_->GetDefaultParentForNewNodes(folder);
      if (node) {
        children.push_back(node);
      }
    }
    if (service_->managed_bookmark_service() &&
        service_->managed_bookmark_service()->managed_node()) {
      children.push_back(service_->managed_bookmark_service()->managed_node());
    }
    return children;
  }

  std::vector<const bookmarks::BookmarkNode*> children;
  BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(parent);
  BookmarkParentFolderChildren merged_children = service_->GetChildren(folder);
  children.reserve(merged_children.size());
  for (const auto* child : merged_children) {
    children.push_back(child);
  }
  return children;
}

std::optional<const bookmarks::BookmarkNode*>
BookmarkMergedSurfaceView::FindNodeByUuid(const base::Uuid& uuid) const {
  if (uuid == synthetic_root_node_->uuid()) {
    return synthetic_root_node_.get();
  }
  const bookmarks::BookmarkNode* node =
      service_->bookmark_model()->GetNodeByUuid(
          uuid, bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
  if (!node) {
    node = service_->bookmark_model()->GetNodeByUuid(
        uuid,
        bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  }
  if (!node) {
    return std::nullopt;
  }
  return node;
}

bool BookmarkMergedSurfaceView::IsPermanentNode(
    const bookmarks::BookmarkNode* node) const {
  if (node == synthetic_root_node_.get()) {
    return true;
  }
  return service_->bookmark_model()->is_permanent_node(node);
}

bookmarks_api::mojom::PermanentFolderType
BookmarkMergedSurfaceView::GetPermanentFolderType(
    const bookmarks::BookmarkNode* node) const {
  if (node == synthetic_root_node_.get()) {
    return bookmarks_api::mojom::PermanentFolderType::kUnknown;
  }
  if (node->type() == bookmarks::BookmarkNode::Type::BOOKMARK_BAR) {
    return bookmarks_api::mojom::PermanentFolderType::kBookmarkBar;
  }
  if (node->type() == bookmarks::BookmarkNode::Type::OTHER_NODE) {
    return bookmarks_api::mojom::PermanentFolderType::kOther;
  }
  if (node->type() == bookmarks::BookmarkNode::Type::MOBILE) {
    return bookmarks_api::mojom::PermanentFolderType::kMobile;
  }
  if (service_->managed_bookmark_service() &&
      node == service_->managed_bookmark_service()->managed_node()) {
    return bookmarks_api::mojom::PermanentFolderType::kManaged;
  }
  return bookmarks_api::mojom::PermanentFolderType::kUnknown;
}

bool BookmarkMergedSurfaceView::IsSynced(
    const bookmarks::BookmarkNode* node) const {
  if (node == synthetic_root_node_.get()) {
    return false;
  }
  return !service_->bookmark_model()->IsLocalOnlyNode(*node);
}

const bookmarks::BookmarkNode* BookmarkMergedSurfaceView::AddURL(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url) {
  CHECK(parent != nullptr);
  CHECK(parent->is_folder());

  BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(parent);
  const bookmarks::BookmarkNode* target_parent =
      folder.as_permanent_folder()
          ? service_->GetDefaultParentForNewNodes(folder)
          : parent;
  size_t storage_index = 0;
  size_t count = 0;
  for (const auto* child : GetChildren(parent)) {
    if (count == index) {
      break;
    }
    if (child->parent() == target_parent) {
      ++storage_index;
    }
    ++count;
  }
  return service_->bookmark_model()->AddNewURL(target_parent, storage_index,
                                               title, url);
}

const bookmarks::BookmarkNode* BookmarkMergedSurfaceView::AddFolder(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title) {
  CHECK(parent != nullptr);
  CHECK(parent->is_folder());

  BookmarkParentFolder folder = BookmarkParentFolder::FromFolderNode(parent);
  const bookmarks::BookmarkNode* target_parent =
      folder.as_permanent_folder()
          ? service_->GetDefaultParentForNewNodes(folder)
          : parent;
  size_t storage_index = 0;
  size_t count = 0;
  for (const auto* child : GetChildren(parent)) {
    if (count == index) {
      break;
    }
    if (child->parent() == target_parent) {
      ++storage_index;
    }
    ++count;
  }
  return service_->bookmark_model()->AddFolder(target_parent, storage_index,
                                               title);
}

void BookmarkMergedSurfaceView::Move(const bookmarks::BookmarkNode* node,
                                     const bookmarks::BookmarkNode* new_parent,
                                     size_t index) {
  BookmarkParentFolder folder =
      BookmarkParentFolder::FromFolderNode(new_parent);
  service_->Move(node, folder, index, /*browser=*/nullptr);
}

void BookmarkMergedSurfaceView::SetTitle(
    const bookmarks::BookmarkNode* node,
    const std::u16string& title,
    bookmarks::metrics::BookmarkEditSource source) {
  service_->bookmark_model()->SetTitle(node, title, source);
}

void BookmarkMergedSurfaceView::SetURL(
    const bookmarks::BookmarkNode* node,
    const GURL& url,
    bookmarks::metrics::BookmarkEditSource source) {
  service_->bookmark_model()->SetURL(node, url, source);
}

void BookmarkMergedSurfaceView::Remove(
    const bookmarks::BookmarkNode* node,
    bookmarks::metrics::BookmarkEditSource source,
    const base::Location& location) {
  service_->bookmark_model()->Remove(node, source, location);
}

void BookmarkMergedSurfaceView::RemoveNodes(
    const std::vector<const bookmarks::BookmarkNode*>& nodes,
    bookmarks::metrics::BookmarkEditSource source,
    const base::Location& location) {
  bookmarks::ScopedGroupBookmarkActions group_deletes(
      service_->bookmark_model());
  for (const auto* node : nodes) {
    service_->bookmark_model()->Remove(node, source, location);
  }
}
