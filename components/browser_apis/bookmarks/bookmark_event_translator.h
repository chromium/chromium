// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_

#include <map>
#include <vector>

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
  BookmarkEventTranslator();
  explicit BookmarkEventTranslator(const BookmarksView* view);
  ~BookmarkEventTranslator();

  BookmarkEventTranslator(const BookmarkEventTranslator&) = delete;
  BookmarkEventTranslator& operator=(const BookmarkEventTranslator&) = delete;

  // Initializes or refreshes the internal snapshot state for `view`.
  void Init(const BookmarksView* view);

  // Static conversion helpers.
  static mojom::BookmarkNodePtr ConvertNode(const bookmarks::BookmarkNode* node,
                                            const BookmarksView* view);

  static mojom::RootNodePtr ConvertRootNode(const bookmarks::BookmarkNode* node,
                                            const BookmarksView* view);

  static mojom::FolderPtr ConvertFolderNode(const bookmarks::BookmarkNode* node,
                                            const BookmarksView* view);

  // Static event builders.
  static mojom::BookmarksEventPtr CreateAddedEvent(
      const BookmarksView* view,
      const bookmarks::BookmarkNode* parent,
      size_t index);

  static mojom::BookmarksEventPtr CreateRemovedEvent(
      const bookmarks::BookmarkNode* node);

  static mojom::BookmarksEventPtr CreateMovedEvent(
      const bookmarks::BookmarkNode* old_parent,
      size_t old_index,
      const bookmarks::BookmarkNode* new_parent,
      size_t new_index);

  static mojom::BookmarksEventPtr CreateChangedEvent(
      const BookmarksView* view,
      const bookmarks::BookmarkNode* node);

  // Instance methods that manage snapshot tracking and translation for
  // removals, reordering, and clearing all user nodes.
  mojom::BookmarksEventPtr OnNodeRemoved(const bookmarks::BookmarkNode* node);

  void OnWillReorderFolder(const bookmarks::BookmarkNode* parent,
                           const BookmarksView* view);

  std::vector<mojom::BookmarksEventPtr> OnFolderReordered(
      const bookmarks::BookmarkNode* parent,
      const BookmarksView* view);

  void OnWillRemoveAllUserBookmarks(const BookmarksView* view);

  std::vector<mojom::BookmarksEventPtr> OnAllUserBookmarksRemoved(
      const BookmarksView* view);

 private:
  class FolderSnapshot;
  std::unique_ptr<FolderSnapshot> snapshot_;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_
