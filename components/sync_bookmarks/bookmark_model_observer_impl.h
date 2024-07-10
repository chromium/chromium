// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_IMPL_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_IMPL_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "url/gurl.h"

namespace sync_pb {
class EntitySpecifics;
}

namespace syncer {
class UniquePosition;
}

namespace sync_bookmarks {

class BookmarkModelView;

// Class for listening to local changes in the bookmark model and updating
// metadata in SyncedBookmarkTracker, such that ultimately the processor exposes
// those local changes to the sync engine.
class BookmarkModelObserverImpl : public bookmarks::BookmarkModelObserver {
 public:
  // `bookmark_model` and `bookmark_tracker` must not be null and must outlive
  // this object. Note that this class doesn't self register as observer.
  BookmarkModelObserverImpl(
      BookmarkModelView* bookmark_model,
      const base::RepeatingClosure& nudge_for_commit_closure,
      base::OnceClosure on_bookmark_model_being_deleted_closure,
      SyncedBookmarkTracker* bookmark_tracker);

  BookmarkModelObserverImpl(const BookmarkModelObserverImpl&) = delete;
  BookmarkModelObserverImpl& operator=(const BookmarkModelObserverImpl&) =
      delete;

  ~BookmarkModelObserverImpl() override;

  // BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void OnWillRemoveBookmarks(const bookmarks::BookmarkNode* parent,
                             size_t old_index,
                             const bookmarks::BookmarkNode* node,
                             const base::Location& location) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void OnWillRemoveAllUserBookmarks(const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkMetaInfoChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;

 private:
  syncer::UniquePosition ComputePosition(const bookmarks::BookmarkNode& parent,
                                         size_t index) const;

  // Process a modification of a local node and updates `bookmark_tracker_`
  // accordingly. No-op if the commit can be optimized away, i.e. if `specifics`
  // are identical to the previously-known specifics (in hashed form).
  void ProcessUpdate(const SyncedBookmarkTrackerEntity* entity,
                     const sync_pb::EntitySpecifics& specifics);

  // Processes the deletion of a bookmake node and updates the
  // `bookmark_tracker_` accordingly. If `node` is a bookmark, it gets marked
  // as deleted and that it requires a commit. If it's a folder, it recurses
  // over all children before processing the folder itself. `location`
  // represents the origin of the deletion, i.e. which specific codepath was
  // responsible for deleting `node`.
  void ProcessDelete(const bookmarks::BookmarkNode* node,
                     const base::Location& location);

  // Recursive function to deal for the case where a moved folder becomes
  // syncable, which requires that all descendants are also newly tracked.
  void ProcessMovedDescendentsAsBookmarkNodeAddedRecursive(
      const bookmarks::BookmarkNode* node);

  // Returns current unique_position from sync metadata for the tracked `node`.
  syncer::UniquePosition GetUniquePositionForNode(
      const bookmarks::BookmarkNode* node) const;

  // Updates the unique position in sync metadata for the tracked `node` and
  // returns the new position. A new position is generated based on the left and
  // right node's positions. At least one of `prev` and `next` must be valid.
  syncer::UniquePosition UpdateUniquePositionForNode(
      const bookmarks::BookmarkNode* node,
      const syncer::UniquePosition& prev,
      const syncer::UniquePosition& next);

  // Updates unique positions for all children from `parent` starting from
  // `start_index` (must not be 0).
  void UpdateAllUniquePositionsStartingAt(const bookmarks::BookmarkNode* parent,
                                          size_t start_index);

  const raw_ptr<BookmarkModelView> bookmark_model_;

  // Points to the tracker owned by the processor. It keeps the mapping between
  // bookmark nodes and corresponding sync server entities.
  const raw_ptr<SyncedBookmarkTracker> bookmark_tracker_;

  // The callback used to inform the sync engine that there are local changes to
  // be committed.
  const base::RepeatingClosure nudge_for_commit_closure_;

  // The callback used to inform the processor that the bookmark is getting
  // deleted.
  base::OnceClosure on_bookmark_model_being_deleted_closure_;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_IMPL_H_
