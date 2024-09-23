// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/synced_bookmark_tracker.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/base64.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"
#include "components/version_info/version_info.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_bookmarks {

namespace {

void HashSpecifics(const sync_pb::EntitySpecifics& specifics,
                   std::string* hash) {
  DCHECK_GT(specifics.ByteSize(), 0);
  *hash =
      base::Base64Encode(base::SHA1HashString(specifics.SerializeAsString()));
}

// Returns a map from id to node for all nodes in |model|.
std::unordered_map<int64_t, const bookmarks::BookmarkNode*>
BuildIdToBookmarkNodeMap(const BookmarkModelView* model) {
  std::unordered_map<int64_t, const bookmarks::BookmarkNode*>
      id_to_bookmark_node_map;

  // The TreeNodeIterator used below doesn't include the node itself, and hence
  // add the root node separately.
  id_to_bookmark_node_map[model->root_node()->id()] = model->root_node();

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      model->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    id_to_bookmark_node_map[node->id()] = node;
  }
  return id_to_bookmark_node_map;
}

}  // namespace

// static
syncer::ClientTagHash SyncedBookmarkTracker::GetClientTagHashFromUuid(
    const base::Uuid& uuid) {
  return syncer::ClientTagHash::FromUnhashed(syncer::BOOKMARKS,
                                             uuid.AsLowercaseString());
}

// static
std::unique_ptr<SyncedBookmarkTracker> SyncedBookmarkTracker::CreateEmpty(
    sync_pb::DataTypeState data_type_state) {
  // base::WrapUnique() used because the constructor is private.
  return base::WrapUnique(new SyncedBookmarkTracker(
      std::move(data_type_state), /*bookmarks_reuploaded=*/false,
      /*num_ignored_updates_due_to_missing_parent=*/std::optional<int64_t>(0),
      /*max_version_among_ignored_updates_due_to_missing_parent=*/
      std::nullopt));
}

// static
std::unique_ptr<SyncedBookmarkTracker>
SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
    const BookmarkModelView* model,
    sync_pb::BookmarkModelMetadata model_metadata) {
  DCHECK(model);

  if (!syncer::IsInitialSyncDone(
          model_metadata.data_type_state().initial_sync_state())) {
    return nullptr;
  }

  // When the reupload feature is enabled and disabled again, there may occur
  // new entities which weren't reuploaded.
  const bool bookmarks_reuploaded =
      model_metadata.bookmarks_hierarchy_fields_reuploaded() &&
      base::FeatureList::IsEnabled(switches::kSyncReuploadBookmarks);

  std::optional<int64_t> num_ignored_updates_due_to_missing_parent;
  if (model_metadata.has_num_ignored_updates_due_to_missing_parent()) {
    num_ignored_updates_due_to_missing_parent =
        model_metadata.num_ignored_updates_due_to_missing_parent();
  }

  std::optional<int64_t>
      max_version_among_ignored_updates_due_to_missing_parent;
  if (model_metadata
          .has_max_version_among_ignored_updates_due_to_missing_parent()) {
    max_version_among_ignored_updates_due_to_missing_parent =
        model_metadata
            .max_version_among_ignored_updates_due_to_missing_parent();
  }

  // base::WrapUnique() used because the constructor is private.
  auto tracker = base::WrapUnique(new SyncedBookmarkTracker(
      model_metadata.data_type_state(), bookmarks_reuploaded,
      num_ignored_updates_due_to_missing_parent,
      max_version_among_ignored_updates_due_to_missing_parent));

  const CorruptionReason corruption_reason =
      tracker->InitEntitiesFromModelAndMetadata(model,
                                                std::move(model_metadata));

  UMA_HISTOGRAM_ENUMERATION("Sync.BookmarksModelMetadataCorruptionReason",
                            corruption_reason);

  if (corruption_reason != CorruptionReason::NO_CORRUPTION) {
    return nullptr;
  }

  return tracker;
}

SyncedBookmarkTracker::~SyncedBookmarkTracker() = default;

void SyncedBookmarkTracker::SetBookmarksReuploaded() {
  bookmarks_reuploaded_ = true;
}

const SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::GetEntityForSyncId(
    const std::string& sync_id) const {
  auto it = sync_id_to_entities_map_.find(sync_id);
  return it != sync_id_to_entities_map_.end() ? it->second.get() : nullptr;
}

const SyncedBookmarkTrackerEntity*
SyncedBookmarkTracker::GetEntityForClientTagHash(
    const syncer::ClientTagHash& client_tag_hash) const {
  auto it = client_tag_hash_to_entities_map_.find(client_tag_hash);
  return it != client_tag_hash_to_entities_map_.end() ? it->second : nullptr;
}

const SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::GetEntityForUuid(
    const base::Uuid& uuid) const {
  return GetEntityForClientTagHash(GetClientTagHashFromUuid(uuid));
}

SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::AsMutableEntity(
    const SyncedBookmarkTrackerEntity* entity) {
  DCHECK(entity);
  DCHECK_EQ(entity, GetEntityForSyncId(entity->metadata().server_id()));

  // As per DCHECK above, this tracker owns |*entity|, so it's legitimate to
  // return non-const pointer.
  return const_cast<SyncedBookmarkTrackerEntity*>(entity);
}

const SyncedBookmarkTrackerEntity*
SyncedBookmarkTracker::GetEntityForBookmarkNode(
    const bookmarks::BookmarkNode* node) const {
  auto it = bookmark_node_to_entities_map_.find(node);
  return it != bookmark_node_to_entities_map_.end() ? it->second : nullptr;
}

const SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::Add(
    const bookmarks::BookmarkNode* bookmark_node,
    const std::string& sync_id,
    int64_t server_version,
    base::Time creation_time,
    const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GT(specifics.ByteSize(), 0);
  DCHECK(bookmark_node);
  DCHECK(specifics.has_bookmark());
  DCHECK(bookmark_node->is_permanent_node() ||
         specifics.bookmark().has_unique_position());

  // Note that this gets computed for permanent nodes too.
  syncer::ClientTagHash client_tag_hash =
      GetClientTagHashFromUuid(bookmark_node->uuid());

  sync_pb::EntityMetadata metadata;
  metadata.set_is_deleted(false);
  metadata.set_server_id(sync_id);
  metadata.set_server_version(server_version);
  metadata.set_creation_time(syncer::TimeToProtoTime(creation_time));
  metadata.set_modification_time(syncer::TimeToProtoTime(creation_time));
  metadata.set_sequence_number(0);
  metadata.set_acked_sequence_number(0);
  *metadata.mutable_unique_position() = specifics.bookmark().unique_position();
  metadata.set_client_tag_hash(client_tag_hash.value());
  HashSpecifics(specifics, metadata.mutable_specifics_hash());
  metadata.set_bookmark_favicon_hash(
      base::PersistentHash(specifics.bookmark().favicon()));
  auto entity = std::make_unique<SyncedBookmarkTrackerEntity>(
      bookmark_node, std::move(metadata));

  DCHECK_EQ(0U, bookmark_node_to_entities_map_.count(bookmark_node));
  bookmark_node_to_entities_map_[bookmark_node] = entity.get();
  DCHECK_EQ(0U, client_tag_hash_to_entities_map_.count(client_tag_hash));
  client_tag_hash_to_entities_map_.emplace(std::move(client_tag_hash),
                                           entity.get());
  DCHECK_EQ(0U, sync_id_to_entities_map_.count(sync_id));
  const SyncedBookmarkTrackerEntity* raw_entity = entity.get();
  sync_id_to_entities_map_[sync_id] = std::move(entity);
  DCHECK_EQ(sync_id_to_entities_map_.size(),
            client_tag_hash_to_entities_map_.size());
  return raw_entity;
}

