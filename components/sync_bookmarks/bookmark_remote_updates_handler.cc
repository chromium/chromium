// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_remote_updates_handler.h"

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/data_type_processor_metrics.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"

namespace sync_bookmarks {

namespace {

// Used in metrics: "Sync.ProblematicServerSideBookmarks". These values are
// persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class RemoteBookmarkUpdateError {
  // Remote and local bookmarks types don't match (URL vs. Folder).
  kConflictingTypes = 0,
  // Invalid specifics.
  kInvalidSpecifics = 1,
  // Invalid unique position.
  // kDeprecatedInvalidUniquePosition = 2,
  // Permanent node creation in an incremental update.
  // kDeprecatedPermanentNodeCreationAfterMerge = 3,
  // Parent entity not found in server.
  kMissingParentEntity = 4,
  // Parent node not found locally.
  kMissingParentNode = 5,
  // Parent entity not found in server when processing a conflict.
  kMissingParentEntityInConflict = 6,
  // Parent Parent node not found locally when processing a conflict.
  kMissingParentNodeInConflict = 7,
  // Failed to create a bookmark.
  kCreationFailure = 8,
  // The bookmark's UUID did not match the originator client item ID.
  kUnexpectedGuid = 9,
  // Parent is not a folder.
  kParentNotFolder = 10,
  // The UUID changed for an already-tracked server ID.
  kGuidChangedForTrackedServerId = 11,
  // An update to a permanent node received without a server-defined unique tag.
  kTrackedServerIdWithoutServerTagMatchesPermanentNode = 12,

