// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MERGED_SURFACE_VIEW_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MERGED_SURFACE_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/browser_apis/bookmarks/bookmark_event_translator.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"
#include "components/browser_apis/bookmarks/bookmarks_view.h"
#include "url/gurl.h"

class BookmarkMergedSurfaceService;

// Implements BookmarksView by delegating queries and mutations to
// BookmarkMergedSurfaceService, enabling merged local and account bookmark
// hierarchies in WebUI services (like Bookmarks Manager).
class BookmarkMergedSurfaceView : public bookmarks_api::BookmarksView,
                                  public BookmarkMergedSurfaceServiceObserver {
 public:
  explicit BookmarkMergedSurfaceView(BookmarkMergedSurfaceService* service);
  ~BookmarkMergedSurfaceView() override;

  BookmarkMergedSurfaceView(const BookmarkMergedSurfaceView&) = delete;
  BookmarkMergedSurfaceView& operator=(const BookmarkMergedSurfaceView&) =
      delete;

  // bookmarks_api::BookmarksView:
  void AddObserver(bookmarks_api::BookmarksViewObserver* observer) override;
  void RemoveObserver(bookmarks_api::BookmarksViewObserver* observer) override;
  bool IsDoingExtensiveChanges() const override;
  const bookmarks::BookmarkNode* GetRootNode() const override;
  std::vector<const bookmarks::BookmarkNode*> GetChildren(
      const bookmarks::BookmarkNode* parent) const override;
  std::optional<const bookmarks::BookmarkNode*> FindNodeByUuid(
      const base::Uuid& uuid) const override;
  bool IsPermanentNode(const bookmarks::BookmarkNode* node) const override;
  bookmarks_api::mojom::PermanentFolderType GetPermanentFolderType(
      const bookmarks::BookmarkNode* node) const override;
  base::Uuid GetUuid(const bookmarks::BookmarkNode* node) override;
  bool IsSynced(const bookmarks::BookmarkNode* node) const override;
  bookmarks_api::BookmarkEventTranslator& GetEventTranslator() override;
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

  // BookmarkMergedSurfaceServiceObserver:
  void BookmarkMergedSurfaceServiceLoaded() override;
  void BookmarkMergedSurfaceServiceBeingDeleted() override;
  void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                         size_t index) override;
  void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) override;
  void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                         size_t old_index,
                         const BookmarkParentFolder& new_parent,
                         size_t new_index) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) override;
  void BookmarkAllUserNodesRemoved() override;
  void ExtensiveBookmarkChangesBeginning() override;
  void ExtensiveBookmarkChangesEnded() override;

 private:
  void Notify(std::vector<bookmarks_api::mojom::BookmarksEventPtr> events);

  const bookmarks::BookmarkNode* GetNodeForParentFolder(
      const BookmarkParentFolder& folder) const;

  raw_ptr<BookmarkMergedSurfaceService> service_;
  std::unique_ptr<bookmarks::BookmarkNode> synthetic_root_node_;
  bookmarks_api::BookmarkEventTranslator translator_{this};

  base::ObserverList<bookmarks_api::BookmarksViewObserver> observers_;
  base::ScopedObservation<BookmarkMergedSurfaceService,
                          BookmarkMergedSurfaceServiceObserver>
      service_observation_{this};

  std::vector<bookmarks_api::mojom::BookmarksEventPtr> queued_events_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_MERGED_SURFACE_VIEW_H_
