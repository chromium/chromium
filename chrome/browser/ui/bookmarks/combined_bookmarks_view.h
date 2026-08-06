// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_COMBINED_BOOKMARKS_VIEW_H_
#define CHROME_BROWSER_UI_BOOKMARKS_COMBINED_BOOKMARKS_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/browser_apis/bookmarks/bookmark_event_translator.h"
#include "components/browser_apis/bookmarks/bookmark_uuid_mapper.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"
#include "components/browser_apis/bookmarks/bookmarks_view.h"
#include "url/gurl.h"

namespace bookmarks {
class ManagedBookmarkService;
}

// Implements BookmarksView directly on top of BookmarkModel (which manages both
// local and account bookmark nodes) and ManagedBookmarkService.
//
// NOTE on special behavior, UUID collision avoidance, and Managed Bookmarks:
// 1. Unlike BookmarkMergedSurfaceView (which merges local and account bookmarks
//    into a single default parent folder per permanent folder type),
//    CombinedBookmarksView returns all permanent nodes (both the local and
//    account bookmark trees) as direct children of the synthetic root node.
// 2. Because BookmarkModel reuses the exact same UUID constants (e.g.
//    kBookmarkBarNodeUuid) for both account and local permanent folders,
//    CombinedBookmarksView maintains a bidirectional mapping between the
//    permanent node ID (int64_t) and unique random V4 UUIDs.
// 3. Managed Bookmarks: Enterprise managed bookmarks are provided via
//    ManagedBookmarkService and attached directly to
//    BookmarkModel::root_node(). Because managed bookmarks are authoritative
//    enterprise policies, they are never synced across user accounts or
//    duplicated into account storage. model_->root_node()->children() already
//    contains the single canonical managed permanent node. In the WebUI,
//    managed bookmarks reside under local/device storage and can be
//    collapsed/expanded independently without affecting sync or model state.
class CombinedBookmarksView : public bookmarks_api::BookmarksView,
                              public bookmarks::BookmarkModelObserver {
 public:
  CombinedBookmarksView(
      bookmarks::BookmarkModel* model,
      bookmarks::ManagedBookmarkService* managed_bookmark_service);
  ~CombinedBookmarksView() override;

  CombinedBookmarksView(const CombinedBookmarksView&) = delete;
  CombinedBookmarksView& operator=(const CombinedBookmarksView&) = delete;

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
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void OnWillRemoveAllUserBookmarks(const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void OnWillReorderBookmarkNode(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkPermanentNodeVisibilityChanged(
      const bookmarks::BookmarkPermanentNode* node) override;
  void ExtensiveBookmarkChangesBeginning() override;
  void ExtensiveBookmarkChangesEnded() override;

 private:
  void Notify(std::vector<bookmarks_api::mojom::BookmarksEventPtr> events);
  void RegisterAccountNodeOverrides();

  raw_ptr<bookmarks::BookmarkModel> model_;
  raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
  std::unique_ptr<bookmarks::BookmarkNode> synthetic_root_node_;
  bookmarks_api::BookmarkUuidMapper uuid_mapper_;
  bookmarks_api::BookmarkEventTranslator translator_{this};

  base::ObserverList<bookmarks_api::BookmarksViewObserver> observers_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      model_observation_{this};

  std::vector<bookmarks_api::mojom::BookmarksEventPtr> queued_events_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_COMBINED_BOOKMARKS_VIEW_H_