void SyncedBookmarkTracker::Update(const SyncedBookmarkTrackerEntity* entity,
                                   int64_t server_version,
                                   base::Time modification_time,
                                   const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GT(specifics.ByteSize(), 0);
  DCHECK(entity);
  DCHECK(specifics.has_bookmark());
  DCHECK(specifics.bookmark().has_unique_position());

  SyncedBookmarkTrackerEntity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->MutableMetadata()->set_server_version(server_version);
  mutable_entity->MutableMetadata()->set_modification_time(
      syncer::TimeToProtoTime(modification_time));
  *mutable_entity->MutableMetadata()->mutable_unique_position() =
      specifics.bookmark().unique_position();
  HashSpecifics(specifics,
                mutable_entity->MutableMetadata()->mutable_specifics_hash());
  mutable_entity->MutableMetadata()->set_bookmark_favicon_hash(
      base::PersistentHash(specifics.bookmark().favicon()));
}

void SyncedBookmarkTracker::UpdateServerVersion(
    const SyncedBookmarkTrackerEntity* entity,
    int64_t server_version) {
  DCHECK(entity);
  AsMutableEntity(entity)->MutableMetadata()->set_server_version(
      server_version);
}

void SyncedBookmarkTracker::MarkCommitMayHaveStarted(
    const SyncedBookmarkTrackerEntity* entity) {
  DCHECK(entity);
  AsMutableEntity(entity)->set_commit_may_have_started(true);
}

void SyncedBookmarkTracker::MarkDeleted(
    const SyncedBookmarkTrackerEntity* entity,
    const base::Location& location) {
  DCHECK(entity);
  DCHECK(!entity->metadata().is_deleted());
  DCHECK(entity->bookmark_node());
  DCHECK_EQ(1U, bookmark_node_to_entities_map_.count(entity->bookmark_node()));

  SyncedBookmarkTrackerEntity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->MutableMetadata()->set_is_deleted(true);
  *mutable_entity->MutableMetadata()->mutable_deletion_origin() =
      syncer::DeletionOrigin::FromLocation(location).ToProto(
          version_info::GetVersionNumber());
  mutable_entity->MutableMetadata()->clear_bookmark_favicon_hash();

  // Clear all references to the deleted bookmark node.
  bookmark_node_to_entities_map_.erase(mutable_entity->bookmark_node());
  mutable_entity->clear_bookmark_node();
  DCHECK_EQ(0, base::ranges::count(ordered_local_tombstones_, entity));
  ordered_local_tombstones_.push_back(mutable_entity);
}

void SyncedBookmarkTracker::Remove(const SyncedBookmarkTrackerEntity* entity) {
  DCHECK(entity);
  DCHECK_EQ(entity, GetEntityForSyncId(entity->metadata().server_id()));
  DCHECK_EQ(entity, GetEntityForClientTagHash(entity->GetClientTagHash()));
  DCHECK_EQ(sync_id_to_entities_map_.size(),
            client_tag_hash_to_entities_map_.size());

  if (entity->bookmark_node()) {
    DCHECK(!entity->metadata().is_deleted());
    DCHECK_EQ(0, base::ranges::count(ordered_local_tombstones_, entity));
    bookmark_node_to_entities_map_.erase(entity->bookmark_node());
  } else {
    DCHECK(entity->metadata().is_deleted());
  }

  client_tag_hash_to_entities_map_.erase(entity->GetClientTagHash());

  std::erase(ordered_local_tombstones_, entity);
  sync_id_to_entities_map_.erase(entity->metadata().server_id());
  DCHECK_EQ(sync_id_to_entities_map_.size(),
            client_tag_hash_to_entities_map_.size());
}

void SyncedBookmarkTracker::IncrementSequenceNumber(
    const SyncedBookmarkTrackerEntity* entity) {
  DCHECK(entity);
  DCHECK(!entity->bookmark_node() ||
         !entity->bookmark_node()->is_permanent_node());

  AsMutableEntity(entity)->MutableMetadata()->set_sequence_number(
      entity->metadata().sequence_number() + 1);
}