  kMaxValue = kTrackedServerIdWithoutServerTagMatchesPermanentNode,
};

void LogProblematicBookmark(RemoteBookmarkUpdateError problem) {
  base::UmaHistogramEnumeration("Sync.ProblematicServerSideBookmarks", problem);
}

// Recursive method to traverse a forest created by ReorderValidUpdates() to
// emit updates in top-down order. |ordered_updates| must not be null because
// traversed updates are appended to |*ordered_updates|.
void TraverseAndAppendChildren(
    const base::Uuid& node_uuid,
    const std::unordered_multimap<base::Uuid,
                                  const syncer::UpdateResponseData*,
                                  base::UuidHash>& uuid_to_updates,
    const std::unordered_map<base::Uuid,
                             std::vector<base::Uuid>,
                             base::UuidHash>& node_to_children,
    std::vector<const syncer::UpdateResponseData*>* ordered_updates) {
  // If no children to traverse, we are done.
  if (node_to_children.count(node_uuid) == 0) {
    return;
  }
  // Recurse over all children.
  for (const base::Uuid& child : node_to_children.at(node_uuid)) {
    auto [begin, end] = uuid_to_updates.equal_range(child);
    DCHECK(begin != end);
    for (auto it = begin; it != end; ++it) {
      ordered_updates->push_back(it->second);
    }
    TraverseAndAppendChildren(child, uuid_to_updates, node_to_children,
                              ordered_updates);
  }
}

syncer::UniquePosition ComputeUniquePositionForTrackedBookmarkNode(
    const SyncedBookmarkTracker* bookmark_tracker,
    const bookmarks::BookmarkNode* bookmark_node) {
  DCHECK(bookmark_tracker);

  const SyncedBookmarkTrackerEntity* child_entity =
      bookmark_tracker->GetEntityForBookmarkNode(bookmark_node);
  DCHECK(child_entity);
  // TODO(crbug.com/40710102): precompute UniquePosition to prevent its
  // calculation on each remote update.
  return syncer::UniquePosition::FromProto(
      child_entity->metadata().unique_position());
}

size_t ComputeChildNodeIndex(const bookmarks::BookmarkNode* parent,
                             const sync_pb::UniquePosition& unique_position,
                             const SyncedBookmarkTracker* bookmark_tracker) {
  DCHECK(parent);
  DCHECK(bookmark_tracker);

  const syncer::UniquePosition position =
      syncer::UniquePosition::FromProto(unique_position);

  auto iter = base::ranges::partition_point(
      parent->children(),
      [bookmark_tracker,
       &position](const std::unique_ptr<bookmarks::BookmarkNode>& child) {
        // Return true for all |parent|'s children whose position is less than
        // |position|.
        return !position.LessThan(ComputeUniquePositionForTrackedBookmarkNode(
            bookmark_tracker, child.get()));
      });

  return iter - parent->children().begin();
}

bool IsPermanentNodeUpdate(const syncer::EntityData& update_entity) {
  return !update_entity.server_defined_unique_tag.empty();
}

// Checks that the |update_entity| is valid and returns false otherwise. It is
// used to verify non-deletion updates. |update| must not be a deletion and a
// permanent node (they are processed in a different way).
bool IsValidUpdate(const syncer::EntityData& update_entity) {
  DCHECK(!update_entity.is_deleted());
  DCHECK(!IsPermanentNodeUpdate(update_entity));

  if (!IsValidBookmarkSpecifics(update_entity.specifics.bookmark())) {
    // Ignore updates with invalid specifics.
    DLOG(ERROR)
        << "Couldn't process an update bookmark with an invalid specifics.";
    LogProblematicBookmark(RemoteBookmarkUpdateError::kInvalidSpecifics);
    return false;
  }

  if (!HasExpectedBookmarkGuid(update_entity.specifics.bookmark(),
                               update_entity.client_tag_hash,
                               update_entity.originator_cache_guid,
                               update_entity.originator_client_item_id)) {
    // Ignore updates with an unexpected UUID.
    DLOG(ERROR) << "Couldn't process an update bookmark with unexpected UUID: "
                << update_entity.specifics.bookmark().guid();
    LogProblematicBookmark(RemoteBookmarkUpdateError::kUnexpectedGuid);
    return false;
  }

  return true;
}

// Determines the parent's UUID included in |update_entity|. |update_entity|
// must be a valid update as defined in IsValidUpdate().
base::Uuid GetParentUuidInUpdate(const syncer::EntityData& update_entity) {
  DCHECK(IsValidUpdate(update_entity));
  base::Uuid parent_uuid = base::Uuid::ParseLowercase(
      update_entity.specifics.bookmark().parent_guid());
  DCHECK(parent_uuid.is_valid());
  return parent_uuid;
}

void ApplyRemoteUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTrackerEntity* tracked_entity,
    const SyncedBookmarkTrackerEntity* new_parent_tracked_entity,
    BookmarkModelView* model,
    SyncedBookmarkTracker* tracker,
    favicon::FaviconService* favicon_service) {
  const syncer::EntityData& update_entity = update.entity;
  DCHECK(!update_entity.is_deleted());
  DCHECK(tracked_entity);
  DCHECK(tracked_entity->bookmark_node());
  DCHECK(new_parent_tracked_entity);
  DCHECK(model);
  DCHECK(tracker);
  DCHECK(favicon_service);
  DCHECK_EQ(
      tracked_entity->bookmark_node()->uuid(),
      base::Uuid::ParseLowercase(update_entity.specifics.bookmark().guid()));

  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  const bookmarks::BookmarkNode* old_parent = node->parent();
  const bookmarks::BookmarkNode* new_parent =
      new_parent_tracked_entity->bookmark_node();

  DCHECK(old_parent);
  DCHECK(new_parent);
  DCHECK(old_parent->is_folder());
  DCHECK(new_parent->is_folder());

  if (update_entity.specifics.bookmark().type() !=
      GetProtoTypeFromBookmarkNode(node)) {
    DLOG(ERROR) << "Could not update bookmark node due to conflicting types";
    LogProblematicBookmark(RemoteBookmarkUpdateError::kConflictingTypes);
    return;
  }

  UpdateBookmarkNodeFromSpecifics(update_entity.specifics.bookmark(), node,
                                  model, favicon_service);
  // Compute index information before updating the |tracker|.
  const size_t old_index = old_parent->GetIndexOf(node).value();
  const size_t new_index = ComputeChildNodeIndex(
      new_parent, update_entity.specifics.bookmark().unique_position(),
      tracker);
  tracker->Update(tracked_entity, update.response_version,
                  update_entity.modification_time, update_entity.specifics);

  if (new_parent == old_parent &&
      (new_index == old_index || new_index == old_index + 1)) {
    // Node hasn't moved. No more work to do.
    return;
  }
  // Node has moved to another position under the same parent. Update the model.
  // BookmarkModel takes care of placing the node in the correct position if the
  // node is move to the left. (i.e. no need to subtract one from |new_index|).
  model->Move(node, new_parent, new_index);
}

}  // namespace

