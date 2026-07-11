// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_event_translator.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"

namespace bookmarks_api {

BookmarkEventTranslator::BookmarkEventTranslator(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed,
    Subscriber* subscriber)
    : model_(model), managed_(managed), subscriber_(subscriber) {
  CHECK(model_);
  CHECK(subscriber_);
  CHECK(model_->loaded());
  model_->AddObserver(this);
}

BookmarkEventTranslator::~BookmarkEventTranslator() {
  model_->RemoveObserver(this);
}

// static
mojom::RootNodePtr BookmarkEventTranslator::ConvertRootNode(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed,
    const bookmarks::BookmarkNode* node) {
  auto root_node = mojom::RootNode::New();
  root_node->id = node->uuid();
  for (const auto& child : node->children()) {
    root_node->children.push_back(
        ConvertFolderNode(model, managed, child.get()));
  }
  return root_node;
}

// static
mojom::FolderPtr BookmarkEventTranslator::ConvertFolderNode(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed,
    const bookmarks::BookmarkNode* node) {
  auto folder_node = mojom::Folder::New();
  folder_node->id = node->uuid();
  folder_node->title = base::UTF16ToUTF8(node->GetTitle());
  for (const auto& child : node->children()) {
    folder_node->children.push_back(ConvertNode(model, managed, child.get()));
  }
  folder_node->is_synced = model && !model->IsLocalOnlyNode(*node);

  if (node->type() == bookmarks::BookmarkNode::Type::BOOKMARK_BAR) {
    folder_node->permanent_folder_type =
        mojom::PermanentFolderType::kBookmarkBar;
  } else if (node->type() == bookmarks::BookmarkNode::Type::OTHER_NODE) {
    folder_node->permanent_folder_type = mojom::PermanentFolderType::kOther;
  } else if (node->type() == bookmarks::BookmarkNode::Type::MOBILE) {
    folder_node->permanent_folder_type = mojom::PermanentFolderType::kMobile;
  } else if (managed && node == managed->managed_node()) {
    folder_node->permanent_folder_type = mojom::PermanentFolderType::kManaged;
  }

  return folder_node;
}

// static
mojom::BookmarkNodePtr BookmarkEventTranslator::ConvertNode(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed,
    const bookmarks::BookmarkNode* node) {
  switch (node->type()) {
    case bookmarks::BookmarkNode::URL: {
      auto url_node = mojom::Url::New();
      url_node->id = node->uuid();
      url_node->title = base::UTF16ToUTF8(node->GetTitle());
      url_node->url = node->url();
      if (node->icon_url()) {
        url_node->favicon_url = *node->icon_url();
      }
      url_node->is_synced = model && !model->IsLocalOnlyNode(*node);
      return mojom::BookmarkNode::NewUrl(std::move(url_node));
    }
    case bookmarks::BookmarkNode::FOLDER:
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
    case bookmarks::BookmarkNode::OTHER_NODE:
    case bookmarks::BookmarkNode::MOBILE: {
      return mojom::BookmarkNode::NewFolder(
          ConvertFolderNode(model, managed, node));
    }
  }
}

void BookmarkEventTranslator::BookmarkModelBeingDeleted() {
  folders_snapshot_.clear();
}

void BookmarkEventTranslator::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  auto moved_event = mojom::BookmarkNodeMoved::New(
      old_parent->uuid(), static_cast<int32_t>(old_index), new_parent->uuid(),
      static_cast<int32_t>(new_index));

  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(mojom::BookmarksEvent::NewMoved(std::move(moved_event)));

  Notify(std::move(events));
}

void BookmarkEventTranslator::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  const bookmarks::BookmarkNode* node = parent->children()[index].get();
  auto added_event = mojom::BookmarkNodeCreated::New(
      parent->uuid(), static_cast<int32_t>(index),
      ConvertNode(model_, managed_, node));

  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(mojom::BookmarksEvent::NewAdded(std::move(added_event)));

  Notify(std::move(events));
}

void BookmarkEventTranslator::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked,
    const base::Location& location) {
  auto removed_event = mojom::BookmarkNodeRemoved::New(node->uuid());

  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(mojom::BookmarksEvent::NewRemoved(std::move(removed_event)));

  // Drop the removed subtree's entries so the snapshot never holds pointers to
  // freed nodes.
  RemoveFolderSubtree(node);

  Notify(std::move(events));
}

void BookmarkEventTranslator::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  auto changed_event =
      mojom::BookmarkNodeChanged::New(ConvertNode(model_, managed_, node));

  std::vector<mojom::BookmarksEventPtr> events;
  events.push_back(mojom::BookmarksEvent::NewChanged(std::move(changed_event)));
  Notify(std::move(events));
}

void BookmarkEventTranslator::OnWillReorderBookmarkNode(
    const bookmarks::BookmarkNode* node) {
  // ReorderChildren() reports only that `node`'s children were reordered, not
  // how. Capture the pre-reorder order now, while the model still holds it, so
  // BookmarkNodeChildrenReordered() below can diff against it. Capturing here
  // on demand is why the add/move/remove paths never have to maintain the
  // snapshot.
  UpdateFolderChildren(node);
}