sync_pb::BookmarkModelMetadata
SyncedBookmarkTracker::BuildBookmarkModelMetadata() const {
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.set_bookmarks_hierarchy_fields_reuploaded(
      bookmarks_reuploaded_);

  if (num_ignored_updates_due_to_missing_parent_.has_value()) {
    model_metadata.set_num_ignored_updates_due_to_missing_parent(
        *num_ignored_updates_due_to_missing_parent_);
  }

  if (max_version_among_ignored_updates_due_to_missing_parent_.has_value()) {
    model_metadata.set_max_version_among_ignored_updates_due_to_missing_parent(
        *max_version_among_ignored_updates_due_to_missing_parent_);
  }

  for (const auto& [sync_id, entity] : sync_id_to_entities_map_) {
    DCHECK(entity) << " for ID " << sync_id;
    if (entity->metadata().is_deleted()) {
      // Deletions will be added later because they need to maintain the same
      // order as in |ordered_local_tombstones_|.
      continue;
    }
    DCHECK(entity->bookmark_node());
    sync_pb::BookmarkMetadata* bookmark_metadata =
        model_metadata.add_bookmarks_metadata();
    bookmark_metadata->set_id(entity->bookmark_node()->id());
    *bookmark_metadata->mutable_metadata() = entity->metadata();
  }
  // Add pending deletions.
  for (const SyncedBookmarkTrackerEntity* tombstone_entity :
       ordered_local_tombstones_) {
    DCHECK(tombstone_entity);
    DCHECK(tombstone_entity->metadata().is_deleted());
    sync_pb::BookmarkMetadata* bookmark_metadata =
        model_metadata.add_bookmarks_metadata();
    *bookmark_metadata->mutable_metadata() = tombstone_entity->metadata();
  }
  *model_metadata.mutable_data_type_state() = data_type_state_;
  return model_metadata;
}

bool SyncedBookmarkTracker::HasLocalChanges() const {
  for (const auto& [sync_id, entity] : sync_id_to_entities_map_) {
    if (entity->IsUnsynced()) {
      return true;
    }
  }
  return false;
}

std::vector<const SyncedBookmarkTrackerEntity*>
SyncedBookmarkTracker::GetAllEntities() const {
  std::vector<const SyncedBookmarkTrackerEntity*> entities;
  for (const auto& [sync_id, entity] : sync_id_to_entities_map_) {
    entities.push_back(entity.get());
  }
  return entities;
}

std::vector<const SyncedBookmarkTrackerEntity*>
SyncedBookmarkTracker::GetEntitiesWithLocalChanges() const {
  std::vector<const SyncedBookmarkTrackerEntity*> entities_with_local_changes;
  // Entities with local non deletions should be sorted such that parent
  // creation/update comes before child creation/update.
  for (const auto& [sync_id, entity] : sync_id_to_entities_map_) {
    if (entity->metadata().is_deleted()) {
      // Deletions are stored sorted in |ordered_local_tombstones_| and will be
      // added later.
      continue;
    }
    if (entity->IsUnsynced()) {
      entities_with_local_changes.push_back(entity.get());
    }
  }
  std::vector<const SyncedBookmarkTrackerEntity*> ordered_local_changes =
      ReorderUnsyncedEntitiesExceptDeletions(entities_with_local_changes);
  for (const SyncedBookmarkTrackerEntity* tombstone_entity :
       ordered_local_tombstones_) {
    DCHECK_EQ(0, base::ranges::count(ordered_local_changes, tombstone_entity));
    ordered_local_changes.push_back(tombstone_entity);
  }
  return ordered_local_changes;
}

SyncedBookmarkTracker::SyncedBookmarkTracker(
    sync_pb::DataTypeState data_type_state,
    bool bookmarks_reuploaded,
    std::optional<int64_t> num_ignored_updates_due_to_missing_parent,
    std::optional<int64_t>
        max_version_among_ignored_updates_due_to_missing_parent)
    : data_type_state_(std::move(data_type_state)),
      bookmarks_reuploaded_(bookmarks_reuploaded),
      num_ignored_updates_due_to_missing_parent_(
          num_ignored_updates_due_to_missing_parent),
      max_version_among_ignored_updates_due_to_missing_parent_(
          max_version_among_ignored_updates_due_to_missing_parent) {}

