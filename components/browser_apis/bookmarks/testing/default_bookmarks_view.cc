// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/testing/default_bookmarks_view.h"

#include "base/check.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/browser_apis/bookmarks/bookmarks_view_observer.h"

namespace bookmarks_api {

DefaultBookmarksView::DefaultBookmarksView(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed_service)
    : model_(model), managed_service_(managed_service) {
  CHECK(model_);
  model_observation_.Observe(model_);
  if (model_->loaded()) {
    translator_.Init(this);
  }
}

DefaultBookmarksView::~DefaultBookmarksView() = default;

void DefaultBookmarksView::AddObserver(BookmarksViewObserver* observer) {
  observers_.AddObserver(observer);
}

void DefaultBookmarksView::RemoveObserver(BookmarksViewObserver* observer) {
  observers_.RemoveObserver(observer);
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

void DefaultBookmarksView::BookmarkModelLoaded(bool ids_reassigned) {
  translator_.Init(this);
}

void DefaultBookmarksView::BookmarkModelBeingDeleted() {
  for (auto& observer : observers_) {
    observer.OnBookmarksViewBeingDeleted(this);
  }
}

void DefaultBookmarksView::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(BookmarkEventTranslator::CreateMovedEvent(
      old_parent, old_index, new_parent, new_index));
  Notify(std::move(events));
}

void DefaultBookmarksView::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(
      BookmarkEventTranslator::CreateAddedEvent(this, parent, index));
  Notify(std::move(events));
}

void DefaultBookmarksView::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked,
    const base::Location& location) {
  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(translator_.OnNodeRemoved(node));
  Notify(std::move(events));
}

void DefaultBookmarksView::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(BookmarkEventTranslator::CreateChangedEvent(this, node));
  Notify(std::move(events));
}

void DefaultBookmarksView::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {
  // Favicon changes are not propagated as Mojo events.
}

void DefaultBookmarksView::BookmarkNodeChildrenReordered(
    const bookmarks::BookmarkNode* node) {
  Notify(translator_.OnFolderReordered(node, this));
}

void DefaultBookmarksView::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  Notify(translator_.OnAllUserBookmarksRemoved(this));
}

void DefaultBookmarksView::OnWillReorderBookmarkNode(
    const bookmarks::BookmarkNode* node) {
  translator_.OnWillReorderFolder(node, this);
}

void DefaultBookmarksView::OnWillRemoveAllUserBookmarks(
    const base::Location& location) {
  translator_.OnWillRemoveAllUserBookmarks(this);
}

void DefaultBookmarksView::ExtensiveBookmarkChangesBeginning() {
  // Extensive changes are handled internally by queueing events.
}

void DefaultBookmarksView::ExtensiveBookmarkChangesEnded() {
  if (!queued_events_.empty()) {
    for (auto& observer : observers_) {
      observer.OnBookmarksEvents(this, queued_events_);
    }
    queued_events_.clear();
  }
}

void DefaultBookmarksView::Notify(
    std::vector<mojom::BookmarksEventPtr> events) {
  if (events.empty()) {
    return;
  }
  if (IsDoingExtensiveChanges()) {
    std::move(events.begin(), events.end(), std::back_inserter(queued_events_));
    return;
  }
  for (auto& observer : observers_) {
    observer.OnBookmarksEvents(this, events);
  }
}

}  // namespace bookmarks_api