BookmarkRemoteUpdatesHandler::BookmarkRemoteUpdatesHandler(
    BookmarkModelView* bookmark_model,
    favicon::FaviconService* favicon_service,
    SyncedBookmarkTracker* bookmark_tracker)
    : bookmark_model_(bookmark_model),
      favicon_service_(favicon_service),
      bookmark_tracker_(bookmark_tracker) {
  DCHECK(bookmark_model);
  DCHECK(bookmark_tracker);
  DCHECK(favicon_service);
}

void BookmarkRemoteUpdatesHandler::Process(
    const syncer::UpdateResponseDataList& updates,
    bool got_new_encryption_requirements) {
  TRACE_EVENT0("sync", "BookmarkRemoteUpdatesHandler::Process");

  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
  // If new encryption requirements come from the server, the entities that are
  // in |updates| will be recorded here so they can be ignored during the
  // re-encryption phase at the end.
  std::unordered_set<std::string> entities_with_up_to_date_encryption;

  for (const syncer::UpdateResponseData* update :
       ReorderValidUpdates(&updates)) {
    const syncer::EntityData& update_entity = update->entity;

    DCHECK(!IsPermanentNodeUpdate(update_entity));
    DCHECK(update_entity.is_deleted() || IsValidUpdate(update_entity));

    bool should_ignore_update = false;
    const SyncedBookmarkTrackerEntity* tracked_entity =
        DetermineLocalTrackedEntityToUpdate(update_entity,
                                            &should_ignore_update);
    if (should_ignore_update) {
      continue;
    }

    // Filter out permanent nodes once again (in case the server tag wasn't
    // populated and yet the entity ID points to a permanent node). This case
    // shoudn't be possible with a well-behaving server.
    if (tracked_entity && tracked_entity->bookmark_node() &&
        tracked_entity->bookmark_node()->is_permanent_node()) {
      DLOG(ERROR) << "Ignoring update to permanent node without server defined "
                     "unique tag for ID "
                  << update_entity.id;
      LogProblematicBookmark(
          RemoteBookmarkUpdateError::
              kTrackedServerIdWithoutServerTagMatchesPermanentNode);
      continue;
    }

    // Ignore updates that have already been seen according to the version.
    if (tracked_entity && tracked_entity->metadata().server_version() >=
                              update->response_version) {
      if (update_entity.id == tracked_entity->metadata().server_id()) {
        // Seen this update before. This update may be a reflection and may have
        // missing the UUID in specifics. Next reupload will populate UUID in
        // specifics and this codepath will not repeat indefinitely. This logic
        // is needed for the case when there is only one device and hence the
        // UUID will not be set by other devices.
        ReuploadEntityIfNeeded(update_entity, tracked_entity);
      }
      continue;
    }

    // Record freshness of the update to UMA. To mimic the behavior in
    // ClientTagBasedDataTypeProcessor, one scenario is special-cased: an
    // incoming tombstone for an entity that is not tracked.
    if (tracked_entity || !update_entity.is_deleted()) {
      syncer::LogNonReflectionUpdateFreshnessToUma(
          syncer::BOOKMARKS,
          /*remote_modification_time=*/
          update_entity.modification_time);
    }

    // The server ID has changed for a tracked entity (matched via client tag).
    // This can happen if a commit succeeds, but the response does not come back
    // fast enough(e.g. before shutdown or crash), then the |bookmark_tracker_|
    // might assume that it was never committed. The server will track the
    // client that sent up the original commit and return this in a get updates
    // response. This also may happen due to duplicate UUIDs. In this case it's
    // better to update to the latest server ID.
    if (tracked_entity) {
      bookmark_tracker_->UpdateSyncIdIfNeeded(tracked_entity,
                                              /*sync_id=*/update_entity.id);
    }

    if (tracked_entity && tracked_entity->IsUnsynced()) {
      tracked_entity = ProcessConflict(*update, tracked_entity);
      if (!tracked_entity) {
        // During conflict resolution, the entity could be dropped in case of
        // a conflict between local and remote deletions. We shouldn't worry
        // about changes to the encryption in that case.
        continue;
      }
    } else if (update_entity.is_deleted()) {
      ProcessDelete(update_entity, tracked_entity);
      // If the local entity has been deleted, no need to check for out of date
      // encryption. Therefore, we can go ahead and process the next update.
      continue;
    } else if (!tracked_entity) {
      tracked_entity = ProcessCreate(*update);
      if (!tracked_entity) {
        // If no new node has been tracked, we shouldn't worry about changes to
        // the encryption.
        continue;
      }
      DCHECK_EQ(tracked_entity,
                bookmark_tracker_->GetEntityForSyncId(update_entity.id));
    } else {
      ProcessUpdate(*update, tracked_entity);
      DCHECK_EQ(tracked_entity,
                bookmark_tracker_->GetEntityForSyncId(update_entity.id));
    }

    // If the received entity has out of date encryption, we schedule another
    // commit to fix it.
    if (bookmark_tracker_->data_type_state().encryption_key_name() !=
        update->encryption_key_name) {
      DVLOG(2) << "Bookmarks: Requesting re-encrypt commit "
               << update->encryption_key_name << " -> "
               << bookmark_tracker_->data_type_state().encryption_key_name();
      bookmark_tracker_->IncrementSequenceNumber(tracked_entity);
    }

    if (got_new_encryption_requirements) {
      entities_with_up_to_date_encryption.insert(update_entity.id);
    }
  }

  // Recommit entities with out of date encryption.
  if (got_new_encryption_requirements) {
    std::vector<const SyncedBookmarkTrackerEntity*> all_entities =
        bookmark_tracker_->GetAllEntities();
    for (const SyncedBookmarkTrackerEntity* entity : all_entities) {
      // No need to recommit tombstones and permanent nodes.
      if (entity->metadata().is_deleted()) {
        continue;
      }
      DCHECK(entity->bookmark_node());
      if (entity->bookmark_node()->is_permanent_node()) {
        continue;
      }
      if (entities_with_up_to_date_encryption.count(
              entity->metadata().server_id()) != 0) {
        continue;
      }
      bookmark_tracker_->IncrementSequenceNumber(entity);
    }
  }
  bookmark_tracker_->CheckAllNodesTracked(bookmark_model_);
}

