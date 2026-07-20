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

// static
mojom::RootNodePtr BookmarkEventTranslator::ConvertRootNode(
    const bookmarks::BookmarkNode* node,
    const BookmarksView* view) {
  CHECK(view);
  auto root_node = mojom::RootNode::New();
  root_node->id = node->uuid();
  for (const bookmarks::BookmarkNode* child : view->GetChildren(node)) {
    root_node->children.push_back(ConvertFolderNode(child, view));
  }
  return root_node;
}

// static
mojom::FolderPtr BookmarkEventTranslator::ConvertFolderNode(
    const bookmarks::BookmarkNode* node,
    const BookmarksView* view) {
  CHECK(view);
  auto folder_node = mojom::Folder::New();
  folder_node->id = node->uuid();
  folder_node->title = base::UTF16ToUTF8(node->GetTitle());
  for (const bookmarks::BookmarkNode* child : view->GetChildren(node)) {
    folder_node->children.push_back(ConvertNode(child, view));
  }
  folder_node->is_synced = view->IsSynced(node);
  mojom::PermanentFolderType perm_type = view->GetPermanentFolderType(node);
  if (perm_type != mojom::PermanentFolderType::kUnknown) {
    folder_node->permanent_folder_type = perm_type;
  }

  return folder_node;
}

// static
mojom::BookmarkNodePtr BookmarkEventTranslator::ConvertNode(
    const bookmarks::BookmarkNode* node,
    const BookmarksView* view) {
  switch (node->type()) {
    case bookmarks::BookmarkNode::URL: {
      auto url_node = mojom::Url::New();
      url_node->id = node->uuid();
      url_node->title = base::UTF16ToUTF8(node->GetTitle());
      url_node->url = node->url();
      if (node->icon_url()) {
        url_node->favicon_url = *node->icon_url();
      }
      url_node->is_synced = view->IsSynced(node);
      return mojom::BookmarkNode::NewUrl(std::move(url_node));
    }
    case bookmarks::BookmarkNode::FOLDER:
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
    case bookmarks::BookmarkNode::OTHER_NODE:
    case bookmarks::BookmarkNode::MOBILE: {
      return mojom::BookmarkNode::NewFolder(ConvertFolderNode(node, view));
    }
  }
}

// static
mojom::BookmarksEventPtr BookmarkEventTranslator::CreateAddedEvent(
    const BookmarksView* view,
    const bookmarks::BookmarkNode* parent,
    size_t index) {
  const bookmarks::BookmarkNode* node = view->GetChildren(parent)[index];
  auto added_event = mojom::BookmarkNodeCreated::New(
      parent->uuid(), static_cast<int32_t>(index), ConvertNode(node, view));
  return mojom::BookmarksEvent::NewAdded(std::move(added_event));
}

// static
mojom::BookmarksEventPtr BookmarkEventTranslator::CreateRemovedEvent(
    const bookmarks::BookmarkNode* node) {
  auto removed_event = mojom::BookmarkNodeRemoved::New(node->uuid());
  return mojom::BookmarksEvent::NewRemoved(std::move(removed_event));
}

// static
mojom::BookmarksEventPtr BookmarkEventTranslator::CreateMovedEvent(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  auto moved_event = mojom::BookmarkNodeMoved::New(
      old_parent->uuid(), static_cast<int32_t>(old_index), new_parent->uuid(),
      static_cast<int32_t>(new_index));
  return mojom::BookmarksEvent::NewMoved(std::move(moved_event));
}

// static
mojom::BookmarksEventPtr BookmarkEventTranslator::CreateChangedEvent(
    const BookmarksView* view,
    const bookmarks::BookmarkNode* node) {
  auto changed_event = mojom::BookmarkNodeChanged::New(ConvertNode(node, view));
  return mojom::BookmarksEvent::NewChanged(std::move(changed_event));
}

class BookmarkEventTranslator::FolderSnapshot {
 public:
  FolderSnapshot() = default;
  ~FolderSnapshot() = default;

  void Clear() { snapshot_.clear(); }

  void Refresh(const BookmarksView* view) {
    Clear();
    if (view && view->GetRootNode()) {
      Populate(view->GetRootNode(), view);
    }
  }

  void UpdateFolder(const bookmarks::BookmarkNode* parent,
                    const BookmarksView* view) {
    CHECK(parent->is_folder());
    std::vector<base::Uuid> children;
    const auto view_children = view->GetChildren(parent);
    children.reserve(view_children.size());
    for (const auto* child : view_children) {
      children.push_back(child->uuid());
    }
    snapshot_[parent] = std::move(children);
  }

