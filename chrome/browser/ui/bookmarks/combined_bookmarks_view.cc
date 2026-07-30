// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/combined_bookmarks_view.h"

#include <inttypes.h>

#include "base/check.h"
#include "base/check_deref.h"
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
  CHECK(model_->loaded()) << "the view can only accept loaded models";
  model_observation_.Observe(model_);
  RegisterAccountNodeOverrides();
  translator_.Init();
}

CombinedBookmarksView::~CombinedBookmarksView() = default;

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

namespace {

const bookmarks::BookmarkNode* FindNodeById(
    const bookmarks::BookmarkModel* model,
    int64_t id) {
  if (!model) {
    return nullptr;
  }
  if (model->account_bookmark_bar_node() &&
      model->account_bookmark_bar_node()->id() == id) {
    return model->account_bookmark_bar_node();
  }
  if (model->account_other_node() && model->account_other_node()->id() == id) {
    return model->account_other_node();
  }
  if (model->account_mobile_node() &&
      model->account_mobile_node()->id() == id) {
    return model->account_mobile_node();
  }
  return nullptr;
}

}  // namespace

std::optional<const bookmarks::BookmarkNode*>
CombinedBookmarksView::FindNodeByUuid(const base::Uuid& uuid) const {
  if (uuid == synthetic_root_node_->uuid()) {
    return synthetic_root_node_.get();
  }
  if (std::optional<int64_t> override_id =
          uuid_mapper_.MaybeGetIdFromUuidOverride(uuid)) {
    const bookmarks::BookmarkNode* found = FindNodeById(model_, *override_id);
    if (found) {
      return found;
    }
  }

  const bookmarks::BookmarkNode* node = model_->GetNodeByUuid(
      uuid,
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
  if (!node) {
    node = model_->GetNodeByUuid(
        uuid, bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
  }
  return node ? std::optional<const bookmarks::BookmarkNode*>(node)
              : std::nullopt;
}

bool CombinedBookmarksView::IsPermanentNode(
    const bookmarks::BookmarkNode* node) const {
  if (node == synthetic_root_node_.get()) {
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

// For signed-in profiles with account bookmark storage enabled, assign unique
// UUID overrides to account permanent folders so they do not collide with local
// permanent folder UUIDs. We infer signed-in status by checking if
// account_bookmark_bar_node() exists.
void CombinedBookmarksView::RegisterAccountNodeOverrides() {
  if (!model_->account_bookmark_bar_node()) {
    return;
  }

  if (!uuid_mapper_.HasOverrideFor(model_->account_bookmark_bar_node())) {
    if (model_->account_bookmark_bar_node()) {
      uuid_mapper_.SetUuidOverride(model_->account_bookmark_bar_node(),
                                   base::Uuid::GenerateRandomV4());
    }
    if (model_->account_other_node()) {
      uuid_mapper_.SetUuidOverride(model_->account_other_node(),
                                   base::Uuid::GenerateRandomV4());
    }
    if (model_->account_mobile_node()) {
      uuid_mapper_.SetUuidOverride(model_->account_mobile_node(),
                                   base::Uuid::GenerateRandomV4());
    }
  }
}

base::Uuid CombinedBookmarksView::GetUuid(
    const bookmarks::BookmarkNode* node) const {
  if (!node) {
    return base::Uuid();
  }
  if (node == synthetic_root_node_.get()) {
    return synthetic_root_node_->uuid();
  }
  return uuid_mapper_.GetUuidFor(node);
}

bool CombinedBookmarksView::IsSynced(
    const bookmarks::BookmarkNode* node) const {
  if (node == synthetic_root_node_.get()) {
    return false;
  }
  return !model_->IsLocalOnlyNode(*node);
}

const bookmarks_api::BookmarkEventTranslator&
CombinedBookmarksView::GetEventTranslator() const {
  return translator_;
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

void CombinedBookmarksView::BookmarkModelLoaded(bool ids_reassigned) {}

void CombinedBookmarksView::BookmarkModelBeingDeleted() {
  for (auto& observer : observers_) {
    observer.OnBookmarksViewBeingDeleted(this);
  }
}

void CombinedBookmarksView::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  // If account permanent folders were created dynamically (e.g., user signed in
  // or enabled bookmark sync during an active session), register their
  // overrides.
  RegisterAccountNodeOverrides();
  std::vector<bookmarks_api::mojom::BookmarksEventPtr> events;
  events.push_back(translator_.CreateAddedEvent(parent, index));
  Notify(std::move(events));
}

void CombinedBookmarksView::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  std::vector<bookmarks_api::mojom::BookmarksEventPtr> events;
  events.push_back(translator_.OnNodeRemoved(node));
  Notify(std::move(events));
}

void CombinedBookmarksView::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  std::vector<bookmarks_api::mojom::BookmarksEventPtr> events;
  events.push_back(translator_.CreateMovedEvent(old_parent, old_index,
                                                new_parent, new_index));
  Notify(std::move(events));
}

void CombinedBookmarksView::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  std::vector<bookmarks_api::mojom::BookmarksEventPtr> events;
  events.push_back(translator_.CreateChangedEvent(node));
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