SyncedBookmarkTracker::CorruptionReason
SyncedBookmarkTracker::InitEntitiesFromModelAndMetadata(
    const BookmarkModelView* model,
    sync_pb::BookmarkModelMetadata model_metadata) {
  DCHECK(syncer::IsInitialSyncDone(data_type_state_.initial_sync_state()));

  // Build a temporary map to look up bookmark nodes efficiently by node ID.
  std::unordered_map<int64_t, const bookmarks::BookmarkNode*>
      id_to_bookmark_node_map = BuildIdToBookmarkNodeMap(model);

  for (sync_pb::BookmarkMetadata& bookmark_metadata :
       *model_metadata.mutable_bookmarks_metadata()) {
    if (!bookmark_metadata.metadata().has_server_id()) {
      DLOG(ERROR) << "Error when decoding sync metadata: Entities must contain "
                     "server id.";
      return CorruptionReason::MISSING_SERVER_ID;
    }

    const std::string sync_id = bookmark_metadata.metadata().server_id();
    if (sync_id_to_entities_map_.count(sync_id) != 0) {
      DLOG(ERROR) << "Error when decoding sync metadata: Duplicated server id.";
      return CorruptionReason::DUPLICATED_SERVER_ID;
    }

    // Handle tombstones.
    if (bookmark_metadata.metadata().is_deleted()) {
      if (bookmark_metadata.has_id()) {
        DLOG(ERROR) << "Error when decoding sync metadata: Tombstones "
                       "shouldn't have a bookmark id.";
        return CorruptionReason::BOOKMARK_ID_IN_TOMBSTONE;
      }

      if (!bookmark_metadata.metadata().has_client_tag_hash()) {
        DLOG(ERROR) << "Error when decoding sync metadata: "
                    << "Tombstone client tag hash is missing.";
        return CorruptionReason::MISSING_CLIENT_TAG_HASH;
      }

      const syncer::ClientTagHash client_tag_hash =
          syncer::ClientTagHash::FromHashed(
              bookmark_metadata.metadata().client_tag_hash());

      auto tombstone_entity = std::make_unique<SyncedBookmarkTrackerEntity>(
          /*node=*/nullptr, std::move(*bookmark_metadata.mutable_metadata()));

      if (!client_tag_hash_to_entities_map_
               .emplace(client_tag_hash, tombstone_entity.get())
               .second) {
        DLOG(ERROR) << "Error when decoding sync metadata: Duplicated client "
                       "tag hash.";
        return CorruptionReason::DUPLICATED_CLIENT_TAG_HASH;
      }

      ordered_local_tombstones_.push_back(tombstone_entity.get());
      DCHECK_EQ(0U, sync_id_to_entities_map_.count(sync_id));
      sync_id_to_entities_map_[sync_id] = std::move(tombstone_entity);
      DCHECK_EQ(sync_id_to_entities_map_.size(),
                client_tag_hash_to_entities_map_.size());
      continue;
    }

    // Non-tombstones.
    DCHECK(!bookmark_metadata.metadata().is_deleted());

    if (!bookmark_metadata.has_id()) {
      DLOG(ERROR)
          << "Error when decoding sync metadata: Bookmark id is missing.";
      return CorruptionReason::MISSING_BOOKMARK_ID;
    }

    const bookmarks::BookmarkNode* node =
        id_to_bookmark_node_map[bookmark_metadata.id()];

    if (!node) {
      DLOG(ERROR) << "Error when decoding sync metadata: unknown Bookmark id.";
      return CorruptionReason::UNKNOWN_BOOKMARK_ID;
    }

    // Note that currently the client tag hash is persisted for permanent nodes
    // too, although it's irrelevant (and even subject to change value upon
    // restart if the code changes).
    if (!bookmark_metadata.metadata().has_client_tag_hash() &&
        !node->is_permanent_node()) {
      DLOG(ERROR) << "Error when decoding sync metadata: "
                  << "Bookmark client tag hash is missing.";
      return CorruptionReason::MISSING_CLIENT_TAG_HASH;
    }

    // The client-tag-hash is expected to be equal to the hash of the bookmark's
    // UUID. This can be hit for example if local bookmark UUIDs were
    // reassigned upon startup due to duplicates (which is a BookmarkModel
    // invariant violation and should be impossible).
    const syncer::ClientTagHash client_tag_hash =
        GetClientTagHashFromUuid(node->uuid());
    if (client_tag_hash !=
        syncer::ClientTagHash::FromHashed(
            bookmark_metadata.metadata().client_tag_hash())) {
      if (node->is_permanent_node()) {
        // For permanent nodes the client tag hash is irrelevant and subject to
        // change if the constants in components/bookmarks change and adopt
        // different UUID constants. To avoid treating such state as corrupt
        // metadata, let's fix it automatically.
        bookmark_metadata.mutable_metadata()->set_client_tag_hash(
            client_tag_hash.value());
      } else {
        DLOG(ERROR) << "Bookmark UUID does not match the client tag.";
        return CorruptionReason::BOOKMARK_UUID_MISMATCH;
      }
    }

    // The code populates |bookmark_favicon_hash| for all new nodes, including
    // folders, but it is possible that folders are tracked that predate the
    // introduction of |bookmark_favicon_hash|, which never got it populated
    // because for some time it got populated opportunistically upon favicon
    // load, which never triggers for folders.
    if (!node->is_folder() &&
        !bookmark_metadata.metadata().has_bookmark_favicon_hash()) {
      return CorruptionReason::MISSING_FAVICON_HASH;
    }

    auto entity = std::make_unique<SyncedBookmarkTrackerEntity>(
        node, std::move(*bookmark_metadata.mutable_metadata()));

    if (!client_tag_hash_to_entities_map_.emplace(client_tag_hash, entity.get())
             .second) {
      DLOG(ERROR) << "Error when decoding sync metadata: Duplicated client "
                     "tag hash.";
      return CorruptionReason::DUPLICATED_CLIENT_TAG_HASH;
    }

    entity->set_commit_may_have_started(true);
    CHECK_EQ(0U, bookmark_node_to_entities_map_.count(node));
    bookmark_node_to_entities_map_[node] = entity.get();
    DCHECK_EQ(0U, sync_id_to_entities_map_.count(sync_id));
    sync_id_to_entities_map_[sync_id] = std::move(entity);
    DCHECK_EQ(sync_id_to_entities_map_.size(),
              client_tag_hash_to_entities_map_.size());
  }

  // See if there are untracked entities in the BookmarkModel.
  std::vector<int> model_node_ids;
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      model->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    if (!model->IsNodeSyncable(node)) {
      if (bookmark_node_to_entities_map_.count(node) != 0) {
        return CorruptionReason::TRACKED_MANAGED_NODE;
      }
      continue;
    }
    if (bookmark_node_to_entities_map_.count(node) == 0) {
      return CorruptionReason::UNTRACKED_BOOKMARK;
    }
  }

  CheckAllNodesTracked(model);
  return CorruptionReason::NO_CORRUPTION;
}

