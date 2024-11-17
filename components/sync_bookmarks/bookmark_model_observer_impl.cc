// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_observer_impl.h"

#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace sync_bookmarks {

namespace {

// A helper wrapper used to compare UniquePosition with positions before the
// first and after the last elements.
class UniquePositionWrapper {
 public:
  static UniquePositionWrapper Min() {
    return UniquePositionWrapper(MinUniquePosition{});
  }

  static UniquePositionWrapper Max() {
    return UniquePositionWrapper(MaxUniquePosition{});
  }

  // |unique_position| must be valid.
  static UniquePositionWrapper ForValidUniquePosition(
      syncer::UniquePosition unique_position) {
    DCHECK(unique_position.IsValid());
    return UniquePositionWrapper(std::move(unique_position));
  }

  UniquePositionWrapper(UniquePositionWrapper&&) = default;
  UniquePositionWrapper& operator=(UniquePositionWrapper&&) = default;

  // Returns valid UniquePosition or invalid one for Min() and Max().
  const syncer::UniquePosition& GetUniquePosition() const {
    static const base::NoDestructor<syncer::UniquePosition>
        kEmptyUniquePosition;
    if (HoldsUniquePosition()) {
      return absl::get<syncer::UniquePosition>(value_);
    }
    return *kEmptyUniquePosition;
  }

  bool LessThan(const UniquePositionWrapper& other) const {
    if (value_.index() != other.value_.index()) {
      return value_.index() < other.value_.index();
    }
    if (!HoldsUniquePosition()) {
      // Both arguments are MinUniquePosition or MaxUniquePosition, in both
      // cases they are equal.
      return false;
    }
    return GetUniquePosition().LessThan(other.GetUniquePosition());
  }

 private:
  struct MinUniquePosition {};
  struct MaxUniquePosition {};

  explicit UniquePositionWrapper(absl::variant<MinUniquePosition,
                                               syncer::UniquePosition,
                                               MaxUniquePosition> value)
      : value_(std::move(value)) {}

  bool HoldsUniquePosition() const {
    return absl::holds_alternative<syncer::UniquePosition>(value_);
  }

  // The order is used to compare positions.
  absl::variant<MinUniquePosition, syncer::UniquePosition, MaxUniquePosition>
      value_;
};

}  // namespace

BookmarkModelObserverImpl::BookmarkModelObserverImpl(
    BookmarkModelView* bookmark_model,
    const base::RepeatingClosure& nudge_for_commit_closure,
    base::OnceClosure on_bookmark_model_being_deleted_closure,
    SyncedBookmarkTracker* bookmark_tracker)
    : bookmark_model_(bookmark_model),
      bookmark_tracker_(bookmark_tracker),
      nudge_for_commit_closure_(nudge_for_commit_closure),
      on_bookmark_model_being_deleted_closure_(
          std::move(on_bookmark_model_being_deleted_closure)) {
  CHECK(bookmark_model_);
  CHECK(bookmark_tracker_);
}

BookmarkModelObserverImpl::~BookmarkModelObserverImpl() = default;

void BookmarkModelObserverImpl::BookmarkModelLoaded(bool ids_reassigned) {
  // This class isn't responsible for any loading-related logic.
}

void BookmarkModelObserverImpl::BookmarkModelBeingDeleted() {
  std::move(on_bookmark_model_being_deleted_closure_).Run();
}

void BookmarkModelObserverImpl::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  const bookmarks::BookmarkNode* node = new_parent->children()[new_index].get();

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!bookmark_model_->is_permanent_node(node));

  // Handle moves that make a node newly syncable.
  if (!bookmark_model_->IsNodeSyncable(old_parent) &&
      bookmark_model_->IsNodeSyncable(new_parent)) {
    BookmarkNodeAdded(new_parent, new_index, false /*unused*/);
    // If the moved node is a folder, the descendants also became syncable and
    // must be reported as well.
    ProcessMovedDescendentsAsBookmarkNodeAddedRecursive(node);
    return;
  }

  // Handle moves that make a node non-syncable.
  if (bookmark_model_->IsNodeSyncable(old_parent) &&
      !bookmark_model_->IsNodeSyncable(new_parent)) {
    // OnWillRemoveBookmarks() cannot be invoked here because |node| is already
    // moved and unsyncable, whereas OnWillRemoveBookmarks() assumes the change
    // hasn't happened yet.
    ProcessDelete(node, FROM_HERE);
    nudge_for_commit_closure_.Run();
    bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
    return;
  }

  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!bookmark_model_->IsNodeSyncable(node)) {
    return;
  }

  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  CHECK(entity);

  const base::Time modification_time = base::Time::Now();
  const syncer::UniquePosition unique_position =
      ComputePosition(*new_parent, new_index);

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, bookmark_model_, unique_position.ToProto(),
      /*force_favicon_load=*/true);

  bookmark_tracker_->Update(entity, entity->metadata().server_version(),
                            modification_time, specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();
  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
}

