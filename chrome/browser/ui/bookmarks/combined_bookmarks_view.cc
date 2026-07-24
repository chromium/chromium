// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/combined_bookmarks_view.h"

#include "base/check.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/browser_apis/bookmarks/bookmark_event_translator.h"
#include "components/browser_apis/bookmarks/bookmarks_view_observer.h"
#include "url/gurl.h"

CombinedBookmarksView::CombinedBookmarksView(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed_bookmark_service)
    : model_(model),
      managed_bookmark_service_(managed_bookmark_service),
      synthetic_root_node_(std::make_unique<bookmarks::BookmarkNode>(
          /*id=*/0,
          base::Uuid::GenerateRandomV4(),
          GURL())) {
  CHECK(model_);
  model_observation_.Observe(model_);
  RebuildPermanentNodeUuids();
}

CombinedBookmarksView::~CombinedBookmarksView() = default;

void CombinedBookmarksView::RebuildPermanentNodeUuids() {
  node_id_to_uuid_.clear();
  uuid_to_node_id_.clear();

  const auto& children = model_->root_node()->children();

  auto register_permanent_node =
      [this](const bookmarks::BookmarkNode* model_node) {
        if (!model_node) {
          return;
        }
        base::Uuid uuid = base::Uuid::GenerateRandomV4();
        node_id_to_uuid_[model_node->id()] = uuid;
        uuid_to_node_id_[uuid] = model_node->id();
      };

  for (const auto& child : children) {
    register_permanent_node(child.get());
  }
  if (managed_bookmark_service_ && managed_bookmark_service_->managed_node()) {
    register_permanent_node(managed_bookmark_service_->managed_node());
  }
}

void CombinedBookmarksView::AddObserver(
    bookmarks_api::BookmarksViewObserver* observer) {
  observers_.AddObserver(observer);
}

