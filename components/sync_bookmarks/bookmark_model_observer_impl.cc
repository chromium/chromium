// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_observer_impl.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"

namespace sync_bookmarks {

BookmarkModelObserverImpl::BookmarkModelObserverImpl(
    const base::RepeatingClosure& nudge_for_commit_closure,
    SyncedBookmarkTracker* bookmark_tracker)
    : bookmark_tracker_(bookmark_tracker),
      nudge_for_commit_closure_(nudge_for_commit_closure) {
  DCHECK(bookmark_tracker_);
}

BookmarkModelObserverImpl::~BookmarkModelObserverImpl() = default;

void BookmarkModelObserverImpl::BookmarkModelLoaded(
    bookmarks::BookmarkModel* model,
    bool ids_reassigned) {
  // This class isn't responsible for any loading-related logic.
}

void BookmarkModelObserverImpl::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* model) {
  // This class isn't responsible for any deletion-related logic.
}

void BookmarkModelObserverImpl::BookmarkNodeMoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* old_parent,
    int old_index,
    const bookmarks::BookmarkNode* new_parent,
    int new_index) {
  const bookmarks::BookmarkNode* node = new_parent->GetChild(new_index);

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!model->is_permanent_node(node));
  if (!model->client()->CanSyncNode(node)) {
    return;
  }
  const SyncedBookmarkTracker::Entity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  DCHECK(entity);

  const std::string& sync_id = entity->metadata()->server_id();
  const base::Time modification_time = base::Time::Now();

  const sync_pb::UniquePosition unique_position =
      ComputePosition(*new_parent, new_index, sync_id).ToProto();

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, model, /*force_favicon_load=*/true);

  bookmark_tracker_->Update(sync_id, entity->metadata()->server_version(),
                            modification_time, unique_position, specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(sync_id);
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::BookmarkNodeAdded(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int index) {
  const bookmarks::BookmarkNode* node = parent->GetChild(index);
  if (!model->client()->CanSyncNode(node)) {
    return;
  }

  const SyncedBookmarkTracker::Entity* parent_entity =
      bookmark_tracker_->GetEntityForBookmarkNode(parent);
  if (!parent_entity) {
    DLOG(WARNING) << "Bookmark parent lookup failed";
    return;
  }
  // Similar to the diectory implementation here:
  // https://cs.chromium.org/chromium/src/components/sync/syncable/mutable_entry.cc?l=237&gsn=CreateEntryKernel
  // Assign a temp server id for the entity. Will be overriden by the actual
  // server id upon receiving commit response.
  const std::string sync_id = base::GenerateGUID();
  const int64_t server_version = syncer::kUncommittedVersion;
  const base::Time creation_time = base::Time::Now();
  const sync_pb::UniquePosition unique_position =
      ComputePosition(*parent, index, sync_id).ToProto();

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, model, /*force_favicon_load=*/true);

  bookmark_tracker_->Add(sync_id, node, server_version, creation_time,
                         unique_position, specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(sync_id);
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::OnWillRemoveBookmarks(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int old_index,
    const bookmarks::BookmarkNode* node) {
  if (!model->client()->CanSyncNode(node)) {
    return;
  }
  ProcessDelete(parent, node);
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  // All the work should have already been done in OnWillRemoveBookmarks.
  DCHECK(bookmark_tracker_->GetEntityForBookmarkNode(node) == nullptr);
}

void BookmarkModelObserverImpl::OnWillRemoveAllUserBookmarks(
    bookmarks::BookmarkModel* model) {
  const bookmarks::BookmarkNode* root_node = model->root_node();
  for (int i = 0; i < root_node->child_count(); ++i) {
    const bookmarks::BookmarkNode* permanent_node = root_node->GetChild(i);
    for (int j = permanent_node->child_count() - 1; j >= 0; --j) {
      if (!model->client()->CanSyncNode(permanent_node->GetChild(j))) {
        continue;
      }
      ProcessDelete(permanent_node, permanent_node->GetChild(j));
    }
  }
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::BookmarkAllUserNodesRemoved(
    bookmarks::BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  // All the work should have already been done in OnWillRemoveAllUserBookmarks.
}

void BookmarkModelObserverImpl::BookmarkNodeChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  if (!model->client()->CanSyncNode(node)) {
    return;
  }

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!model->is_permanent_node(node));

  const SyncedBookmarkTracker::Entity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  if (!entity) {
    // If the node hasn't been added to the tracker yet, we do nothing. It will
    // be added later. It's how BookmarkModel currently notifies observers, if
    // further changes are triggered *during* observer notification. Consider
    // the following scenario:
    // 1. New bookmark added.
    // 2. BookmarkModel notifies all the observers about the new node.
    // 3. One observer A get's notified before us.
    // 4. Observer A decided to update the bookmark node.
    // 5. BookmarkModel notifies all observers about the update.
    // 6. We received the notification about the update before the creation.
    // 7. We will get the notification about the addition later and then we can
    //    start tracking the node.
    return;
  }
  const base::Time modification_time = base::Time::Now();
  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, model, /*force_favicon_load=*/true);
  // TODO(crbug.com/516866): The below CHECKs are added to debug some crashes.
  // Should be removed after figuring out the reason for the crash.
  CHECK_EQ(entity, bookmark_tracker_->GetEntityForBookmarkNode(node));
  if (entity->MatchesSpecificsHash(specifics)) {
    // We should push data to the server only if there is an actual change in
    // the data. We could hit this code path without having actual changes
    // (e.g.upon a favicon load).
    return;
  }
  const std::string& sync_id = entity->metadata()->server_id();
  bookmark_tracker_->Update(sync_id, entity->metadata()->server_version(),
                            modification_time,
                            entity->metadata()->unique_position(), specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(sync_id);
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::BookmarkMetaInfoChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  BookmarkNodeChanged(model, node);
}

void BookmarkModelObserverImpl::BookmarkNodeFaviconChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  if (!model->client()->CanSyncNode(node)) {
    return;
  }

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!model->is_permanent_node(node));

  // Ignore favicons that are being loaded.
  if (!node->is_favicon_loaded()) {
    // Subtle way to trigger a load of the favicon. This very same function will
    // be notified when the favicon gets loaded (read from HistoryService and
    // cached in RAM within BookmarkModel).
    model->GetFavicon(node);
    return;
  }
  BookmarkNodeChanged(model, node);
}