void BookmarkModelObserverImpl::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  const bookmarks::BookmarkNode* node = parent->children()[index].get();

  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!bookmark_model_->IsNodeSyncable(node)) {
    return;
  }

  const SyncedBookmarkTrackerEntity* parent_entity =
      bookmark_tracker_->GetEntityForBookmarkNode(parent);
  DCHECK(parent_entity);

  const syncer::UniquePosition unique_position =
      ComputePosition(*parent, index);

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, bookmark_model_, unique_position.ToProto(),
      /*force_favicon_load=*/true);

  // It is possible that a created bookmark was restored after deletion and
  // the tombstone was not committed yet. In that case the existing entity
  // should be updated.
  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForUuid(node->uuid());
  const base::Time creation_time = base::Time::Now();
  if (entity) {
    // If there is a tracked entity with the same client tag hash (effectively
    // the same bookmark GUID), it must be a tombstone. Otherwise it means
    // the bookmark model contains to bookmarks with the same GUID.
    DCHECK(!entity->bookmark_node()) << "Added bookmark with duplicate GUID";
    bookmark_tracker_->UndeleteTombstoneForBookmarkNode(entity, node);
    bookmark_tracker_->Update(entity, entity->metadata().server_version(),
                              creation_time, specifics);
  } else {
    entity = bookmark_tracker_->Add(node, node->uuid().AsLowercaseString(),
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
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const base::Location& location) {
  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!bookmark_model_->IsNodeSyncable(node)) {
    return;
  }
  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
  ProcessDelete(node, location);
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  // All the work should have already been done in OnWillRemoveBookmarks.
  DCHECK(bookmark_tracker_->GetEntityForBookmarkNode(node) == nullptr);
  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
}

void BookmarkModelObserverImpl::OnWillRemoveAllUserBookmarks(
    const base::Location& location) {
  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
  const bookmarks::BookmarkNode* root_node = bookmark_model_->root_node();
  for (const auto& permanent_node : root_node->children()) {
    for (const auto& child : permanent_node->children()) {
      if (bookmark_model_->IsNodeSyncable(child.get())) {
        ProcessDelete(child.get(), location);
      }
    }
  }
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  // All the work should have already been done in OnWillRemoveAllUserBookmarks.
  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
}

void BookmarkModelObserverImpl::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!bookmark_model_->IsNodeSyncable(node)) {
    return;
  }

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!bookmark_model_->is_permanent_node(node));

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
      node, bookmark_model_, entity->metadata().unique_position(),
      /*force_favicon_load=*/true);
  ProcessUpdate(entity, specifics);
}

void BookmarkModelObserverImpl::BookmarkMetaInfoChanged(
    const bookmarks::BookmarkNode* node) {
  BookmarkNodeChanged(node);
}

