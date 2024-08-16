// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_PROCESSOR_ENTITY_TRACKER_H_
#define COMPONENTS_SYNC_MODEL_PROCESSOR_ENTITY_TRACKER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/protocol/data_type_state.pb.h"

namespace sync_pb {
class EntityMetadata;
class EntitySpecifics;
}  // namespace sync_pb

namespace syncer {

class ProcessorEntity;

// This component tracks entities for ClientTagBasedDataTypeProcessor.
class ProcessorEntityTracker {
 public:
  // Creates tracker and fills entities data from batch metadata map. This
  // constructor must be used only if initial_sync_done returns true in
  // |data_type_state|.
  ProcessorEntityTracker(
      const sync_pb::DataTypeState& data_type_state,
      std::map<std::string, std::unique_ptr<sync_pb::EntityMetadata>>
          metadata_map);

  ~ProcessorEntityTracker();

  // Returns true if all processor entities have non-empty storage keys.
  // This may happen during initial merge and for some data types during any
  // remote creation.
  bool AllStorageKeysPopulated() const;

  // Clears any in-memory sync state associated with outstanding commits
  // for each entity.
  void ClearTransientSyncState();

  // Returns number of entities with non-deleted metadata.
  size_t CountNonTombstoneEntries() const;

  // Starts tracking new locally-created entity (must not be deleted outside
  // current object). The entity will be created unsynced with pending commit
  // data.
  ProcessorEntity* AddUnsyncedLocal(
      const std::string& storage_key,
      std::unique_ptr<EntityData> data,
      sync_pb::EntitySpecifics trimmed_specifics,
      std::optional<sync_pb::UniquePosition> unique_position);

  // Starts tracking new remotely-created entity (must not be deleted outside
  // current object).
  ProcessorEntity* AddRemote(
      const std::string& storage_key,
      const UpdateResponseData& update_data,
      sync_pb::EntitySpecifics trimmed_specifics,
      std::optional<sync_pb::UniquePosition> unique_position);

  // Removes item from |entities_| and |storage_key_to_tag_hash|. If entity does
  // not exist, does nothing.
  void RemoveEntityForClientTagHash(const ClientTagHash& client_tag_hash);
  void RemoveEntityForStorageKey(const std::string& storage_key);

  // Removes items from |entities_| which are associated with a collaboration
  // which is not active anymore. Returns storage keys for the deleted entities.
  std::vector<std::string> RemoveInactiveCollaborations(
      const base::flat_set<std::string>& active_collaborations);

  // Removes |storage_key| from |storage_key_to_tag_hash_| and clears it for
  // the corresponding entity. Does not remove the entity from |entities_|.
  void ClearStorageKey(const std::string& storage_key);

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  // Gets the entity for the given tag hash, or null if there isn't one.
  ProcessorEntity* GetEntityForTagHash(const ClientTagHash& tag_hash);
  const ProcessorEntity* GetEntityForTagHash(
      const ClientTagHash& tag_hash) const;

  // Gets the entity for the given storage key, or null if there isn't one.
  ProcessorEntity* GetEntityForStorageKey(const std::string& storage_key);
  const ProcessorEntity* GetEntityForStorageKey(
      const std::string& storage_key) const;

  // Returns all entities including tombstones.
  std::vector<const ProcessorEntity*> GetAllEntitiesIncludingTombstones() const;

  // Returns all entities with local changes.
  // TODO(rushans): make it const, at this moment returned entities must be
  // initialized to commit.
  std::vector<ProcessorEntity*> GetEntitiesWithLocalChanges(size_t max_entries);

  // Returns true if there are any local entities to be committed.
  bool HasLocalChanges() const;

  const sync_pb::DataTypeState& data_type_state() const {
    return data_type_state_;
  }

  void set_data_type_state(const sync_pb::DataTypeState& data_type_state) {
    data_type_state_ = data_type_state;
  }

  // Returns number of entities, including tombstones.
  size_t size() const;

  // Increments sequence number for all entities except those in
  // |already_updated_storage_keys|. Returns affected list of entities.
  std::vector<const ProcessorEntity*> IncrementSequenceNumberForAllExcept(
      const std::unordered_set<std::string>& already_updated_storage_keys);

  // Assigns a new storage key to the entity for the given |client_tag_hash|.
  // Clears previous storage key if entity already has one (the metadata of the
  // entity must be deleted).
  void UpdateOrOverrideStorageKey(const ClientTagHash& client_tag_hash,
                                  const std::string& storage_key);

 private:
  // Creates a new processor entity (must not be deleted outside current
  // object).
  ProcessorEntity* AddInternal(const std::string& storage_key,
                               const EntityData& data,
                               int64_t server_version);

  // A map of client tag hash to sync entities known to this tracker. This
  // should contain entries and metadata, although the entities may not always
  // contain data type data/specifics.
  std::map<ClientTagHash, std::unique_ptr<ProcessorEntity>> entities_;

  // The data type metadata (progress marker, initial sync done, etc).
  sync_pb::DataTypeState data_type_state_;

  // The bridge wants to communicate entirely via storage keys that it is free
  // to define and can understand more easily. All of the sync machinery wants
  // to use client tag hash. This mapping allows us to convert from storage key
  // to client tag hash. The other direction can use |entities_|.
  // Entity is temporarily not included in this map for the duration of
  // MergeFullSyncData/ApplyIncrementalSyncChanges call when the bridge doesn't
  // support GetStorageKey(). In this case the bridge is responsible for
  // updating storage key with UpdateStorageKey() call from within
  // MergeFullSyncData/ApplyIncrementalSyncChanges.
  std::map<std::string, ClientTagHash> storage_key_to_tag_hash_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_PROCESSOR_ENTITY_TRACKER_H_