  void RemoveFolderSubtree(const bookmarks::BookmarkNode* node) {
    if (!node->is_folder()) {
      return;
    }
    for (const auto& child : node->children()) {
      RemoveFolderSubtree(child.get());
    }
    snapshot_.erase(node);
  }

  std::vector<mojom::BookmarksEventPtr> DiffReorderedFolder(
      const bookmarks::BookmarkNode* parent,
      const BookmarksView* view) {
    std::vector<mojom::BookmarksEventPtr> events;
    const auto new_ordering = view->GetChildren(parent);
    auto it_snapshot = snapshot_.find(parent);
    if (it_snapshot == snapshot_.end()) {
      return events;
    }
    auto& current_view = it_snapshot->second;

    for (size_t i = 0; i < new_ordering.size(); ++i) {
      const auto* target = new_ordering[i];
      auto it =
          std::find(current_view.begin(), current_view.end(), target->uuid());

      CHECK(it != current_view.end())
          << "a reordered node could not be found in the current folder "
             "snapshot, this should never happen";

      size_t current_index = std::distance(current_view.begin(), it);
      if (current_index != i) {
        current_view.erase(it);
        current_view.insert(current_view.begin() + i, target->uuid());
        auto moved_event = mojom::BookmarkNodeMoved::New(
            parent->uuid(), static_cast<int32_t>(current_index), parent->uuid(),
            static_cast<int32_t>(i));
        events.push_back(
            mojom::BookmarksEvent::NewMoved(std::move(moved_event)));
      }
    }
    UpdateFolder(parent, view);
    return events;
  }

  std::vector<mojom::BookmarksEventPtr> ClearAllUserBookmarks(
      const BookmarksView* view) {
    std::vector<mojom::BookmarksEventPtr> events;
    for (const auto* permanent_node : view->GetChildren(view->GetRootNode())) {
      if (!permanent_node || !view->IsPermanentNode(permanent_node)) {
        continue;
      }
      auto it = snapshot_.find(permanent_node);
      if (it != snapshot_.end()) {
        for (const auto& child_uuid : it->second) {
          auto removed_event = mojom::BookmarkNodeRemoved::New(child_uuid);
          events.push_back(
              mojom::BookmarksEvent::NewRemoved(std::move(removed_event)));
        }
      }
    }
    Refresh(view);
    return events;
  }

 private:
  void Populate(const bookmarks::BookmarkNode* node,
                const BookmarksView* view) {
    if (node->is_folder()) {
      std::vector<base::Uuid> children;
      for (const bookmarks::BookmarkNode* child : view->GetChildren(node)) {
        children.push_back(child->uuid());
        Populate(child, view);
      }
      snapshot_[node] = std::move(children);
    }
  }

  std::map<const bookmarks::BookmarkNode*, std::vector<base::Uuid>> snapshot_;
};

BookmarkEventTranslator::BookmarkEventTranslator()
    : snapshot_(std::make_unique<FolderSnapshot>()) {}

BookmarkEventTranslator::BookmarkEventTranslator(const BookmarksView* view)
    : snapshot_(std::make_unique<FolderSnapshot>()) {
  if (view) {
    Init(view);
  }
}

BookmarkEventTranslator::~BookmarkEventTranslator() = default;

void BookmarkEventTranslator::Init(const BookmarksView* view) {
  snapshot_->Refresh(view);
}

mojom::BookmarksEventPtr BookmarkEventTranslator::OnNodeRemoved(
    const bookmarks::BookmarkNode* node) {
  snapshot_->RemoveFolderSubtree(node);
  return CreateRemovedEvent(node);
}

void BookmarkEventTranslator::OnWillReorderFolder(
    const bookmarks::BookmarkNode* parent,
    const BookmarksView* view) {
  snapshot_->UpdateFolder(parent, view);
}

std::vector<mojom::BookmarksEventPtr>
BookmarkEventTranslator::OnFolderReordered(
    const bookmarks::BookmarkNode* parent,
    const BookmarksView* view) {
  return snapshot_->DiffReorderedFolder(parent, view);
}

void BookmarkEventTranslator::OnWillRemoveAllUserBookmarks(
    const BookmarksView* view) {
  snapshot_->Refresh(view);
}

std::vector<mojom::BookmarksEventPtr>
BookmarkEventTranslator::OnAllUserBookmarksRemoved(const BookmarksView* view) {
  return snapshot_->ClearAllUserBookmarks(view);
}

}  // namespace bookmarks_api