void BookmarkModelObserverImpl::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {
  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!bookmark_model_->IsNodeSyncable(node)) {
    return;
  }

  // We shouldn't see changes to the top-level nodes.
  DCHECK(!bookmark_model_->is_permanent_node(node));

  // Ignore favicons that are being loaded.
  if (!node->is_favicon_loaded()) {
    // Subtle way to trigger a load of the favicon. This very same function will
    // be notified when the favicon gets loaded (read from HistoryService and
    // cached in RAM within BookmarkModel).
    bookmark_model_->GetFavicon(node);
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
      node, bookmark_model_, entity->metadata().unique_position(),
      /*force_favicon_load=*/false);

  // TODO(crbug.com/40699726): implement |base_specifics_hash| similar to
  // ClientTagBasedDataTypeProcessor.
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
    const bookmarks::BookmarkNode* node) {
  // Ignore changes to non-syncable nodes (e.g. managed nodes).
  if (!bookmark_model_->IsNodeSyncable(node)) {
    return;
  }

  if (node->children().size() <= 1) {
    // There is no real change in the order of |node|'s children.
    return;
  }

  // The given node's children got reordered, all the corresponding sync nodes
  // need to be reordered. The code is optimized to detect move of only one
  // bookmark (which is the case on Android platform). There are 2 main cases:
  // a bookmark moved to left or to right. Moving a bookmark to the first and
  // last positions are two more special cases. The algorithm iterates over each
  // bookmark and compares it to the left and right nodes to determine whether
  // it's ordered or not.
  //
  // Each digit below represents bookmark's original position.
  //
  // Moving a bookmark to the left: 0123456 -> 0612345.
  // When processing '6', its unique position is greater than both left and
  // right nodes.
  //
  // Moving a bookmark to the right: 0123456 -> 0234516.
  // When processing '1', its unique position is less than both left and right
  // nodes.
  //
  // Note that in both cases left node is less than right node. This condition
  // is checked when iterating over bookmarks and if it's violated, the
  // algorithm falls back to generating positions for all the following nodes.
  //
  // For example, if two nodes are moved to one place: 0123456 -> 0156234 (nodes
  // '5' and '6' are moved together). In this case, 0156 will remain and when
  // processing '2', algorithm will fall back to generating unique positions for
  // nodes '2', '3' and '4'. It will be detected by comparing the next node '3'
  // with the previous '6'.

  // Store |cur| outside of the loop to prevent parsing UniquePosition twice.
  UniquePositionWrapper cur = UniquePositionWrapper::ForValidUniquePosition(
      GetUniquePositionForNode(node->children().front().get()));
  UniquePositionWrapper prev = UniquePositionWrapper::Min();
  for (size_t current_index = 0; current_index < node->children().size();
       ++current_index) {
    UniquePositionWrapper next = UniquePositionWrapper::Max();
    if (current_index + 1 < node->children().size()) {
      next = UniquePositionWrapper::ForValidUniquePosition(
          GetUniquePositionForNode(node->children()[current_index + 1].get()));
    }

    // |prev| is the last ordered node. Compare |cur| and |next| with it to
    // decide whether current node needs to be updated. Consider the following
    // cases: 0. |prev| < |cur| < |next|: all elements are ordered.
    // 1. |cur| < |prev| < |next|: update current node and put it between |prev|
    //                             and |next|.
    // 2. |cur| < |next| < |prev|: both |cur| and |next| are out of order, fall
    //                             back to simple approach.
    // 3. |next| < |cur| < |prev|: same as #2.
    // 4. |prev| < |next| < |cur|: update current node and put it between |prev|
    //                             and |next|.
    // 5. |next| < |prev| < |cur|: consider current node ordered, |next| will be
    //                             updated on the next step.
    //
    // In the following code only cases where current node needs to be updated
    // are considered (#0 and #5 are omitted because there is nothing to do).

    bool update_current_position = false;
    if (cur.LessThan(prev)) {
      // |cur| < |prev|, cases #1, #2 and #3.
      if (next.LessThan(prev)) {
        // There are two consecutive nodes which both are out of order (#2, #3):
        // |prev| > |cur| and |prev| > |next|.
        // It means that more than one bookmark has been reordered, fall back to
        // generating unique positions for all the remaining children.
        //
        // |current_index| is always not 0 because |prev| cannot be
        // UniquePositionWrapper::Min() if |next| < |prev|.
        DCHECK_GT(current_index, 0u);
        UpdateAllUniquePositionsStartingAt(node, current_index);
        break;
      }
      update_current_position = true;
    } else if (next.LessThan(cur) && prev.LessThan(next)) {
      // |prev| < |next| < |cur| (case #4).
      update_current_position = true;
    }

    if (update_current_position) {
      cur = UniquePositionWrapper::ForValidUniquePosition(
          UpdateUniquePositionForNode(node->children()[current_index].get(),
                                      prev.GetUniquePosition(),
                                      next.GetUniquePosition()));
    }

    DCHECK(prev.LessThan(cur));
    prev = std::move(cur);
    cur = std::move(next);
  }

  nudge_for_commit_closure_.Run();
}