// static
std::vector<const syncer::UpdateResponseData*>
BookmarkRemoteUpdatesHandler::ReorderValidUpdatesForTest(
    const syncer::UpdateResponseDataList* updates) {
  return ReorderValidUpdates(updates);
}

// static
size_t BookmarkRemoteUpdatesHandler::ComputeChildNodeIndexForTest(
    const bookmarks::BookmarkNode* parent,
    const sync_pb::UniquePosition& unique_position,
    const SyncedBookmarkTracker* bookmark_tracker) {
  return ComputeChildNodeIndex(parent, unique_position, bookmark_tracker);
}

// static
std::vector<const syncer::UpdateResponseData*>
BookmarkRemoteUpdatesHandler::ReorderValidUpdates(
    const syncer::UpdateResponseDataList* updates) {
  // This method sorts the remote updates according to the following rules:
  // 1. Creations and updates come before deletions.
  // 2. Parent creation/update should come before child creation/update.
  // 3. No need to further order deletions. Parent deletions can happen before
  //    child deletions. This is safe because all updates (e.g. moves) should
  //    have been processed already.

  // The algorithm works by constructing a forest of all non-deletion updates
  // and then traverses each tree in the forest recursively: Forest
  // Construction:
  // 1. Iterate over all updates and construct the |parent_to_children| map and
  //    collect all parents in |roots|.
  // 2. Iterate over all updates again and drop any parent that has a
  //    coressponding update. What's left in |roots| are the roots of the
  //    forest.
  // 3. Start at each root in |roots|, emit the update and recurse over its
  //    children.

  // Normally there shouldn't be multiple updates for the same UUID, but let's
  // avoiding dedupping here just in case (e.g. the could in theory be a
  // combination of client-tagged and non-client-tagged updated that
  // DataTypeWorker failed to deduplicate.
  std::unordered_multimap<base::Uuid, const syncer::UpdateResponseData*,
                          base::UuidHash>
      uuid_to_updates;

  // Add only valid, non-deletions to |uuid_to_updates|.
  int invalid_updates_count = 0;
  int root_node_updates_count = 0;
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity;
    // Ignore updates to root nodes.
    if (IsPermanentNodeUpdate(update_entity)) {
      ++root_node_updates_count;
      continue;
    }
    if (update_entity.is_deleted()) {
      continue;
    }
    if (!IsValidUpdate(update_entity)) {
      ++invalid_updates_count;
      continue;
    }
    base::Uuid uuid =
        base::Uuid::ParseLowercase(update_entity.specifics.bookmark().guid());
    DCHECK(uuid.is_valid());
    uuid_to_updates.emplace(std::move(uuid), &update);
  }

  // Iterate over |uuid_to_updates| and construct |roots| and
  // |parent_to_children|.
  std::set<base::Uuid> roots;
  std::unordered_map<base::Uuid, std::vector<base::Uuid>, base::UuidHash>
      parent_to_children;
  for (const auto& [uuid, update] : uuid_to_updates) {
    base::Uuid parent_uuid = GetParentUuidInUpdate(update->entity);
    base::Uuid child_uuid =
        base::Uuid::ParseLowercase(update->entity.specifics.bookmark().guid());
    DCHECK(child_uuid.is_valid());

    parent_to_children[parent_uuid].emplace_back(std::move(child_uuid));
    // If this entity's parent has no pending update, add it to |roots|.
    if (uuid_to_updates.count(parent_uuid) == 0) {
      roots.insert(std::move(parent_uuid));
    }
  }
  // |roots| contains only root of all trees in the forest all of which are
  // ready to be processed because none has a pending update.
  std::vector<const syncer::UpdateResponseData*> ordered_updates;
  for (const base::Uuid& root : roots) {
    TraverseAndAppendChildren(root, uuid_to_updates, parent_to_children,
                              &ordered_updates);
  }
  // Add deletions.
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity;
    if (!IsPermanentNodeUpdate(update_entity) && update_entity.is_deleted()) {
      ordered_updates.push_back(&update);
    }
  }
  // All non root updates should have been included in |ordered_updates|.
  DCHECK_EQ(updates->size(), ordered_updates.size() + root_node_updates_count +
                                 invalid_updates_count);
  return ordered_updates;
}