std::vector<const SyncedBookmarkTrackerEntity*>
SyncedBookmarkTracker::ReorderUnsyncedEntitiesExceptDeletions(
    const std::vector<const SyncedBookmarkTrackerEntity*>& entities) const {
  // This method sorts the entities with local non deletions such that parent
  // creation/update comes before child creation/update.

  // The algorithm works by constructing a forest of all non-deletion updates
  // and then traverses each tree in the forest recursively:
  // 1. Iterate over all entities and collect all nodes in |nodes|.
  // 2. Iterate over all entities again and node that is a child of another
  //    node. What's left in |nodes| are the roots of the forest.
  // 3. Start at each root in |nodes|, emit the update and recurse over its
  //    children.
  std::unordered_set<const bookmarks::BookmarkNode*> nodes;
  // Collect nodes with updates
  for (const SyncedBookmarkTrackerEntity* entity : entities) {
    DCHECK(entity->IsUnsynced());
    DCHECK(!entity->metadata().is_deleted());
    DCHECK(entity->bookmark_node());
    nodes.insert(entity->bookmark_node());
  }
  // Remove those who are direct children of another node.
  for (const SyncedBookmarkTrackerEntity* entity : entities) {
    const bookmarks::BookmarkNode* node = entity->bookmark_node();
    for (const auto& child : node->children()) {
      nodes.erase(child.get());
    }
  }
  // |nodes| contains only roots of all trees in the forest all of which are
  // ready to be processed because their parents have no pending updates.
  std::vector<const SyncedBookmarkTrackerEntity*> ordered_entities;
  for (const bookmarks::BookmarkNode* node : nodes) {
    TraverseAndAppend(node, &ordered_entities);
  }
  return ordered_entities;
}