syncer::UniquePosition BookmarkModelObserverImpl::ComputePosition(
    const bookmarks::BookmarkNode& parent,
    size_t index) const {
  CHECK_LT(index, parent.children().size());

  const bookmarks::BookmarkNode* node = parent.children()[index].get();
  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::GenerateSuffix(
          SyncedBookmarkTracker::GetClientTagHashFromUuid(node->uuid()));

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
            successor_entity->metadata().unique_position()),
        suffix);
  }

  if (predecessor_entity && !successor_entity) {
    // No successor, insert after the predecessor
    return syncer::UniquePosition::After(
        syncer::UniquePosition::FromProto(
            predecessor_entity->metadata().unique_position()),
        suffix);
  }

  // Both predecessor and successor, insert in the middle.
  return syncer::UniquePosition::Between(
      syncer::UniquePosition::FromProto(
          predecessor_entity->metadata().unique_position()),
      syncer::UniquePosition::FromProto(
          successor_entity->metadata().unique_position()),
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
    return;
  }

  bookmark_tracker_->Update(entity, entity->metadata().server_version(),
                            /*modification_time=*/base::Time::Now(), specifics);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(entity);
  nudge_for_commit_closure_.Run();
}

void BookmarkModelObserverImpl::ProcessDelete(
    const bookmarks::BookmarkNode* node,
    const base::Location& location) {
  // If not a leaf node, process all children first.
  for (const auto& child : node->children()) {
    ProcessDelete(child.get(), location);
  }
  // Process the current node.
  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  // Shouldn't try to delete untracked entities.
  DCHECK(entity);
  // If the entity hasn't been committed and doesn't have an inflight commit
  // request, simply remove it from the tracker.
  if (entity->metadata().server_version() == syncer::kUncommittedVersion &&
      !entity->commit_may_have_started()) {
    bookmark_tracker_->Remove(entity);
    return;
  }
  bookmark_tracker_->MarkDeleted(entity, location);
  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(entity);
}

void BookmarkModelObserverImpl::
    ProcessMovedDescendentsAsBookmarkNodeAddedRecursive(
        const bookmarks::BookmarkNode* node) {
  CHECK(node);
  for (size_t index = 0; index < node->children().size(); ++index) {
    BookmarkNodeAdded(node, index, false /*unused*/);
    ProcessMovedDescendentsAsBookmarkNodeAddedRecursive(
        node->children()[index].get());
  }
}

syncer::UniquePosition BookmarkModelObserverImpl::GetUniquePositionForNode(
    const bookmarks::BookmarkNode* node) const {
  DCHECK(bookmark_tracker_);
  DCHECK(node);
  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  DCHECK(entity);
  return syncer::UniquePosition::FromProto(
      entity->metadata().unique_position());
}

syncer::UniquePosition BookmarkModelObserverImpl::UpdateUniquePositionForNode(
    const bookmarks::BookmarkNode* node,
    const syncer::UniquePosition& prev,
    const syncer::UniquePosition& next) {
  CHECK(bookmark_tracker_);
  CHECK(node);

  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  CHECK(entity);
  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::GenerateSuffix(entity->GetClientTagHash());
  const base::Time modification_time = base::Time::Now();

  syncer::UniquePosition new_unique_position;
  if (prev.IsValid() && next.IsValid()) {
    new_unique_position = syncer::UniquePosition::Between(prev, next, suffix);
  } else if (prev.IsValid()) {
    new_unique_position = syncer::UniquePosition::After(prev, suffix);
  } else {
    new_unique_position = syncer::UniquePosition::Before(next, suffix);
  }

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      node, bookmark_model_, new_unique_position.ToProto(),
      /*force_favicon_load=*/true);
  bookmark_tracker_->Update(entity, entity->metadata().server_version(),
                            modification_time, specifics);

  // Mark the entity that it needs to be committed.
  bookmark_tracker_->IncrementSequenceNumber(entity);
  return new_unique_position;
}

void BookmarkModelObserverImpl::UpdateAllUniquePositionsStartingAt(
    const bookmarks::BookmarkNode* parent,
    size_t start_index) {
  DCHECK_GT(start_index, 0u);
  DCHECK_LT(start_index, parent->children().size());

  syncer::UniquePosition prev =
      GetUniquePositionForNode(parent->children()[start_index - 1].get());
  for (size_t current_index = start_index;
       current_index < parent->children().size(); ++current_index) {
    // Right position is unknown because it will also be updated.
    prev = UpdateUniquePositionForNode(parent->children()[current_index].get(),
                                       prev,
                                       /*next=*/syncer::UniquePosition());
  }
}

}  // namespace sync_bookmarks