const SyncedBookmarkTrackerEntity*
BookmarkRemoteUpdatesHandler::DetermineLocalTrackedEntityToUpdate(
    const syncer::EntityData& update_entity,
    bool* should_ignore_update) {
  *should_ignore_update = false;

  // If there's nothing other than a server ID to issue a lookup, just do that
  // and return immediately. This is the case for permanent nodes and possibly
  // tombstones (at least the LoopbackServer only sets the server ID).
  if (update_entity.originator_client_item_id.empty() &&
      update_entity.client_tag_hash.value().empty()) {
    return bookmark_tracker_->GetEntityForSyncId(update_entity.id);
  }

  // Parse the client tag hash in the update or infer it from the originator
  // information (all of which are immutable properties of a sync entity).
  const syncer::ClientTagHash client_tag_hash_in_update =
      !update_entity.client_tag_hash.value().empty()
          ? update_entity.client_tag_hash
          : SyncedBookmarkTracker::GetClientTagHashFromUuid(
                InferGuidFromLegacyOriginatorId(
                    update_entity.originator_cache_guid,
                    update_entity.originator_client_item_id));

  const SyncedBookmarkTrackerEntity* const tracked_entity_by_client_tag =
      bookmark_tracker_->GetEntityForClientTagHash(client_tag_hash_in_update);
  const SyncedBookmarkTrackerEntity* const tracked_entity_by_sync_id =
      bookmark_tracker_->GetEntityForSyncId(update_entity.id);

  // The most common scenario is that both lookups, client-tag-based and
  // server-ID-based, refer to the same tracked entity or both lookups fail. In
  // that case there's nothing to reconcile and the function can return
  // trivially.
  if (tracked_entity_by_client_tag == tracked_entity_by_sync_id) {
    return tracked_entity_by_client_tag;
  }

  // Client-tags (UUIDs) are known at all times and immutable (as opposed to
  // server IDs which get a temp value for local creations), so they cannot have
  // changed.
  if (tracked_entity_by_sync_id &&
      tracked_entity_by_sync_id->GetClientTagHash() !=
          client_tag_hash_in_update) {
    // The client tag has changed for an already-tracked entity, which is a
    // protocol violation. This should be practically unreachable, but guard
    // against misbehaving servers.
    DLOG(ERROR) << "Ignoring remote bookmark update with protocol violation: "
                   "UUID must be immutable";
    LogProblematicBookmark(
        RemoteBookmarkUpdateError::kGuidChangedForTrackedServerId);
    *should_ignore_update = true;
    return nullptr;
  }

  // At this point |tracked_entity_by_client_tag| must be non-null because
  // otherwise one of the two codepaths above would have returned early.
  DCHECK(tracked_entity_by_client_tag);
  DCHECK(!tracked_entity_by_sync_id);

  return tracked_entity_by_client_tag;
}