bool SyncedBookmarkTracker::ReuploadBookmarksOnLoadIfNeeded() {
  if (bookmarks_reuploaded_ ||
      !base::FeatureList::IsEnabled(switches::kSyncReuploadBookmarks)) {
    return false;
  }
  for (const auto& [sync_id, entity] : sync_id_to_entities_map_) {
    if (entity->IsUnsynced() || entity->metadata().is_deleted()) {
      continue;
    }
    if (entity->bookmark_node()->is_permanent_node()) {
      continue;
    }
    IncrementSequenceNumber(entity.get());
  }
  SetBookmarksReuploaded();
  return true;
}

void SyncedBookmarkTracker::RecordIgnoredServerUpdateDueToMissingParent(
    int64_t server_version) {
  if (num_ignored_updates_due_to_missing_parent_.has_value()) {
    ++(*num_ignored_updates_due_to_missing_parent_);
  }

  if (max_version_among_ignored_updates_due_to_missing_parent_.has_value()) {
    *max_version_among_ignored_updates_due_to_missing_parent_ =
        std::max(*max_version_among_ignored_updates_due_to_missing_parent_,
                 server_version);
  } else {
    max_version_among_ignored_updates_due_to_missing_parent_ = server_version;
  }
}

std::optional<int64_t>
SyncedBookmarkTracker::GetNumIgnoredUpdatesDueToMissingParentForTest() const {
  return num_ignored_updates_due_to_missing_parent_;
}

std::optional<int64_t> SyncedBookmarkTracker::
    GetMaxVersionAmongIgnoredUpdatesDueToMissingParentForTest() const {
  return max_version_among_ignored_updates_due_to_missing_parent_;
}

void SyncedBookmarkTracker::TraverseAndAppend(
    const bookmarks::BookmarkNode* node,
    std::vector<const SyncedBookmarkTrackerEntity*>* ordered_entities) const {
  const SyncedBookmarkTrackerEntity* entity = GetEntityForBookmarkNode(node);
  DCHECK(entity);
  DCHECK(entity->IsUnsynced());
  DCHECK(!entity->metadata().is_deleted());
  ordered_entities->push_back(entity);
  // Recurse for all children.
  for (const auto& child : node->children()) {
    const SyncedBookmarkTrackerEntity* child_entity =
        GetEntityForBookmarkNode(child.get());
    DCHECK(child_entity);
    if (!child_entity->IsUnsynced()) {
      // If the entity has no local change, no need to check its children. If
      // any of the children would have a pending commit, it would be a root for
      // a separate tree in the forest built in
      // ReorderEntitiesWithLocalNonDeletions() and will be handled by another
      // call to TraverseAndAppend().
      continue;
    }
    if (child_entity->metadata().is_deleted()) {
      // Deletion are stored sorted in |ordered_local_tombstones_| and will be
      // added later.
      continue;
    }
    TraverseAndAppend(child.get(), ordered_entities);
  }
}

void SyncedBookmarkTracker::UpdateUponCommitResponse(
    const SyncedBookmarkTrackerEntity* entity,
    const std::string& sync_id,
    int64_t server_version,
    int64_t acked_sequence_number) {
  DCHECK(entity);

  SyncedBookmarkTrackerEntity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->MutableMetadata()->set_acked_sequence_number(
      acked_sequence_number);
  mutable_entity->MutableMetadata()->set_server_version(server_version);
  // If there are no pending commits, remove tombstones.
  if (!mutable_entity->IsUnsynced() &&
      mutable_entity->metadata().is_deleted()) {
    Remove(mutable_entity);
    return;
  }

  UpdateSyncIdIfNeeded(mutable_entity, sync_id);
}

