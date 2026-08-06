// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_BOOKMARKS_TESTING_DEFAULT_BOOKMARKS_VIEW_H_
#define COMPONENTS_BROWSER_APIS_BOOKMARKS_TESTING_DEFAULT_BOOKMARKS_VIEW_H_

#include <optional>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/browser_apis/bookmarks/bookmark_event_translator.h"
#include "components/browser_apis/bookmarks/bookmark_uuid_mapper.h"
#include "components/browser_apis/bookmarks/bookmarks_view.h"
#include "url/gurl.h"

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
}  // namespace bookmarks

namespace bookmarks_api {

class BookmarksViewObserver;

// Default implementation that directly forwards operations to BookmarkModel.
// This is intended for use in tests where a merged view is not needed.
class DefaultBookmarksView : public BookmarksView,
                             public bookmarks::BookmarkModelObserver {
 public:
  explicit DefaultBookmarksView(
      bookmarks::BookmarkModel* model,
      bookmarks::ManagedBookmarkService* managed_service = nullptr);
  ~DefaultBookmarksView() override;

  DefaultBookmarksView(const DefaultBookmarksView&) = delete;
  DefaultBookmarksView& operator=(const DefaultBookmarksView&) = delete;

  // BookmarksView:
  void AddObserver(BookmarksViewObserver* observer) override;
  void RemoveObserver(BookmarksViewObserver* observer) override;
  bool IsDoingExtensiveChanges() const override;
  const bookmarks::BookmarkNode* GetRootNode() const override;
  std::vector<const bookmarks::BookmarkNode*> GetChildren(
      const bookmarks::BookmarkNode* parent) const override;
  std::optional<const bookmarks::BookmarkNode*> FindNodeByUuid(
      const base::Uuid& uuid) const override;
  bool IsPermanentNode(const bookmarks::BookmarkNode* node) const override;
  mojom::PermanentFolderType GetPermanentFolderType(
      const bookmarks::BookmarkNode* node) const override;
  base::Uuid GetUuid(const bookmarks::BookmarkNode* node) override;
  bool IsSynced(const bookmarks::BookmarkNode* node) const override;
  BookmarkEventTranslator& GetEventTranslator() override;
  const bookmarks::BookmarkNode* AddURL(const bookmarks::BookmarkNode* parent,
                                        size_t index,
                                        const std::u16string& title,
                                        const GURL& url) override;
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      const std::u16string& title) override;
  void Move(const bookmarks::BookmarkNode* node,
            const bookmarks::BookmarkNode* new_parent,
            size_t index) override;
  void SetTitle(const bookmarks::BookmarkNode* node,
                const std::u16string& title,
                bookmarks::metrics::BookmarkEditSource source) override;
  void SetURL(const bookmarks::BookmarkNode* node,
              const GURL& url,
              bookmarks::metrics::BookmarkEditSource source) override;
  void Remove(const bookmarks::BookmarkNode* node,
              bookmarks::metrics::BookmarkEditSource source,
              const base::Location& location) override;
  void RemoveNodes(const std::vector<const bookmarks::BookmarkNode*>& nodes,
                   bookmarks::metrics::BookmarkEditSource source,
                   const base::Location& location) override;

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
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
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void OnWillReorderBookmarkNode(const bookmarks::BookmarkNode* node) override;
  void OnWillRemoveAllUserBookmarks(const base::Location& location) override;
  void ExtensiveBookmarkChangesBeginning() override;
  void ExtensiveBookmarkChangesEnded() override;

 private:
  void Notify(std::vector<mojom::BookmarksEventPtr> events);
  void RegisterAccountNodeOverrides();

  raw_ptr<bookmarks::BookmarkModel> model_;
  raw_ptr<bookmarks::ManagedBookmarkService> managed_service_;
  base::ObserverList<BookmarksViewObserver> observers_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      model_observation_{this};

  BookmarkUuidMapper uuid_mapper_;
  BookmarkEventTranslator translator_{this};
  std::vector<mojom::BookmarksEventPtr> queued_events_;
};

}  // namespace bookmarks_api

#endif  // COMPONENTS_BROWSER_APIS_BOOKMARKS_TESTING_DEFAULT_BOOKMARKS_VIEW_H_