const SyncedBookmarkTrackerEntity* BookmarkRemoteUpdatesHandler::ProcessCreate(
    const syncer::UpdateResponseData& update) {
  const syncer::EntityData& update_entity = update.entity;
  DCHECK(!update_entity.is_deleted());
  DCHECK(!IsPermanentNodeUpdate(update_entity));
  DCHECK(IsValidBookmarkSpecifics(update_entity.specifics.bookmark()));

  const bookmarks::BookmarkNode* parent_node = GetParentNode(update_entity);
  if (!parent_node) {
    // If we cannot find the parent, we can do nothing.
    LogProblematicBookmark(RemoteBookmarkUpdateError::kMissingParentNode);
    bookmark_tracker_->RecordIgnoredServerUpdateDueToMissingParent(
        update.response_version);
    return nullptr;
  }
  if (!parent_node->is_folder()) {
    LogProblematicBookmark(RemoteBookmarkUpdateError::kParentNotFolder);
    return nullptr;
  }
  const bookmarks::BookmarkNode* bookmark_node =
      CreateBookmarkNodeFromSpecifics(
          update_entity.specifics.bookmark(), parent_node,
          ComputeChildNodeIndex(
              parent_node, update_entity.specifics.bookmark().unique_position(),
              bookmark_tracker_),
          bookmark_model_, favicon_service_);
  DCHECK(bookmark_node);
  const SyncedBookmarkTrackerEntity* entity = bookmark_tracker_->Add(
      bookmark_node, update_entity.id, update.response_version,
      update_entity.creation_time, update_entity.specifics);
  ReuploadEntityIfNeeded(update_entity, entity);
  return entity;
}

void BookmarkRemoteUpdatesHandler::ProcessUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTrackerEntity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity;
  // Can only update existing nodes.
  DCHECK(tracked_entity);
  DCHECK(tracked_entity->bookmark_node());
  DCHECK(!tracked_entity->bookmark_node()->is_permanent_node());
  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));
  // Must not be a deletion.
  DCHECK(!update_entity.is_deleted());
  DCHECK(!IsPermanentNodeUpdate(update_entity));
  DCHECK(IsValidBookmarkSpecifics(update_entity.specifics.bookmark()));
  DCHECK(!tracked_entity->IsUnsynced());

  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  const bookmarks::BookmarkNode* old_parent = node->parent();
  DCHECK(old_parent);
  DCHECK(old_parent->is_folder());

  const SyncedBookmarkTrackerEntity* new_parent_entity =
      bookmark_tracker_->GetEntityForUuid(GetParentUuidInUpdate(update_entity));
  if (!new_parent_entity) {
    LogProblematicBookmark(RemoteBookmarkUpdateError::kMissingParentEntity);
    return;
  }
  const bookmarks::BookmarkNode* new_parent =
      new_parent_entity->bookmark_node();
  if (!new_parent) {
    LogProblematicBookmark(RemoteBookmarkUpdateError::kMissingParentNode);
    return;
  }
  if (!new_parent->is_folder()) {
    LogProblematicBookmark(RemoteBookmarkUpdateError::kParentNotFolder);
    return;
  }
  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change.
  if (tracked_entity->MatchesData(update_entity)) {
    DCHECK_EQ(new_parent, old_parent);
    bookmark_tracker_->Update(tracked_entity, update.response_version,
                              update_entity.modification_time,
                              update_entity.specifics);
    ReuploadEntityIfNeeded(update_entity, tracked_entity);
    return;
  }
  ApplyRemoteUpdate(update, tracked_entity, new_parent_entity, bookmark_model_,
                    bookmark_tracker_, favicon_service_);
  ReuploadEntityIfNeeded(update_entity, tracked_entity);
}

