// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/synced_bookmark_tracker.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/extend.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/hash/hash.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/model/processor_entity_metadata.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/models/tree_node_iterator.h"

namespace sync_bookmarks {

namespace {

// Returns a map from id to node for all nodes in |model|. |expected_size| is
// an approximate capacity hint used to pre-allocate the map.
absl::flat_hash_map<int64_t, const bookmarks::BookmarkNode*>
BuildIdToBookmarkNodeMap(const BookmarkModelView* model, size_t expected_size) {
  absl::flat_hash_map<int64_t, const bookmarks::BookmarkNode*>
      id_to_bookmark_node_map;
  id_to_bookmark_node_map.reserve(expected_size);

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

SyncedBookmarkTrackerEntity*
SyncedBookmarkTracker::GetEntityForSyncIdExhaustively(
    const std::string& sync_id) {
  return AsMutableEntity(
      std::as_const(*this).GetEntityForSyncIdExhaustively(sync_id));
}

const SyncedBookmarkTrackerEntity*
SyncedBookmarkTracker::GetEntityForSyncIdExhaustively(
    const std::string& sync_id) const {
  for (const auto& [hash, entity] : client_tag_hash_to_entities_map_) {
    if (entity->metadata().server_id() == sync_id) {
      return entity.get();
    }
  }
  return nullptr;
}

SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::GetEntityForClientTagHash(
    const syncer::ClientTagHash& client_tag_hash) {
  return AsMutableEntity(
      std::as_const(*this).GetEntityForClientTagHash(client_tag_hash));
}

const SyncedBookmarkTrackerEntity*
SyncedBookmarkTracker::GetEntityForClientTagHash(
    const syncer::ClientTagHash& client_tag_hash) const {
  return base::FindPtrOrNull(client_tag_hash_to_entities_map_, client_tag_hash);
}

SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::GetEntityForUuid(
    const base::Uuid& uuid) {
  return AsMutableEntity(std::as_const(*this).GetEntityForUuid(uuid));
}

const SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::GetEntityForUuid(
    const base::Uuid& uuid) const {
  return GetEntityForClientTagHash(GetClientTagHashFromUuid(uuid));
}

SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::GetEntityForBookmarkNode(
    const bookmarks::BookmarkNode* node) {
  return AsMutableEntity(std::as_const(*this).GetEntityForBookmarkNode(node));
}

const SyncedBookmarkTrackerEntity*
SyncedBookmarkTracker::GetEntityForBookmarkNode(
    const bookmarks::BookmarkNode* node) const {
  return base::FindPtrOrNull(bookmark_node_to_entities_map_, node);
}

SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::AddInternal(
    const bookmarks::BookmarkNode* bookmark_node,
    const std::string& sync_id,
    int64_t server_version,
    base::Time creation_time) {
  CHECK(bookmark_node);
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
  metadata.set_client_tag_hash(client_tag_hash.value());

  std::unique_ptr<syncer::ProcessorEntityMetadata> entity_metadata =
      syncer::ProcessorEntityMetadata::FromProto(std::move(metadata));
  CHECK(entity_metadata);
  auto entity = std::make_unique<SyncedBookmarkTrackerEntity>(
      bookmark_node, std::move(*entity_metadata));

  SyncedBookmarkTrackerEntity* raw_entity = entity.get();
  const bool node_inserted =
      bookmark_node_to_entities_map_.insert_or_assign(bookmark_node, raw_entity)
          .second;
  DCHECK(node_inserted);
  const bool hash_inserted =
      client_tag_hash_to_entities_map_
          .insert_or_assign(std::move(client_tag_hash), std::move(entity))
          .second;
  DCHECK(hash_inserted);
  return raw_entity;
}

SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::AddLocalCreation(
    const bookmarks::BookmarkNode* bookmark_node,
    const std::string& sync_id,
    base::Time creation_time,
    const sync_pb::EntitySpecifics& specifics) {
  SyncedBookmarkTrackerEntity* entity = AddInternal(
      bookmark_node, sync_id, syncer::kUncommittedVersion, creation_time);
  entity->RecordLocalUpdate(specifics, creation_time);
  return entity;
}

SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::AddRemote(
    const bookmarks::BookmarkNode* bookmark_node,
    const std::string& sync_id,
    int64_t server_version,
    base::Time creation_time,
    const sync_pb::EntitySpecifics& specifics) {
  CHECK_NE(server_version, syncer::kUncommittedVersion);
  SyncedBookmarkTrackerEntity* entity =
      AddInternal(bookmark_node, sync_id, server_version, creation_time);
  syncer::UpdateResponseData update;
  update.entity.id = sync_id;
  update.response_version = server_version;
  update.entity.modification_time = creation_time;
  update.entity.specifics = specifics;
  entity->RecordAcceptedRemoteUpdate(update);
  return entity;
}

SyncedBookmarkTrackerEntity* SyncedBookmarkTracker::AsMutableEntity(
    const SyncedBookmarkTrackerEntity* entity) {
  if (!entity) {
    return nullptr;
  }
  // Use `std::as_const` to invoke the `const` overload of
  // `GetEntityForClientTagHash`, preventing infinite recursion through
  // `AsMutableEntity` while ensuring invariant lookup during server ID updates.
  DCHECK_EQ(entity, std::as_const(*this).GetEntityForClientTagHash(
                        entity->GetClientTagHash()));

  // As per DCHECK above, this tracker owns |*entity|, so it's legitimate to
  // return non-const pointer.
  return const_cast<SyncedBookmarkTrackerEntity*>(entity);
}

void SyncedBookmarkTracker::OverrideServerMetadata(
    const syncer::ClientTagHash& client_tag_hash,
    const std::string& sync_id,
    int64_t server_version) {
  SyncedBookmarkTrackerEntity* entity =
      GetEntityForClientTagHash(client_tag_hash);
  if (entity) {
    entity->OverrideServerMetadata(sync_id, server_version);
  }
}

void SyncedBookmarkTracker::MarkDeleted(
    const SyncedBookmarkTrackerEntity* entity,
    const base::Location& location) {
  DCHECK(entity);
  DCHECK(!entity->IsDeleted());
  DCHECK(entity->bookmark_node());
  DCHECK(bookmark_node_to_entities_map_.contains(entity->bookmark_node()));

  SyncedBookmarkTrackerEntity* mutable_entity = AsMutableEntity(entity);

  mutable_entity->RecordLocalDeletion(
      SyncedBookmarkTrackerEntity::PassKey(),
      syncer::DeletionOrigin::FromLocation(location));

  // Clear all references to the deleted bookmark node.
  bookmark_node_to_entities_map_.erase(mutable_entity->bookmark_node());
  mutable_entity->clear_bookmark_node(SyncedBookmarkTrackerEntity::PassKey());
  DCHECK(!std::ranges::contains(ordered_local_tombstones_, mutable_entity));
  ordered_local_tombstones_.push_back(mutable_entity);
}

void SyncedBookmarkTracker::Remove(const SyncedBookmarkTrackerEntity* entity) {
  DCHECK(entity);
  CHECK_EQ(entity, GetEntityForClientTagHash(entity->GetClientTagHash()));

  SyncedBookmarkTrackerEntity* mutable_entity = AsMutableEntity(entity);

  if (mutable_entity->bookmark_node()) {
    DCHECK(!mutable_entity->IsDeleted());
    DCHECK(!std::ranges::contains(ordered_local_tombstones_, mutable_entity));
    bookmark_node_to_entities_map_.erase(mutable_entity->bookmark_node());
  } else {
    DCHECK(mutable_entity->IsDeleted());
    std::erase(ordered_local_tombstones_, entity);
  }

  client_tag_hash_to_entities_map_.erase(entity->GetClientTagHash());
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

  for (const auto& [client_tag_hash, entity] :
       client_tag_hash_to_entities_map_) {
    DCHECK(entity) << " for client tag hash " << client_tag_hash.value();
    if (entity->IsDeleted()) {
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
    DCHECK(tombstone_entity->IsDeleted());
    sync_pb::BookmarkMetadata* bookmark_metadata =
        model_metadata.add_bookmarks_metadata();
    *bookmark_metadata->mutable_metadata() = tombstone_entity->metadata();
  }
  *model_metadata.mutable_data_type_state() = data_type_state_;
  return model_metadata;
}

bool SyncedBookmarkTracker::HasLocalChanges() const {
  for (const auto& [client_tag_hash, entity] :
       client_tag_hash_to_entities_map_) {
    if (entity->IsUnsynced()) {
      return true;
    }
  }
  return false;
}

size_t SyncedBookmarkTracker::GetUnsyncedDataCount() const {
  return std::ranges::count_if(GetAllEntities(),
                               &SyncedBookmarkTrackerEntity::IsUnsynced);
}

std::vector<const SyncedBookmarkTrackerEntity*>
SyncedBookmarkTracker::GetAllEntities() const {
  return base::ToVector(
      client_tag_hash_to_entities_map_,
      [](const auto& pair) -> const SyncedBookmarkTrackerEntity* {
        return pair.second.get();
      });
}

std::vector<SyncedBookmarkTrackerEntity*>
SyncedBookmarkTracker::GetAllMutableEntities() {
  return base::ToVector(GetAllEntities(),
                        [this](const SyncedBookmarkTrackerEntity* entity) {
                          return AsMutableEntity(entity);
                        });
}

std::vector<SyncedBookmarkTrackerEntity*>
SyncedBookmarkTracker::GetMutableEntitiesWithLocalChanges() {
  return base::ToVector(GetEntitiesWithLocalChanges(),
                        [this](const SyncedBookmarkTrackerEntity* entity) {
                          return AsMutableEntity(entity);
                        });
}

std::vector<const SyncedBookmarkTrackerEntity*>
SyncedBookmarkTracker::GetEntitiesWithLocalChanges() const {
  std::vector<const SyncedBookmarkTrackerEntity*> ordered_local_changes =
      GetOrderedUnsyncedEntitiesExceptDeletions();
  // Deletions are stored sorted in |ordered_local_tombstones_| and appended
  // after all non-deletion updates.
  base::Extend(ordered_local_changes, ordered_local_tombstones_);
  DCHECK_EQ(absl::flat_hash_set<const SyncedBookmarkTrackerEntity*>(
                ordered_local_changes.begin(), ordered_local_changes.end())
                .size(),
            ordered_local_changes.size());
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

  const size_t metadata_count = model_metadata.bookmarks_metadata_size();

  // Build a temporary map to look up bookmark nodes efficiently by node ID.
  // |metadata_count + 1| serves as an approximate capacity hint (+1 accounts
  // for |model->root_node()|, which is not in |bookmarks_metadata|; |model| may
  // also contain managed/non-syncable nodes and |bookmarks_metadata| may
  // include uncommitted tombstones).
  absl::flat_hash_map<int64_t, const bookmarks::BookmarkNode*>
      id_to_bookmark_node_map =
          BuildIdToBookmarkNodeMap(model, metadata_count + 1);

  client_tag_hash_to_entities_map_.reserve(metadata_count);
  bookmark_node_to_entities_map_.reserve(metadata_count);
  absl::flat_hash_set<std::string> seen_sync_ids;
  seen_sync_ids.reserve(metadata_count);

  for (sync_pb::BookmarkMetadata& bookmark_metadata :
       *model_metadata.mutable_bookmarks_metadata()) {
    if (!bookmark_metadata.metadata().has_server_id()) {
      DLOG(ERROR) << "Error when decoding sync metadata: Entities must contain "
                     "server id.";
      return CorruptionReason::MISSING_SERVER_ID;
    }

    const std::string sync_id = bookmark_metadata.metadata().server_id();
    if (!seen_sync_ids.insert(sync_id).second) {
      DLOG(ERROR) << "Error when decoding sync metadata: Duplicated server id.";
      return CorruptionReason::DUPLICATED_SERVER_ID;
    }

    // Note that currently the client tag hash is persisted for permanent nodes
    // too, although it is not required for anything beyond in-memory tracking
    // of entities (which use a client tag hash as key).
    if (!bookmark_metadata.metadata().has_client_tag_hash()) {
      DLOG(ERROR) << "Error when decoding sync metadata: "
                  << "Bookmark client tag hash is missing.";
      return CorruptionReason::MISSING_CLIENT_TAG_HASH;
    }

    std::unique_ptr<syncer::ProcessorEntityMetadata> metadata =
        syncer::ProcessorEntityMetadata::FromProto(
            std::move(*bookmark_metadata.mutable_metadata()));
    if (!metadata) {
      DLOG(ERROR) << "Error when decoding sync metadata: Metadata is invalid.";
      return CorruptionReason::INVALID_METADATA;
    }

    // Handle tombstones.
    if (metadata->IsDeleted()) {
      if (bookmark_metadata.has_id()) {
        DLOG(ERROR) << "Error when decoding sync metadata: Tombstones "
                       "shouldn't have a bookmark id.";
        return CorruptionReason::BOOKMARK_ID_IN_TOMBSTONE;
      }

      syncer::ClientTagHash client_tag_hash = metadata->GetClientTagHash();

      auto tombstone_entity = std::make_unique<SyncedBookmarkTrackerEntity>(
          /*node=*/nullptr, std::move(*metadata));
      SyncedBookmarkTrackerEntity* raw_tombstone_entity =
          tombstone_entity.get();

      if (!client_tag_hash_to_entities_map_
               .try_emplace(std::move(client_tag_hash),
                            std::move(tombstone_entity))
               .second) {
        DLOG(ERROR) << "Error when decoding sync metadata: Duplicated client "
                       "tag hash.";
        return CorruptionReason::DUPLICATED_CLIENT_TAG_HASH;
      }

      ordered_local_tombstones_.push_back(raw_tombstone_entity);
      continue;
    }

    // Non-tombstones.
    DCHECK(!metadata->IsDeleted());

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

    // The client-tag-hash is expected to be equal to the hash of the bookmark's
    // UUID. This can be hit for example if local bookmark UUIDs were
    // reassigned upon startup due to duplicates (which is a BookmarkModel
    // invariant violation and should be impossible).
    syncer::ClientTagHash client_tag_hash =
        GetClientTagHashFromUuid(node->uuid());
    if (client_tag_hash != metadata->GetClientTagHash()) {
      DLOG(ERROR) << "Bookmark UUID does not match the client tag.";
      return CorruptionReason::BOOKMARK_UUID_MISMATCH;
    }

    // The code populates |bookmark_favicon_hash| for all new nodes, including
    // folders, but it is possible that folders are tracked that predate the
    // introduction of |bookmark_favicon_hash|, which never got it populated
    // because for some time it got populated opportunistically upon favicon
    // load, which never triggers for folders.
    if (!node->is_folder() && !metadata->proto().has_bookmark_favicon_hash()) {
      return CorruptionReason::MISSING_FAVICON_HASH;
    }

    auto entity = std::make_unique<SyncedBookmarkTrackerEntity>(
        node, std::move(*metadata));
    SyncedBookmarkTrackerEntity* raw_entity = entity.get();

    if (!client_tag_hash_to_entities_map_
             .try_emplace(std::move(client_tag_hash), std::move(entity))
             .second) {
      DLOG(ERROR) << "Error when decoding sync metadata: Duplicated client "
                     "tag hash.";
      return CorruptionReason::DUPLICATED_CLIENT_TAG_HASH;
    }

    raw_entity->MarkCommitMayHaveStarted();
    const bool node_inserted =
        bookmark_node_to_entities_map_.emplace(node, raw_entity).second;
    CHECK(node_inserted);
  }

  // See if there are untracked entities in the BookmarkModel.
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      model->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();
    if (!model->IsNodeSyncable(node)) {
      if (bookmark_node_to_entities_map_.contains(node)) {
        return CorruptionReason::TRACKED_MANAGED_NODE;
      }
      continue;
    }
    if (!bookmark_node_to_entities_map_.contains(node)) {
      return CorruptionReason::UNTRACKED_BOOKMARK;
    }
  }

  CheckAllNodesTracked(model);
  return CorruptionReason::NO_CORRUPTION;
}

std::vector<const SyncedBookmarkTrackerEntity*>
SyncedBookmarkTracker::GetOrderedUnsyncedEntitiesExceptDeletions() const {
  // Entities with local non-deletions must be ordered such that parent
  // creation/update comes before child creation/update. Treat each unsynced
  // non-deleted entity whose parent has no pending non-deletion update as a
  // root in the forest of updates, and traverse top-down from each root.
  std::vector<const SyncedBookmarkTrackerEntity*> ordered_entities;
  for (const auto& [client_tag_hash, entity] :
       client_tag_hash_to_entities_map_) {
    if (entity->IsDeleted()) {
      // Deletions are stored sorted in |ordered_local_tombstones_| and will be
      // added later.
      continue;
    }
    if (!entity->IsUnsynced()) {
      continue;
    }
    const bookmarks::BookmarkNode* node = entity->bookmark_node();
    DCHECK(node);
    const bookmarks::BookmarkNode* parent = node->parent();
    const SyncedBookmarkTrackerEntity* parent_entity =
        parent ? GetEntityForBookmarkNode(parent) : nullptr;
    if (!parent_entity || !parent_entity->IsUnsynced() ||
        parent_entity->IsDeleted()) {
      TraverseAndAppend(entity.get(), &ordered_entities);
    }
  }
  return ordered_entities;
}

bool SyncedBookmarkTracker::ReuploadBookmarksOnLoadIfNeeded() {
  if (bookmarks_reuploaded_ ||
      !base::FeatureList::IsEnabled(switches::kSyncReuploadBookmarks)) {
    return false;
  }
  for (const auto& [client_tag_hash, entity] :
       client_tag_hash_to_entities_map_) {
    if (entity->IsUnsynced() || entity->IsDeleted()) {
      continue;
    }
    if (entity->bookmark_node()->is_permanent_node()) {
      continue;
    }
    entity->IncrementSequenceNumber();
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
    const SyncedBookmarkTrackerEntity* entity,
    std::vector<const SyncedBookmarkTrackerEntity*>* ordered_entities) const {
  DCHECK(entity);
  DCHECK(entity->IsUnsynced());
  DCHECK(!entity->IsDeleted());
  const bookmarks::BookmarkNode* node = entity->bookmark_node();
  DCHECK(node);
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
      // GetOrderedUnsyncedEntitiesExceptDeletions() and will be handled by
      // another call to TraverseAndAppend().
      continue;
    }
    if (child_entity->IsDeleted()) {
      // Deletion are stored sorted in |ordered_local_tombstones_| and will be
      // added later.
      continue;
    }
    TraverseAndAppend(child_entity, ordered_entities);
  }
}

void SyncedBookmarkTracker::UndeleteTombstoneForBookmarkNode(
    const SyncedBookmarkTrackerEntity* entity,
    const bookmarks::BookmarkNode* node,
    const sync_pb::EntitySpecifics& specifics,
    base::Time modification_time) {
  DCHECK(entity);
  DCHECK(node);
  DCHECK(entity->IsDeleted());
  const syncer::ClientTagHash client_tag_hash =
      GetClientTagHashFromUuid(node->uuid());
  // The same entity must be used only for the same bookmark node.
  DCHECK_EQ(entity->GetClientTagHash(), client_tag_hash);
  DCHECK(!bookmark_node_to_entities_map_.contains(node));
  DCHECK_EQ(GetEntityForClientTagHash(entity->GetClientTagHash()), entity);

  SyncedBookmarkTrackerEntity* mutable_entity = AsMutableEntity(entity);
  std::erase(ordered_local_tombstones_, mutable_entity);
  mutable_entity->UndeleteTombstoneForBookmarkNode(
      SyncedBookmarkTrackerEntity::PassKey(), node, specifics,
      modification_time);
  bookmark_node_to_entities_map_.emplace(node, mutable_entity);
}

bool SyncedBookmarkTracker::IsEmpty() const {
  return client_tag_hash_to_entities_map_.empty();
}

size_t SyncedBookmarkTracker::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  memory_usage += EstimateMemoryUsage(client_tag_hash_to_entities_map_);
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
  return client_tag_hash_to_entities_map_.size();
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