void CombinedBookmarksView::RemoveObserver(
    bookmarks_api::BookmarksViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool CombinedBookmarksView::IsDoingExtensiveChanges() const {
  return model_->IsDoingExtensiveChanges();
}

const bookmarks::BookmarkNode* CombinedBookmarksView::GetRootNode() const {
  return synthetic_root_node_.get();
}

std::vector<const bookmarks::BookmarkNode*> CombinedBookmarksView::GetChildren(
    const bookmarks::BookmarkNode* parent) const {
  CHECK(parent != nullptr);
  CHECK(parent->is_folder());

  if (parent == synthetic_root_node_.get()) {
    std::vector<const bookmarks::BookmarkNode*> children;
    for (const auto& child : model_->root_node()->children()) {
      children.push_back(child.get());
    }
    if (managed_bookmark_service_ &&
        managed_bookmark_service_->managed_node()) {
      children.push_back(managed_bookmark_service_->managed_node());
    }
    return children;
  }

  std::vector<const bookmarks::BookmarkNode*> children;
  children.reserve(parent->children().size());
  for (const auto& child : parent->children()) {
    children.push_back(child.get());
  }
  return children;
}

std::optional<const bookmarks::BookmarkNode*>
CombinedBookmarksView::FindNodeByUuid(const base::Uuid& uuid) const {
  if (uuid == synthetic_root_node_->uuid()) {
    return synthetic_root_node_.get();
  }
  auto it = uuid_to_node_id_.find(uuid);
  if (it != uuid_to_node_id_.end()) {
    int64_t target_id = it->second;
    for (const auto& child : model_->root_node()->children()) {
      if (child->id() == target_id) {
        return child.get();
      }
    }
    if (managed_bookmark_service_ &&
        managed_bookmark_service_->managed_node() &&
        managed_bookmark_service_->managed_node()->id() == target_id) {
      return managed_bookmark_service_->managed_node();
    }
  }
  const bookmarks::BookmarkNode* node = model_->GetNodeByUuid(
      uuid, bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
  if (!node) {
    node = model_->GetNodeByUuid(
        uuid,
        bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  }
  if (!node) {
    return std::nullopt;
  }
  return node;
}

bool CombinedBookmarksView::IsPermanentNode(
    const bookmarks::BookmarkNode* node) const {
  if (node == synthetic_root_node_.get() ||
      node_id_to_uuid_.contains(node->id())) {
    return true;
  }
  return model_->is_permanent_node(node);
}

bookmarks_api::mojom::PermanentFolderType
CombinedBookmarksView::GetPermanentFolderType(
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
  if (managed_bookmark_service_ &&
      node == managed_bookmark_service_->managed_node()) {
    return bookmarks_api::mojom::PermanentFolderType::kManaged;
  }
  return bookmarks_api::mojom::PermanentFolderType::kUnknown;
}

bool CombinedBookmarksView::IsSynced(
    const bookmarks::BookmarkNode* node) const {
  if (node == synthetic_root_node_.get()) {
    return false;
  }
  return !model_->IsLocalOnlyNode(*node);
}

const bookmarks::BookmarkNode* CombinedBookmarksView::AddURL(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url) {
  CHECK(parent != nullptr);
  CHECK(parent->is_folder());

  return model_->AddNewURL(parent, index, title, url);
}

const bookmarks::BookmarkNode* CombinedBookmarksView::AddFolder(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title) {
  CHECK(parent != nullptr);
  CHECK(parent->is_folder());

  return model_->AddFolder(parent, index, title);
}

void CombinedBookmarksView::Move(const bookmarks::BookmarkNode* node,
                                 const bookmarks::BookmarkNode* new_parent,
                                 size_t index) {
  model_->Move(node, new_parent, index);
}

void CombinedBookmarksView::SetTitle(
    const bookmarks::BookmarkNode* node,
    const std::u16string& title,
    bookmarks::metrics::BookmarkEditSource source) {
  model_->SetTitle(node, title, source);
}

void CombinedBookmarksView::SetURL(
    const bookmarks::BookmarkNode* node,
    const GURL& url,
    bookmarks::metrics::BookmarkEditSource source) {
  model_->SetURL(node, url, source);
}

void CombinedBookmarksView::Remove(
    const bookmarks::BookmarkNode* node,
    bookmarks::metrics::BookmarkEditSource source,
    const base::Location& location) {
  model_->Remove(node, source, location);
}

void CombinedBookmarksView::RemoveNodes(
    const std::vector<const bookmarks::BookmarkNode*>& nodes,
    bookmarks::metrics::BookmarkEditSource source,
    const base::Location& location) {
  bookmarks::ScopedGroupBookmarkActions group_deletes(model_);
  for (const auto* node : nodes) {
    model_->Remove(node, source, location);
  }
}

void CombinedBookmarksView::BookmarkModelLoaded(bool ids_reassigned) {
  RebuildPermanentNodeUuids();
}

void CombinedBookmarksView::BookmarkModelBeingDeleted() {
  for (auto& observer : observers_) {
    observer.OnBookmarksViewBeingDeleted(this);
  }
}

void CombinedBookmarksView::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  std::vector<bookmarks_api::mojom::BookmarksEventPtr> events;
  events.push_back(bookmarks_api::BookmarkEventTranslator::CreateAddedEvent(
      this, parent, index));
  Notify(std::move(events));
}

void CombinedBookmarksView::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  std::vector<bookmarks_api::mojom::BookmarksEventPtr> events;
  events.push_back(
      bookmarks_api::BookmarkEventTranslator::CreateRemovedEvent(node));
  Notify(std::move(events));
}

void CombinedBookmarksView::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  std::vector<bookmarks_api::mojom::BookmarksEventPtr> events;
  events.push_back(bookmarks_api::BookmarkEventTranslator::CreateMovedEvent(
      old_parent, old_index, new_parent, new_index));
  Notify(std::move(events));
}

void CombinedBookmarksView::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  std::vector<bookmarks_api::mojom::BookmarksEventPtr> events;
  events.push_back(
      bookmarks_api::BookmarkEventTranslator::CreateChangedEvent(this, node));
  Notify(std::move(events));
}

void CombinedBookmarksView::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {
  BookmarkNodeChanged(node);
}

void CombinedBookmarksView::BookmarkNodeChildrenReordered(
    const bookmarks::BookmarkNode* node) {}

void CombinedBookmarksView::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {}

void CombinedBookmarksView::ExtensiveBookmarkChangesBeginning() {
  // Extensive changes are handled internally by queueing events.
}

void CombinedBookmarksView::ExtensiveBookmarkChangesEnded() {
  if (!queued_events_.empty()) {
    for (auto& observer : observers_) {
      observer.OnBookmarksEvents(this, queued_events_);
    }
    queued_events_.clear();
  }
}

void CombinedBookmarksView::Notify(
    std::vector<bookmarks_api::mojom::BookmarksEventPtr> events) {
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
