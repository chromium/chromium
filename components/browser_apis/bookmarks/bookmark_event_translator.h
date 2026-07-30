// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"
#include "components/browser_apis/bookmarks/bookmarks_view.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace bookmarks_api {

// Helper class to translate BookmarkModel/View changes to Mojo events.
class BookmarkEventTranslator {
 public:
  explicit BookmarkEventTranslator(const BookmarksView* view);
  ~BookmarkEventTranslator();

  BookmarkEventTranslator(const BookmarkEventTranslator&) = delete;
  BookmarkEventTranslator& operator=(const BookmarkEventTranslator&) = delete;

  // Initializes or refreshes the internal snapshot state for the view.
  void Init();

  // Conversion helpers.
  mojom::BookmarkNodePtr ConvertNode(const bookmarks::BookmarkNode* node) const;
  mojom::RootNodePtr ConvertRootNode(const bookmarks::BookmarkNode* node) const;
  mojom::FolderPtr ConvertFolderNode(const bookmarks::BookmarkNode* node) const;

  // Event builders.
  mojom::BookmarksEventPtr CreateAddedEvent(
      const bookmarks::BookmarkNode* parent,
      size_t index) const;

  mojom::BookmarksEventPtr CreateRemovedEvent(
      const bookmarks::BookmarkNode* node) const;

  mojom::BookmarksEventPtr CreateMovedEvent(
      const bookmarks::BookmarkNode* old_parent,
      size_t old_index,
      const bookmarks::BookmarkNode* new_parent,
      size_t new_index) const;

  mojom::BookmarksEventPtr CreateChangedEvent(
      const bookmarks::BookmarkNode* node) const;

  // Instance methods that manage snapshot tracking and translation for
  // removals, reordering, and clearing all user nodes.
  mojom::BookmarksEventPtr OnNodeRemoved(const bookmarks::BookmarkNode* node);
  void OnWillReorderFolder(const bookmarks::BookmarkNode* parent);
  std::vector<mojom::BookmarksEventPtr> OnFolderReordered(
      const bookmarks::BookmarkNode* parent);
  void OnWillRemoveAllUserBookmarks();
  std::vector<mojom::BookmarksEventPtr> OnAllUserBookmarksRemoved();

 private:
  class FolderSnapshot;

  raw_ptr<const BookmarksView> view_;
  std::unique_ptr<FolderSnapshot> snapshot_;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_