void SyncedBookmarkTracker::UpdateSyncIdIfNeeded(
    const SyncedBookmarkTrackerEntity* entity,
    const std::string& sync_id) {
  DCHECK(entity);

  const std::string old_id = entity->metadata().server_id();
  if (old_id == sync_id) {
    return;
  }
  DCHECK_EQ(1U, sync_id_to_entities_map_.count(old_id));
  DCHECK_EQ(0U, sync_id_to_entities_map_.count(sync_id));

  std::unique_ptr<SyncedBookmarkTrackerEntity> owned_entity =
      std::move(sync_id_to_entities_map_.at(old_id));
  DCHECK_EQ(entity, owned_entity.get());
  owned_entity->MutableMetadata()->set_server_id(sync_id);
  sync_id_to_entities_map_[sync_id] = std::move(owned_entity);
  sync_id_to_entities_map_.erase(old_id);
}

void SyncedBookmarkTracker::UndeleteTombstoneForBookmarkNode(
    const SyncedBookmarkTrackerEntity* entity,
    const bookmarks::BookmarkNode* node) {
  DCHECK(entity);
  DCHECK(node);
  DCHECK(entity->metadata().is_deleted());
  const syncer::ClientTagHash client_tag_hash =
      GetClientTagHashFromUuid(node->uuid());
  // The same entity must be used only for the same bookmark node.
  DCHECK_EQ(entity->metadata().client_tag_hash(), client_tag_hash.value());
  DCHECK(bookmark_node_to_entities_map_.find(node) ==
         bookmark_node_to_entities_map_.end());
  DCHECK_EQ(GetEntityForSyncId(entity->metadata().server_id()), entity);

  std::erase(ordered_local_tombstones_, entity);
  SyncedBookmarkTrackerEntity* mutable_entity = AsMutableEntity(entity);
  mutable_entity->MutableMetadata()->set_is_deleted(false);
  mutable_entity->set_bookmark_node(node);
  bookmark_node_to_entities_map_[node] = mutable_entity;
}

void SyncedBookmarkTracker::AckSequenceNumber(
    const SyncedBookmarkTrackerEntity* entity) {
  DCHECK(entity);
  AsMutableEntity(entity)->MutableMetadata()->set_acked_sequence_number(
      entity->metadata().sequence_number());
}

bool SyncedBookmarkTracker::IsEmpty() const {
  return sync_id_to_entities_map_.empty();
}

size_t SyncedBookmarkTracker::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(sync_id_to_entities_map_);
  memory_usage += EstimateMemoryUsage(bookmark_node_to_entities_map_);
  memory_usage += EstimateMemoryUsage(ordered_local_tombstones_);
  memory_usage += EstimateMemoryUsage(data_type_state_);
  return memory_usage;
}

size_t SyncedBookmarkTracker::TrackedBookmarksCount() const {
  return bookmark_node_to_entities_map_.size();
}

size_t SyncedBookmarkTracker::TrackedUncommittedTombstonesCount() const {
  return ordered_local_tombstones_.size();
}

size_t SyncedBookmarkTracker::TrackedEntitiesCountForTest() const {
  return sync_id_to_entities_map_.size();
}

void SyncedBookmarkTracker::ClearSpecificsHashForTest(
    const SyncedBookmarkTrackerEntity* entity) {
  AsMutableEntity(entity)->MutableMetadata()->clear_specifics_hash();
}

void SyncedBookmarkTracker::CheckAllNodesTracked(
    const BookmarkModelView* bookmark_model) const {
#if DCHECK_IS_ON()
  DCHECK(GetEntityForBookmarkNode(bookmark_model->bookmark_bar_node()));
  DCHECK(GetEntityForBookmarkNode(bookmark_model->other_node()));
  DCHECK(GetEntityForBookmarkNode(bookmark_model->mobile_node()));

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmark_model->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    if (!bookmark_model->IsNodeSyncable(node)) {
      DCHECK(!GetEntityForBookmarkNode(node));
      continue;
    }
    DCHECK(GetEntityForBookmarkNode(node));
  }
#endif  // DCHECK_IS_ON()
}

}  // namespace sync_bookmarks