void BookmarkRemoteUpdatesHandler::ProcessDelete(
    const syncer::EntityData& update_entity,
    const SyncedBookmarkTrackerEntity* tracked_entity) {
  DCHECK(update_entity.is_deleted());

  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));

  // Handle corner cases first.
  if (tracked_entity == nullptr) {
    // Process deletion only if the entity is still tracked. It could have
    // been recursively deleted already with an earlier deletion of its
    // parent.
    DVLOG(1) << "Received remote delete for a non-existing item.";
    return;
  }

  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  DCHECK(node);
  // Changes to permanent nodes have been filtered out earlier.
  DCHECK(!node->is_permanent_node());
  // Remove the entities of |node| and its children.
  RemoveEntityAndChildrenFromTracker(node);
  // Remove the node and its children from the model.
  bookmark_model_->Remove(node, FROM_HERE);
}

// This method doesn't explicitly handle conflicts as a result of re-encryption:
// remote update wins even if there wasn't a real change in specifics. However,
// this scenario is very unlikely and hence the implementation is less
// sophisticated than in ClientTagBasedDataTypeProcessor (it would require
// introducing base hash specifics to track remote changes).
const SyncedBookmarkTrackerEntity*
BookmarkRemoteUpdatesHandler::ProcessConflict(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTrackerEntity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity;

  // Can only conflict with existing nodes.
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));
  DCHECK(!tracked_entity->bookmark_node() ||
         !tracked_entity->bookmark_node()->is_permanent_node());
  DCHECK(!IsPermanentNodeUpdate(update_entity));

  if (tracked_entity->metadata().is_deleted() && update_entity.is_deleted()) {
    // Both have been deleted, delete the corresponding entity from the tracker.
    bookmark_tracker_->Remove(tracked_entity);
    syncer::RecordDataTypeEntityConflictResolution(
        syncer::BOOKMARKS, syncer::ConflictResolution::kChangesMatch);
    return nullptr;
  }

  if (update_entity.is_deleted()) {
    // Only remote has been deleted. Local wins. Record that we received the
    // update from the server but leave the pending commit intact.
    bookmark_tracker_->UpdateServerVersion(tracked_entity,
                                           update.response_version);
    syncer::RecordDataTypeEntityConflictResolution(
        syncer::BOOKMARKS, syncer::ConflictResolution::kUseLocal);
    return tracked_entity;
  }

  DCHECK(IsValidBookmarkSpecifics(update_entity.specifics.bookmark()));

  if (tracked_entity->metadata().is_deleted()) {
    // Only local node has been deleted. It should be restored from the server
    // data as a remote creation.
    bookmark_tracker_->Remove(tracked_entity);
    syncer::RecordDataTypeEntityConflictResolution(
        syncer::BOOKMARKS, syncer::ConflictResolution::kUseRemote);
    return ProcessCreate(update);
  }

  // No deletions, there are potentially conflicting updates.
  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  const bookmarks::BookmarkNode* old_parent = node->parent();
  DCHECK(old_parent);
  DCHECK(old_parent->is_folder());

  const SyncedBookmarkTrackerEntity* new_parent_entity =
      bookmark_tracker_->GetEntityForUuid(GetParentUuidInUpdate(update_entity));

  // The |new_parent_entity| could be null in some racy conditions.  For
  // example, when a client A moves a node and deletes the old parent and
  // commits, and then updates the node again, and at the same time client B
  // updates before receiving the move updates. The client B update will arrive
  // at client A after the parent entity has been deleted already.
  if (!new_parent_entity) {
    LogProblematicBookmark(
        RemoteBookmarkUpdateError::kMissingParentEntityInConflict);
    syncer::RecordDataTypeEntityConflictResolution(
        syncer::BOOKMARKS, syncer::ConflictResolution::kUseLocal);
    return tracked_entity;
  }
  const bookmarks::BookmarkNode* new_parent =
      new_parent_entity->bookmark_node();
  // |new_parent| would be null if the parent has been deleted locally and not
  // committed yet. Deletions are executed recursively, so a parent deletions
  // entails child deletion, and if this child has been updated on another
  // client, this would cause conflict.
  if (!new_parent) {
    LogProblematicBookmark(
        RemoteBookmarkUpdateError::kMissingParentNodeInConflict);
    syncer::RecordDataTypeEntityConflictResolution(
        syncer::BOOKMARKS, syncer::ConflictResolution::kUseLocal);
    return tracked_entity;
  }
  // Either local and remote data match or server wins, and in both cases we
  // should squash any pending commits.
  bookmark_tracker_->AckSequenceNumber(tracked_entity);

  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change.
  if (tracked_entity->MatchesData(update_entity)) {
    DCHECK_EQ(new_parent, old_parent);
    bookmark_tracker_->Update(tracked_entity, update.response_version,
                              update_entity.modification_time,
                              update_entity.specifics);

    // The changes are identical so there isn't a real conflict.
    syncer::RecordDataTypeEntityConflictResolution(
        syncer::BOOKMARKS, syncer::ConflictResolution::kChangesMatch);
  } else {
    // Conflict where data don't match and no remote deletion, and hence server
    // wins. Update the model from server data.
    syncer::RecordDataTypeEntityConflictResolution(
        syncer::BOOKMARKS, syncer::ConflictResolution::kUseRemote);
    ApplyRemoteUpdate(update, tracked_entity, new_parent_entity,
                      bookmark_model_, bookmark_tracker_, favicon_service_);
  }
  ReuploadEntityIfNeeded(update_entity, tracked_entity);
  return tracked_entity;
}

