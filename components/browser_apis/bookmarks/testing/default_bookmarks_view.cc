// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/testing/default_bookmarks_view.h"

#include "base/check.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"

namespace bookmarks_api {

DefaultBookmarksView::DefaultBookmarksView(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed_service)
    : model_(model), managed_service_(managed_service) {
  CHECK(model_);
}

DefaultBookmarksView::~DefaultBookmarksView() = default;

void DefaultBookmarksView::AddObserver(
    bookmarks::BookmarkModelObserver* observer) {
  model_->AddObserver(observer);
}

void DefaultBookmarksView::RemoveObserver(
    bookmarks::BookmarkModelObserver* observer) {
  model_->RemoveObserver(observer);
}

bool DefaultBookmarksView::IsDoingExtensiveChanges() const {
  return model_->IsDoingExtensiveChanges();
}

const bookmarks::BookmarkNode* DefaultBookmarksView::GetRootNode() const {
  return model_->root_node();
}

std::vector<const bookmarks::BookmarkNode*> DefaultBookmarksView::GetChildren(
    const bookmarks::BookmarkNode* parent) const {
  std::vector<const bookmarks::BookmarkNode*> children;
  if (!parent) {
    return children;
  }
  children.reserve(parent->children().size());
  for (const auto& child : parent->children()) {
    children.push_back(child.get());
  }
  return children;
}

std::optional<const bookmarks::BookmarkNode*>
DefaultBookmarksView::FindNodeByUuid(const base::Uuid& uuid) const {
  const bookmarks::BookmarkNode* node = model_->GetNodeByUuid(
      uuid,
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  if (!node) {
    return std::nullopt;
  }
  return node;
}

bool DefaultBookmarksView::IsPermanentNode(
    const bookmarks::BookmarkNode* node) const {
  return model_->is_permanent_node(node);
}

mojom::PermanentFolderType DefaultBookmarksView::GetPermanentFolderType(
    const bookmarks::BookmarkNode* node) const {
  if (node->type() == bookmarks::BookmarkNode::Type::BOOKMARK_BAR) {
    return mojom::PermanentFolderType::kBookmarkBar;
  }
  if (node->type() == bookmarks::BookmarkNode::Type::OTHER_NODE) {
    return mojom::PermanentFolderType::kOther;
  }
  if (node->type() == bookmarks::BookmarkNode::Type::MOBILE) {
    return mojom::PermanentFolderType::kMobile;
  }
  if (managed_service_ && node == managed_service_->managed_node()) {
    return mojom::PermanentFolderType::kManaged;
  }
  return mojom::PermanentFolderType::kUnknown;
}

bool DefaultBookmarksView::IsSynced(const bookmarks::BookmarkNode* node) const {
  return !model_->IsLocalOnlyNode(*node);
}

const bookmarks::BookmarkNode* DefaultBookmarksView::AddURL(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url) {
  return model_->AddNewURL(parent, index, title, url);
}

const bookmarks::BookmarkNode* DefaultBookmarksView::AddFolder(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title) {
  return model_->AddFolder(parent, index, title);
}

void DefaultBookmarksView::Move(const bookmarks::BookmarkNode* node,
                                const bookmarks::BookmarkNode* new_parent,
                                size_t index) {
  model_->Move(node, new_parent, index);
}

void DefaultBookmarksView::SetTitle(
    const bookmarks::BookmarkNode* node,
    const std::u16string& title,
    bookmarks::metrics::BookmarkEditSource source) {
  model_->SetTitle(node, title, source);
}

void DefaultBookmarksView::SetURL(
    const bookmarks::BookmarkNode* node,
    const GURL& url,
    bookmarks::metrics::BookmarkEditSource source) {
  model_->SetURL(node, url, source);
}

void DefaultBookmarksView::Remove(const bookmarks::BookmarkNode* node,
                                  bookmarks::metrics::BookmarkEditSource source,
                                  const base::Location& location) {
  model_->Remove(node, source, location);
}

void DefaultBookmarksView::RemoveNodes(
    const std::vector<const bookmarks::BookmarkNode*>& nodes,
    bookmarks::metrics::BookmarkEditSource source,
    const base::Location& location) {
  bookmarks::ScopedGroupBookmarkActions group_deletes(model_);
  for (const auto* node : nodes) {
    model_->Remove(node, source, location);
  }
}

}  // namespace bookmarks_api