void BookmarkModelObserverImpl::BookmarkNodeChildrenReordered(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  if (!model->client()->CanSyncNode(node)) {
    return;
  }

  // The given node's children got reordered. We need to reorder all the
  // corresponding sync node.

  // TODO(crbug/com/516866): Make sure that single-move case doesn't produce
  // unnecessary updates. One approach would be something like:
  // 1. Find a subsequence of elements in the beginning of the vector that is
  //    already sorted.
  // 2. The same for the end.
  // 3. If the two overlap, adjust so they don't.
  // 4. Sort the middle, using Between (e.g. recursive implementation).

  syncer::UniquePosition position;
  syncer::UniquePosition previous_position;
  for (int i = 0; i < node->child_count(); ++i) {
    const bookmarks::BookmarkNode* child = node->GetChild(i);

    const SyncedBookmarkTracker::Entity* entity =
        bookmark_tracker_->GetEntityForBookmarkNode(child);
    DCHECK(entity);

    const std::string& sync_id = entity->metadata()->server_id();
    const std::string suffix = syncer::GenerateSyncableBookmarkHash(
        bookmark_tracker_->model_type_state().cache_guid(), sync_id);
    const base::Time modification_time = base::Time::Now();

    if (i == 0) {
      position = syncer::UniquePosition::InitialPosition(suffix);
    } else {
      position = syncer::UniquePosition::After(previous_position, suffix);
    }

    previous_position = position;

    const sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
        node, model, /*force_favicon_load=*/true);

    bookmark_tracker_->Update(sync_id, entity->metadata()->server_version(),
                              modification_time, position.ToProto(), specifics);
    // Mark the entity that it needs to be committed.
    bookmark_tracker_->IncrementSequenceNumber(sync_id);
  }
  nudge_for_commit_closure_.Run();
}

syncer::UniquePosition BookmarkModelObserverImpl::ComputePosition(
    const bookmarks::BookmarkNode& parent,
    int index,
    const std::string& sync_id) {
  const std::string& suffix = syncer::GenerateSyncableBookmarkHash(
      bookmark_tracker_->model_type_state().cache_guid(), sync_id);
  DCHECK_NE(0, parent.child_count());
  const SyncedBookmarkTracker::Entity* predecessor_entity = nullptr;
  const SyncedBookmarkTracker::Entity* successor_entity = nullptr;

  // Look for the first tracked predecessor.
  for (int i = index - 1; i >= 0; i--) {
    const bookmarks::BookmarkNode* predecessor_node = parent.GetChild(i);
    predecessor_entity =
        bookmark_tracker_->GetEntityForBookmarkNode(predecessor_node);
    if (predecessor_entity) {
      break;
    }
  }

  // Look for the first tracked successor.
  for (int i = index + 1; i < parent.child_count(); i++) {
    const bookmarks::BookmarkNode* successor_node = parent.GetChild(i);
    successor_entity =
        bookmark_tracker_->GetEntityForBookmarkNode(successor_node);
    if (successor_entity) {
      break;
    }
  }

  if (!predecessor_entity && !successor_entity) {
    // No tracked siblings.
    return syncer::UniquePosition::InitialPosition(suffix);
  }

  if (!predecessor_entity && successor_entity) {
    // No predecessor, insert before the successor.
    return syncer::UniquePosition::Before(
        syncer::UniquePosition::FromProto(
            successor_entity->metadata()->unique_position()),
        suffix);
  }

  if (predecessor_entity && !successor_entity) {
    // No successor, insert after the predecessor
    return syncer::UniquePosition::After(
        syncer::UniquePosition::FromProto(
            predecessor_entity->metadata()->unique_position()),
        suffix);
  }

  // Both predecessor and successor, insert in the middle.
  return syncer::UniquePosition::Between(
      syncer::UniquePosition::FromProto(
          predecessor_entity->metadata()->unique_position()),
      syncer::UniquePosition::FromProto(
          successor_entity->metadata()->unique_position()),
      suffix);
}

void BookmarkModelObserverImpl::ProcessDelete(
    const bookmarks::BookmarkNode* parent,
    const bookmarks::BookmarkNode* node) {
  // If not a leaf node, process all children first.
  for (int i = 0; i < node->child_count(); ++i) {
    const bookmarks::BookmarkNode* child = node->GetChild(i);
    ProcessDelete(node, child);
  }
  // Process the current node.
  const SyncedBookmarkTracker::Entity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  // Shouldn't try to delete untracked entities.
  DCHECK(entity);
  const std::string& sync_id = entity->metadata()->server_id();
  bookmark_tracker_->MarkDeleted(sync_id);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(sync_id);
}

}  // namespace sync_bookmarks
