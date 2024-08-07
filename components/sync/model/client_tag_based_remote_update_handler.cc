// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/client_tag_based_remote_update_handler.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/data_type_processor_metrics.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/processor_entity.h"
#include "components/sync/model/processor_entity_tracker.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/unique_position.pb.h"

namespace syncer {

namespace {

std::optional<sync_pb::UniquePosition> ExtractUniquePositionIfSupported(
    const UpdateResponseData& update,
    const DataTypeSyncBridge& bridge) {
  CHECK(!update.entity.is_deleted());
  if (!bridge.SupportsUniquePositions()) {
    return std::nullopt;
  }
  return bridge.GetUniquePosition(update.entity.specifics);
}

}  // namespace

ClientTagBasedRemoteUpdateHandler::ClientTagBasedRemoteUpdateHandler(
    DataType type,
    DataTypeSyncBridge* bridge,
    ProcessorEntityTracker* entity_tracker)
    : type_(type), bridge_(bridge), entity_tracker_(entity_tracker) {
  DCHECK(bridge_);
  DCHECK(entity_tracker_);
}

std::optional<ModelError>
ClientTagBasedRemoteUpdateHandler::ProcessIncrementalUpdate(
    const sync_pb::DataTypeState& data_type_state,
    UpdateResponseDataList updates,
    std::optional<sync_pb::GarbageCollectionDirective> gc_directive) {
  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge_->CreateMetadataChangeList();
  EntityChangeList entity_changes;

  metadata_changes->UpdateDataTypeState(data_type_state);
  const bool got_new_encryption_requirements =
      entity_tracker_->data_type_state().encryption_key_name() !=
      data_type_state.encryption_key_name();
  entity_tracker_->set_data_type_state(data_type_state);

  // If new encryption requirements come from the server, the entities that are
  // in |updates| will be recorded here so they can be ignored during the
  // re-encryption phase at the end.
  std::unordered_set<std::string> already_updated;

  for (syncer::UpdateResponseData& update : updates) {
    std::string storage_key_to_clear;
    ProcessorEntity* entity = ProcessUpdate(std::move(update), &entity_changes,
                                            &storage_key_to_clear);

    if (!entity) {
      // The update is either of the following:
      // 1. Tombstone of entity that didn't exist locally.
      // 2. Reflection, thus should be ignored.
      // 3. Update without a client tag hash (including permanent nodes, which
      // have server tags instead).
      // 4. Remote creation or update containing invalid data according to the
      // bridge.
      continue;
    }

    // Log update freshness metrics only if the initial sync is fully done (for
    // data types in ApplyUpdatesImmediatelyTypes(), it may only be
    // PARTIALLY_DONE here).
    if (IsInitialSyncDone(data_type_state.initial_sync_state())) {
      LogNonReflectionUpdateFreshnessToUma(
          type_,
          /*remote_modification_time=*/
          ProtoTimeToTime(entity->metadata().modification_time()));
    }

    if (entity->storage_key().empty()) {
      // Storage key of this entity is not known yet. Don't update metadata, it
      // will be done from UpdateStorageKey.

      // If this is the result of a conflict resolution (where a remote
      // undeletion was preferred), then need to clear a metadata entry from
      // the database.
      if (!storage_key_to_clear.empty()) {
        metadata_changes->ClearMetadata(storage_key_to_clear);
      }
      continue;
    }

    DCHECK(storage_key_to_clear.empty());

    if (got_new_encryption_requirements) {
      already_updated.insert(entity->storage_key());
    }

    if (entity->CanClearMetadata()) {
      metadata_changes->ClearMetadata(entity->storage_key());
      // The line below frees |entity| and it shouldn't be used afterwards.
      entity_tracker_->RemoveEntityForStorageKey(entity->storage_key());
    } else {
      metadata_changes->UpdateMetadata(entity->storage_key(),
                                       entity->metadata());
    }
  }

  if (gc_directive && gc_directive->has_collaboration_gc()) {
    auto active_collaborations = base::MakeFlatSet<std::string>(
        gc_directive->collaboration_gc().active_collaboration_ids());
    std::vector<std::string> removed_storage_keys =
        entity_tracker_->RemoveInactiveCollaborations(active_collaborations);
    DVLOG(2) << "Storage keys to remove for inactive collaborations: "
             << removed_storage_keys.size();
    for (const std::string& removed_storage_key : removed_storage_keys) {
      metadata_changes->ClearMetadata(removed_storage_key);
      entity_changes.push_back(
          EntityChange::CreateDeletedCollaborationMembership(
              removed_storage_key));
    }
  }

  if (got_new_encryption_requirements) {
    // TODO(pavely): Currently we recommit all entities. We should instead
    // recommit only the ones whose encryption key doesn't match the one in
    // DataTypeState. Work is tracked in http://crbug.com/727874.
    std::vector<const ProcessorEntity*> entities =
        entity_tracker_->IncrementSequenceNumberForAllExcept(already_updated);
    for (const ProcessorEntity* entity : entities) {
      metadata_changes->UpdateMetadata(entity->storage_key(),
                                       entity->metadata());
    }
  }

  // Inform the bridge of the new or updated data.
  return bridge_->ApplyIncrementalSyncChanges(std::move(metadata_changes),
                                              std::move(entity_changes));
}

ProcessorEntity* ClientTagBasedRemoteUpdateHandler::ProcessUpdate(
    UpdateResponseData update,
    EntityChangeList* entity_changes,
    std::string* storage_key_to_clear) {
  const EntityData& data = update.entity;
  const ClientTagHash& client_tag_hash = data.client_tag_hash;

  // Filter out updates without a client tag hash (including permanent nodes,
  // which have server tags instead).
  if (client_tag_hash.value().empty()) {
    return nullptr;
  }

  // Filter out unexpected client tag hashes.
  if (!data.is_deleted() && bridge_->SupportsGetClientTag() &&
      client_tag_hash !=
          ClientTagHash::FromUnhashed(type_, bridge_->GetClientTag(data))) {
    SyncRecordDataTypeUpdateDropReason(UpdateDropReason::kInconsistentClientTag,
                                       type_);
    DLOG(WARNING) << "Received unexpected client tag hash: " << client_tag_hash
                  << " for " << DataTypeToDebugString(type_);
    return nullptr;
  }

  ProcessorEntity* entity =
      entity_tracker_->GetEntityForTagHash(client_tag_hash);

  // Handle corner cases first.
  if (entity == nullptr && data.is_deleted()) {
    // Local entity doesn't exist and update is tombstone.
    SyncRecordDataTypeUpdateDropReason(
        UpdateDropReason::kTombstoneForNonexistentInIncrementalUpdate, type_);
    DLOG(WARNING) << "Received remote delete for a non-existing item."
                  << " client_tag_hash: " << client_tag_hash << " for "
                  << DataTypeToDebugString(type_);
    return nullptr;
  }

  if (entity && entity->IsVersionAlreadyKnown(update.response_version)) {
    // Seen this update before; just ignore it.
    return nullptr;
  }

  // TODO(crbug.com/40889096): Remove the storage key check as storage keys
  // should not be empty after IsEntityDataValid() has been implemented by all
  // bridges.
  if (!data.is_deleted() && (!bridge_->IsEntityDataValid(data) ||
                             (bridge_->SupportsGetStorageKey() &&
                              bridge_->GetStorageKey(data).empty()))) {
    DLOG(WARNING) << "Received invalid remote update."
                  << " client_tag_hash: " << client_tag_hash << " for "
                  << DataTypeToDebugString(type_);
    return nullptr;
  }

  // Cache update encryption_key_name and is_deleted in case |update| will be
  // moved away into ResolveConflict().
  const std::string update_encryption_key_name = update.encryption_key_name;
  const bool update_is_tombstone = data.is_deleted();
  if (entity == nullptr) {
    // Remote creation.
    DCHECK(!data.is_deleted());
    entity = CreateEntity(update);
    CHECK(entity);
    entity_changes->push_back(EntityChange::CreateAdd(
        entity->storage_key(), std::move(update.entity)));
  } else if (entity->IsUnsynced()) {
    // Conflict.
    ResolveConflict(std::move(update), entity, entity_changes,
                    storage_key_to_clear);
  } else if (data.is_deleted()) {
    // Remote deletion. Note that the local data cannot be already deleted,
    // because it would have been treated as a conflict earlier above.
    DCHECK(!entity->metadata().is_deleted());
    entity->RecordAcceptedRemoteUpdate(update, /*trimmed_specifics=*/{},
                                       /*unique_position=*/std::nullopt);
    entity_changes->push_back(
        EntityChange::CreateDelete(entity->storage_key()));
  } else if (entity->MatchesData(data)) {
    // Remote update that is a no-op, metadata should still be updated.
    entity->RecordAcceptedRemoteUpdate(
        update,
        bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(data.specifics),
        ExtractUniquePositionIfSupported(update, *bridge_));
  } else {
    // Remote update.
    entity->RecordAcceptedRemoteUpdate(
        update,
        bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(data.specifics),
        ExtractUniquePositionIfSupported(update, *bridge_));
    entity_changes->push_back(EntityChange::CreateUpdate(
        entity->storage_key(), std::move(update.entity)));
  }

  // If the received entity has out of date encryption, we schedule another
  // commit to fix it. Tombstones aren't encrypted and hence shouldn't be
  // checked.
  if (!update_is_tombstone &&
      entity_tracker_->data_type_state().encryption_key_name() !=
          update_encryption_key_name) {
    DVLOG(2) << DataTypeToDebugString(type_)
             << ": Requesting re-encrypt commit " << update_encryption_key_name
             << " -> "
             << entity_tracker_->data_type_state().encryption_key_name();

    entity->IncrementSequenceNumber(base::Time::Now());
  }
  return entity;
}

void ClientTagBasedRemoteUpdateHandler::ResolveConflict(
    UpdateResponseData update,
    ProcessorEntity* entity,
    EntityChangeList* changes,
    std::string* storage_key_to_clear) {
  const EntityData& remote_data = update.entity;

  ConflictResolution resolution_type = ConflictResolution::kChangesMatch;

  // Determine the type of resolution.
  if (entity->MatchesData(remote_data)) {
    // The changes are identical so there isn't a real conflict.
    resolution_type = ConflictResolution::kChangesMatch;
  } else if (entity->metadata().is_deleted()) {
    // Local tombstone vs remote update (non-deletion). Should be undeleted.
    resolution_type = ConflictResolution::kUseRemote;
  } else if (entity->MatchesOwnBaseData()) {
    // If there is no real local change, then the entity must be unsynced due to
    // a pending local re-encryption request. In this case, the remote data
    // should win.
    resolution_type = ConflictResolution::kIgnoreLocalEncryption;
  } else if (entity->MatchesBaseData(remote_data)) {
    // The remote data isn't actually changing from the last remote data that
    // was seen, so it must have been a re-encryption and can be ignored.
    resolution_type = ConflictResolution::kIgnoreRemoteEncryption;
  } else {
    // There's a real data conflict here; let the bridge resolve it.
    resolution_type =
        bridge_->ResolveConflict(entity->storage_key(), remote_data);
  }
  RecordDataTypeEntityConflictResolution(type_, resolution_type);

  // Apply the resolution.
  switch (resolution_type) {
    case ConflictResolution::kChangesMatch:
      // Record the update and squash the pending commit. Trimming should not be
      // called for matching deleted entities to avoid failing its requirement
      // to have a `password` field present.
      // TODO(crbug.com/40214653): Consider introducing a dedicated function for
      // recording exact matching updates.
      if (!update.entity.is_deleted()) {
        entity->RecordForcedRemoteUpdate(
            update,
            bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(
                update.entity.specifics),
            ExtractUniquePositionIfSupported(update, *bridge_));
      } else {
        entity->RecordForcedRemoteUpdate(update, /*trimmed_specifics=*/{},
                                         /*unique_position=*/std::nullopt);
      }
      break;
    case ConflictResolution::kUseLocal:
    case ConflictResolution::kIgnoreRemoteEncryption:
      // Record that we received the update from the server but leave the
      // pending commit intact.
      entity->RecordIgnoredRemoteUpdate(update);
      break;
    case ConflictResolution::kUseRemote:
    case ConflictResolution::kIgnoreLocalEncryption:
      // Update client data to match server.
      if (update.entity.is_deleted()) {
        DCHECK(!entity->metadata().is_deleted());
        // Squash the pending commit.
        entity->RecordForcedRemoteUpdate(update,
                                         /*trimmed_specifics=*/{},
                                         /*unique_position=*/std::nullopt);
        changes->push_back(EntityChange::CreateDelete(entity->storage_key()));
      } else if (!entity->metadata().is_deleted()) {
        // Squash the pending commit.
        entity->RecordForcedRemoteUpdate(
            update,
            bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(
                update.entity.specifics),
            ExtractUniquePositionIfSupported(update, *bridge_));
        changes->push_back(EntityChange::CreateUpdate(
            entity->storage_key(), std::move(update.entity)));
      } else {
        // Remote undeletion. This could imply a new storage key for some
        // bridges, so we may need to wait until UpdateStorageKey() is called.
        if (!bridge_->SupportsGetStorageKey()) {
          *storage_key_to_clear = entity->storage_key();
          entity_tracker_->ClearStorageKey(entity->storage_key());
          DCHECK(entity->storage_key().empty());
        }
        // Squash the pending commit.
        entity->RecordForcedRemoteUpdate(
            update,
            bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(
                update.entity.specifics),
            ExtractUniquePositionIfSupported(update, *bridge_));
        changes->push_back(EntityChange::CreateAdd(entity->storage_key(),
                                                   std::move(update.entity)));
      }
      break;
  }
}

ProcessorEntity* ClientTagBasedRemoteUpdateHandler::CreateEntity(
    const UpdateResponseData& update) {
  CHECK(bridge_->IsEntityDataValid(update.entity));
  DCHECK(!update.entity.client_tag_hash.value().empty());
  if (bridge_->SupportsGetClientTag()) {
    DCHECK_EQ(update.entity.client_tag_hash,
              ClientTagHash::FromUnhashed(
                  type_, bridge_->GetClientTag(update.entity)));
  }
  std::string storage_key;
  if (bridge_->SupportsGetStorageKey()) {
    storage_key = bridge_->GetStorageKey(update.entity);
    // If the storage key was empty, CreateEntity() won't be reached.
    CHECK(!storage_key.empty());
  }
  return entity_tracker_->AddRemote(
      storage_key, update,
      bridge_->TrimAllSupportedFieldsFromRemoteSpecifics(
          update.entity.specifics),
      ExtractUniquePositionIfSupported(update, *bridge_));
}

}  // namespace syncer
