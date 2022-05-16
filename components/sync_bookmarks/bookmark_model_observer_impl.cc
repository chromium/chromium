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
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"

namespace sync_bookmarks {

BookmarkModelObserverImpl::BookmarkModelObserverImpl(
    const base::RepeatingClosure& nudge_for_commit_closure,
    base::OnceClosure on_bookmark_model_being_deleted_closure,
    SyncedBookmarkTracker* bookmark_tracker)
    : bookmark_tracker_(bookmark_tracker),
      nudge_for_commit_closure_(nudge_for_commit_closure),
      on_bookmark_model_being_deleted_closure_(
          std::move(on_bookmark_model_being_deleted_closure)) {
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
  std::move(on_bookmark_model_being_deleted_closure_).Run();
}

void BookmarkModelObserverImpl::BookmarkNodeMoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  const bookmarks::BookmarkNode* node = new_parent->children()[new_index].get();

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!model->is_permanent_node(node));
  if (!model->client()->CanSyncNode(node)) {
    return;
  }
  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  DCHECK(entity);

  const std::string& sync_id = entity->metadata()->server_id();
  const base::Time modification_time = base::Time::Now();
  const syncer::UniquePosition unique_position =
      ComputePosition(*new_parent, new_index, sync_id);

  sync_pb::EntitySpecifics specifics =
      CreateSpecificsFromBookmarkNode(node, model, unique_position.ToProto(),
                                      /*force_favicon_load=*/true);

  bookmark_tracker_->Update(entity, entity->metadata()->server_version(),
                            modification_time, specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();
  bookmark_tracker_->CheckAllNodesTracked(model);
}

void BookmarkModelObserverImpl::BookmarkNodeAdded(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t index) {
  const bookmarks::BookmarkNode* node = parent->children()[index].get();
  if (!model->client()->CanSyncNode(node)) {
    return;
  }

  const SyncedBookmarkTrackerEntity* parent_entity =
      bookmark_tracker_->GetEntityForBookmarkNode(parent);
  DCHECK(parent_entity);

  const syncer::UniquePosition unique_position =
      ComputePosition(*parent, index, node->guid().AsLowercaseString());

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, model, unique_position.ToProto(), /*force_favicon_load=*/true);

  // It is possible that a created bookmark was restored after deletion and
  // the tombstone was not committed yet. In that case the existing entity
  // should be updated.
  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForGUID(node->guid());
  const base::Time creation_time = base::Time::Now();
  if (entity) {
    // If there is a tracked entity with the same client tag hash (effectively
    // the same bookmark GUID), it must be a tombstone. Otherwise it means
    // the bookmark model contains to bookmarks with the same GUID.
    DCHECK(!entity->bookmark_node()) << "Added bookmark with duplicate GUID";
    bookmark_tracker_->UndeleteTombstoneForBookmarkNode(entity, node);
    bookmark_tracker_->Update(entity, entity->metadata()->server_version(),
                              creation_time, specifics);
  } else {
    entity = bookmark_tracker_->Add(node, node->guid().AsLowercaseString(),
                                    syncer::kUncommittedVersion, creation_time,
                                    specifics);
  }

  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();

  // Do not check if all nodes are tracked because it's still possible that some
  // nodes are untracked, e.g. if current node has been just restored and its
  // children will be added soon.
}

void BookmarkModelObserverImpl::OnWillRemoveBookmarks(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node) {
  if (!model->client()->CanSyncNode(node)) {
    return;
  }
  bookmark_tracker_->CheckAllNodesTracked(model);
  ProcessDelete(node);
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  // All the work should have already been done in OnWillRemoveBookmarks.
  DCHECK(bookmark_tracker_->GetEntityForBookmarkNode(node) == nullptr);
  bookmark_tracker_->CheckAllNodesTracked(model);
}

void BookmarkModelObserverImpl::OnWillRemoveAllUserBookmarks(
    bookmarks::BookmarkModel* model) {
  bookmark_tracker_->CheckAllNodesTracked(model);
  const bookmarks::BookmarkNode* root_node = model->root_node();
  for (const auto& permanent_node : root_node->children()) {
    for (const auto& child : permanent_node->children()) {
      if (model->client()->CanSyncNode(child.get())) {
        ProcessDelete(child.get());
      }
    }
  }
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::BookmarkAllUserNodesRemoved(
    bookmarks::BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  // All the work should have already been done in OnWillRemoveAllUserBookmarks.
  bookmark_tracker_->CheckAllNodesTracked(model);
}

void BookmarkModelObserverImpl::BookmarkNodeChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  if (!model->client()->CanSyncNode(node)) {
    return;
  }

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!model->is_permanent_node(node));

  const SyncedBookmarkTrackerEntity* entity =
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

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, model, entity->metadata()->unique_position(),
      /*force_favicon_load=*/true);
  ProcessUpdate(entity, specifics);
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

  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  if (!entity) {
    // This should be practically unreachable but in theory it's possible that a
    // favicon changes *during* the creation of a bookmark (by another
    // observer). See analogous codepath in BookmarkNodeChanged().
    return;
  }

  const sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, model, entity->metadata()->unique_position(),
      /*force_favicon_load=*/false);

  // TODO(crbug.com/1094825): implement |base_specifics_hash| similar to
  // ClientTagBasedModelTypeProcessor.
  if (!entity->MatchesFaviconHash(specifics.bookmark().favicon())) {
    ProcessUpdate(entity, specifics);
    return;
  }

  // The favicon content didn't actually change, which means this event is
  // almost certainly the result of favicon loading having completed.
  if (entity->IsUnsynced()) {
    // Nudge for commit once favicon is loaded. This is needed in case when
    // unsynced entity was skipped while building commit requests (since favicon
    // wasn't loaded).
    nudge_for_commit_closure_.Run();
  }
}