void BookmarkEventTranslator::BookmarkNodeChildrenReordered(
    const bookmarks::BookmarkNode* node) {
  std::vector<mojom::BookmarksEventPtr> events;

  // Shuffle forward algorithm. We start at index 0 then progress forward. For
  // any node that isn't where it's supposed to be in the new order, we produce
  // a "shuffle forward" event. This prevents clients from needing to reorder
  // elements that have already been moved.
  const auto& new_ordering = node->children();
  auto current_view = folders_snapshot_[node];
  for (size_t i = 0; i < new_ordering.size(); ++i) {
    const auto& target = new_ordering[i];
    auto it =
        std::find(current_view.begin(), current_view.end(), target->uuid());

    CHECK(it != current_view.end())
        << "a reordered node could not be found in the current folder "
           "snapshot, this should never happen";

    size_t current_index = std::distance(current_view.begin(), it);
    // If the index does not match the expected index.
    if (current_index != i) {
      // Shuffle forward.
      current_view.erase(it);
      current_view.insert(current_view.begin() + i, target->uuid());

      auto moved_event = mojom::BookmarkNodeMoved::New(
          node->uuid(), static_cast<int32_t>(current_index), node->uuid(),
          static_cast<int32_t>(i));
      events.push_back(mojom::BookmarksEvent::NewMoved(std::move(moved_event)));
    }
  }

  Notify(std::move(events));
}

void BookmarkEventTranslator::OnWillRemoveAllUserBookmarks(
    const base::Location& location) {
  // RemoveAllUserBookmarks() empties every permanent folder and fires only
  // BookmarkAllUserNodesRemoved() afterward, once the children are already
  // gone. Snapshot the current tree now so that notification can emit a removed
  // event for each child.
  RefreshFoldersSnapshot();
}

void BookmarkEventTranslator::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  std::vector<mojom::BookmarksEventPtr> events;

  // RemoveAllUserBookmarks() removes the children of every permanent folder
  // except managed ones, across both local and account storage, and fires only
  // this single notification, so enumerate them all here.
  for (const auto& permanent_node : model_->root_node()->children()) {
    if (managed_ && permanent_node.get() == managed_->managed_node()) {
      continue;
    }
    auto it = folders_snapshot_.find(permanent_node.get());
    if (it != folders_snapshot_.end()) {
      for (const auto& child_uuid : it->second) {
        auto removed_event = mojom::BookmarkNodeRemoved::New(child_uuid);
        events.push_back(
            mojom::BookmarksEvent::NewRemoved(std::move(removed_event)));
      }
    }
  }

  // Reset snapshot.
  RefreshFoldersSnapshot();

  Notify(std::move(events));
}

void BookmarkEventTranslator::RefreshFoldersSnapshot() {
  folders_snapshot_.clear();
  PopulateFoldersSnapshot(model_->root_node());
}

void BookmarkEventTranslator::PopulateFoldersSnapshot(
    const bookmarks::BookmarkNode* node) {
  if (node->is_folder()) {
    std::vector<base::Uuid> children;
    for (const auto& child : node->children()) {
      children.push_back(child->uuid());
      PopulateFoldersSnapshot(child.get());
    }
    folders_snapshot_[node] = std::move(children);
  }
}

void BookmarkEventTranslator::ExtensiveBookmarkChangesBeginning() {}

void BookmarkEventTranslator::ExtensiveBookmarkChangesEnded() {
  if (!queued_events_.empty()) {
    subscriber_->OnBookmarkEvents(queued_events_);
    queued_events_.clear();
  }
}

void BookmarkEventTranslator::UpdateFolderChildren(
    const bookmarks::BookmarkNode* parent) {
  // Only OnWillReorderBookmarkNode() calls this, and a node whose children are
  // being reordered is always a folder.
  CHECK(parent->is_folder());
  std::vector<base::Uuid> children;
  children.reserve(parent->children().size());
  for (const auto& child : parent->children()) {
    children.push_back(child->uuid());
  }
  folders_snapshot_[parent] = std::move(children);
}

void BookmarkEventTranslator::RemoveFolderSubtree(
    const bookmarks::BookmarkNode* node) {
  if (!node->is_folder()) {
    return;
  }
  for (const auto& child : node->children()) {
    RemoveFolderSubtree(child.get());
  }
  folders_snapshot_.erase(node);
}

void BookmarkEventTranslator::Notify(
    std::vector<mojom::BookmarksEventPtr> events) {
  if (events.empty()) {
    return;
  }
  if (model_->IsDoingExtensiveChanges()) {
    std::move(events.begin(), events.end(), std::back_inserter(queued_events_));
    return;
  }
  subscriber_->OnBookmarkEvents(events);
}

}  // namespace bookmarks_api
