// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"
#include "components/browser_apis/bookmarks/bookmarks_view.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace bookmarks_api {

// Translates between BookmarkModelObserver to mojo based event types.
class BookmarkEventTranslator : public bookmarks::BookmarkModelObserver {
 public:
  class Subscriber {
   public:
    virtual void OnBookmarkEvents(
        const std::vector<mojom::BookmarksEventPtr>& events) = 0;

   protected:
    virtual ~Subscriber() = default;
  };

  BookmarkEventTranslator(BookmarksView* view, Subscriber* subscriber);
  BookmarkEventTranslator(const BookmarkEventTranslator&) = delete;
  BookmarkEventTranslator& operator=(const BookmarkEventTranslator&) = delete;
  ~BookmarkEventTranslator() override;

  static mojom::BookmarkNodePtr ConvertNode(const bookmarks::BookmarkNode* node,
                                            const BookmarksView* view);

  static mojom::RootNodePtr ConvertRootNode(const bookmarks::BookmarkNode* node,
                                            const BookmarksView* view);

  static mojom::FolderPtr ConvertFolderNode(const bookmarks::BookmarkNode* node,
                                            const BookmarksView* view);

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override {}
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked,
                           const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void OnWillReorderBookmarkNode(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void OnWillRemoveAllUserBookmarks(const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void ExtensiveBookmarkChangesBeginning() override;
  void ExtensiveBookmarkChangesEnded() override;

 private:
  void RefreshFoldersSnapshot();
  void PopulateFoldersSnapshot(const bookmarks::BookmarkNode* node);
  // Rebuilds `parent`'s snapshot entry from its current children.
  void UpdateFolderChildren(const bookmarks::BookmarkNode* parent);
  // Erases the snapshot entries for `node` and all descendant folders.
  void RemoveFolderSubtree(const bookmarks::BookmarkNode* node);
  void Notify(std::vector<mojom::BookmarksEventPtr> events);

  raw_ptr<BookmarksView> view_;
  raw_ptr<Subscriber> subscriber_;
  // A snapshot of the folder structure (mapping each folder node to its
  // children's UUIDs) used to detect changes (adds, removes, moves) in the
  // bookmark model. Keyed by node pointer rather than UUID because permanent
  // folders are not uniquely identified by UUID: account and local permanent
  // folders share the same fixed UUIDs. This is necessary because the bookmark
  // model has a "reorder" event type, which performs several move operations at
  // once. We need to keep an old snapshot to compute individual move events.
  // The snapshot is captured on demand in OnWillReorderBookmarkNode() and
  // OnWillRemoveAllUserBookmarks(), just before the model applies those
  // changes, so the add/move/remove paths don't have to maintain it. Entries
  // are cleared in BookmarkModelBeingDeleted() so the raw pointers never
  // dangle.
  std::map<const bookmarks::BookmarkNode*, std::vector<base::Uuid>>
      folders_snapshot_;

  std::vector<mojom::BookmarksEventPtr> queued_events_;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_BOOKMARK_EVENT_TRANSLATOR_H_