void BookmarkModelObserverImpl::BookmarkNodeChildrenReordered(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  if (!model->client()->CanSyncNode(node)) {
    return;
  }

  // The given node's children got reordered. We need to reorder all the
  // corresponding sync node.
  // TODO(crbug.com/1321519): children reordering is used to move one bookmark
  // on Android, it should be either taken into account here or another bridge
  // interface should be provided. One approach would be something like:
  // 1. Find a subsequence of elements in the beginning of the vector that is
  //    already sorted.
  // 2. The same for the end.
  // 3. If the two overlap, adjust so they don't.
  // 4. Sort the middle, using Between (e.g. recursive implementation).
  syncer::UniquePosition position;
  for (const auto& child : node->children()) {
    const SyncedBookmarkTrackerEntity* entity =
        bookmark_tracker_->GetEntityForBookmarkNode(child.get());
    DCHECK(entity);

    const std::string& sync_id = entity->metadata()->server_id();
    const std::string suffix = syncer::GenerateSyncableBookmarkHash(
        bookmark_tracker_->model_type_state().cache_guid(), sync_id);
    const base::Time modification_time = base::Time::Now();

    position = (child == node->children().front())
                   ? syncer::UniquePosition::InitialPosition(suffix)
                   : syncer::UniquePosition::After(position, suffix);

    const sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
        child.get(), model, position.ToProto(), /*force_favicon_load=*/true);

    bookmark_tracker_->Update(entity, entity->metadata()->server_version(),
                              modification_time, specifics);
    // Mark the entity that it needs to be committed.
    bookmark_tracker_->IncrementSequenceNumber(entity);
  }
  nudge_for_commit_closure_.Run();
}

syncer::UniquePosition BookmarkModelObserverImpl::ComputePosition(
    const bookmarks::BookmarkNode& parent,
    size_t index,
    const std::string& sync_id) {
  const std::string& suffix = syncer::GenerateSyncableBookmarkHash(
      bookmark_tracker_->model_type_state().cache_guid(), sync_id);
  DCHECK(!parent.children().empty());
  const SyncedBookmarkTrackerEntity* predecessor_entity = nullptr;
  const SyncedBookmarkTrackerEntity* successor_entity = nullptr;

  // Look for the first tracked predecessor.
  for (auto i = parent.children().crend() - index;
       i != parent.children().crend(); ++i) {
    const bookmarks::BookmarkNode* predecessor_node = i->get();
    predecessor_entity =
        bookmark_tracker_->GetEntityForBookmarkNode(predecessor_node);
    if (predecessor_entity) {
      break;
    }
  }

  // Look for the first tracked successor.
  for (auto i = parent.children().cbegin() + index + 1;
       i != parent.children().cend(); ++i) {
    const bookmarks::BookmarkNode* successor_node = i->get();
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

void BookmarkModelObserverImpl::ProcessUpdate(
    const SyncedBookmarkTrackerEntity* entity,
    const sync_pb::EntitySpecifics& specifics) {
  DCHECK(entity);

  // Data should be committed to the server only if there is an actual change,
  // determined here by comparing hashes.
  if (entity->MatchesSpecificsHash(specifics)) {
    // Specifics haven't actually changed, so the local change can be ignored.
    //
    // This is an opportunity to populate the favicon hash in sync metadata if
    // it hasn't been populated yet. This is needed because the proto field that
    // stores favicon hashes was introduced late. The fact that hashed specifics
    // match implies that the favicon (which is part of specifics) must also
    // match, hence the proto field can be safely populated.
    bookmark_tracker_->PopulateFaviconHashIfUnset(
        entity, specifics.bookmark().favicon());
    return;
  }

  bookmark_tracker_->Update(entity, entity->metadata()->server_version(),
                            /*modification_time=*/base::Time::Now(), specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::ProcessDelete(
    const bookmarks::BookmarkNode* node) {
  // If not a leaf node, process all children first.
  for (const auto& child : node->children()) {
    ProcessDelete(child.get());
  }
  // Process the current node.
  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  // Shouldn't try to delete untracked entities.
  DCHECK(entity);
  // If the entity hasn't been committed and doesn't have an inflight commit
  // request, simply remove it from the tracker.
  if (entity->metadata()->server_version() == syncer::kUncommittedVersion &&
      !entity->commit_may_have_started()) {
    bookmark_tracker_->Remove(entity);
    return;
  }
  bookmark_tracker_->MarkDeleted(entity);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(entity);
}

}  // namespace sync_bookmarks
