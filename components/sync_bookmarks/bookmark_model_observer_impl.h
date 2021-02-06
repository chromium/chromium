// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_IMPL_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_IMPL_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "url/gurl.h"

namespace syncer {
class UniquePosition;
}

namespace sync_bookmarks {

// Class for listening to local changes in the bookmark model and updating
// metadata in SyncedBookmarkTracker, such that ultimately the processor exposes
// those local changes to the sync engine.
class BookmarkModelObserverImpl : public bookmarks::BookmarkModelObserver {
 public:
  // |bookmark_tracker_| must not be null and must outlive this object.
  BookmarkModelObserverImpl(
      const base::RepeatingClosure& nudge_for_commit_closure,
      base::OnceClosure on_bookmark_model_being_deleted_closure,
      SyncedBookmarkTracker* bookmark_tracker);
  ~BookmarkModelObserverImpl() override;

  // BookmarkModelObserver:
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index) override;
  void OnWillRemoveBookmarks(bookmarks::BookmarkModel* model,
                             const bookmarks::BookmarkNode* parent,
                             size_t old_index,
                             const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void OnWillRemoveAllUserBookmarks(bookmarks::BookmarkModel* model) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void BookmarkMetaInfoChanged(bookmarks::BookmarkModel* model,
                               const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;

 private:
  syncer::UniquePosition ComputePosition(const bookmarks::BookmarkNode& parent,
                                         size_t index,
                                         const std::string& sync_id);

  // Process a modification of a local node and updates |bookmark_tracker_|
  // accordingly. No-op if the commit can be optimized away, i.e. if |specifics|
  // are identical to the previously-known specifics (in hashed form).
  void ProcessUpdate(const SyncedBookmarkTracker::Entity* entity,
                     const sync_pb::EntitySpecifics& specifics);

  // Processes the deletion of a bookmake node and updates the
  // |bookmark_tracker_| accordingly. If |node| is a bookmark, it gets marked
  // as deleted and that it requires a commit. If it's a folder, it recurses
  // over all children before processing the folder itself.
  void ProcessDelete(const bookmarks::BookmarkNode* parent,
                     const bookmarks::BookmarkNode* node);

  // Points to the tracker owned by the processor. It keeps the mapping between
  // bookmark nodes and corresponding sync server entities.
  SyncedBookmarkTracker* const bookmark_tracker_;

  // The callback used to inform the sync engine that there are local changes to
  // be committed.
  const base::RepeatingClosure nudge_for_commit_closure_;

  // The callback used to inform the processor that the bookmark is getting
  // deleted.
  base::OnceClosure on_bookmark_model_being_deleted_closure_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelObserverImpl);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_IMPL_H_