void BookmarkRemoteUpdatesHandler::RemoveEntityAndChildrenFromTracker(
    const bookmarks::BookmarkNode* node) {
  DCHECK(node);
  DCHECK(!node->is_permanent_node());

  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  DCHECK(entity);
  bookmark_tracker_->Remove(entity);

  for (const auto& child : node->children()) {
    RemoveEntityAndChildrenFromTracker(child.get());
  }
}

const bookmarks::BookmarkNode* BookmarkRemoteUpdatesHandler::GetParentNode(
    const syncer::EntityData& update_entity) const {
  DCHECK(IsValidBookmarkSpecifics(update_entity.specifics.bookmark()));

  const SyncedBookmarkTrackerEntity* parent_entity =
      bookmark_tracker_->GetEntityForUuid(GetParentUuidInUpdate(update_entity));
  if (!parent_entity) {
    return nullptr;
  }
  return parent_entity->bookmark_node();
}

void BookmarkRemoteUpdatesHandler::ReuploadEntityIfNeeded(
    const syncer::EntityData& entity_data,
    const SyncedBookmarkTrackerEntity* tracked_entity) {
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity->metadata().server_id(), entity_data.id);
  DCHECK(!tracked_entity->bookmark_node() ||
         !tracked_entity->bookmark_node()->is_permanent_node());

  // Do not initiate reupload if the local entity is a tombstone.
  const bool is_reupload_needed = tracked_entity->bookmark_node() &&
                                  IsBookmarkEntityReuploadNeeded(entity_data);
  base::UmaHistogramBoolean(
      "Sync.BookmarkEntityReuploadNeeded.OnIncrementalUpdate",
      is_reupload_needed);
  if (is_reupload_needed) {
    bookmark_tracker_->IncrementSequenceNumber(tracked_entity);
  }
}

}  // namespace sync_bookmarks
