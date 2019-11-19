// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_PROCESSOR_ENTITY_H_
#define COMPONENTS_SYNC_MODEL_IMPL_PROCESSOR_ENTITY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/entity_metadata.pb.h"

namespace syncer {

class ClientTagHash;
struct CommitRequestData;
struct CommitResponseData;
struct UpdateResponseData;

// This class is used by the ClientTagBasedModelTypeProcessor to track the state
// of each entity with its type. It can be considered a helper class internal to
// the processor. It manages the metadata for its entity and caches entity data
// upon a local change until commit confirmation is received.
class ProcessorEntity {
 public:
  // Construct an instance representing a new locally-created item.
  static std::unique_ptr<ProcessorEntity> CreateNew(
      const std::string& storage_key,
      const ClientTagHash& client_tag_hash,
      const std::string& id,
      base::Time creation_time);

  // Construct an instance representing an item loaded from storage on init.
  static std::unique_ptr<ProcessorEntity> CreateFromMetadata(
      const std::string& storage_key,
      sync_pb::EntityMetadata metadata);

  ~ProcessorEntity();

  const std::string& storage_key() const { return storage_key_; }
  const sync_pb::EntityMetadata& metadata() const { return metadata_; }
  const EntityData& commit_data() { return *commit_data_; }

  // Returns true if this data is out of sync with the server.
  // A commit may or may not be in progress at this time.
  bool IsUnsynced() const;

  // Returns true if this data is out of sync with the sync thread.
  //
  // There may or may not be a commit in progress for this item, but there's
  // definitely no commit in progress for this (most up to date) version of
  // this item.
  bool RequiresCommitRequest() const;

  // Whether commit data is needed to be cached before a commit request can be
  // created. Note that deletions do not require cached data.
  bool RequiresCommitData() const;

  // Whether it's safe to clear the metadata for this entity. This means that
  // the entity is deleted and either knowledge of this entity has never left
  // this client or it is up to date with the server.
  bool CanClearMetadata() const;

  // Returns true if the specified update version does not contain new data.
  bool UpdateIsReflection(int64_t update_version) const;

  void RecordEntityUpdateLatency(int64_t update_version, ModelType type);

  // Records that an update from the server was received but ignores its data.
  void RecordIgnoredUpdate(const UpdateResponseData& response_data);

  // Records an update from the server assuming its data is the new data for
  // this entity.
  void RecordAcceptedUpdate(const UpdateResponseData& response_data);

  // Squashes a pending commit with an update from the server.
  void RecordForcedUpdate(const UpdateResponseData& response_data);

  // Applies a local change to this item.
  void MakeLocalChange(std::unique_ptr<EntityData> data);

  // Applies a local deletion to this item. Returns true if entity was
  // previously committed to server and tombstone should be sent.
  bool Delete();

  // Initializes a message representing this item's uncommitted state
  // and assumes that it is forwarded to the sync engine for commiting.
  void InitializeCommitRequestData(CommitRequestData* request);

  // Receives a successful commit response.
  //
  // Successful commit responses can overwrite an item's ID.
  //
  // Note that the receipt of a successful commit response does not necessarily
  // unset IsUnsynced().  If many local changes occur in quick succession, it's
  // possible that the committed item was already out of date by the time it
  // reached the server.
  void ReceiveCommitResponse(const CommitResponseData& data,
                             bool commit_only,
                             ModelType type_for_uma);

  // Clears any in-memory sync state associated with outstanding commits.
  void ClearTransientSyncState();

  // Update storage_key_. Allows setting storage key for datatypes that don't
  // generate storage key from syncer::EntityData. Should only be called for
  // an entity initialized with empty storage key.
  void SetStorageKey(const std::string& storage_key);

  // Undoes SetStorageKey(), which is needed in certain conflict resolution
  // scenarios.
  void ClearStorageKey();

  // Takes the passed commit data updates its fields with values from metadata
  // and caches it in the instance.
  void SetCommitData(std::unique_ptr<EntityData> data);

  void CacheCommitData(std::unique_ptr<EntityData> data);

  // Check if the instance has cached commit data.
  bool HasCommitData() const;

  // Check whether |data| matches the stored specifics hash.
  bool MatchesData(const EntityData& data) const;

  // Check whether the current metadata of an unsynced entity matches the stored
  // base specifics hash.
  bool MatchesOwnBaseData() const;

  // Check whether |data| matches the stored base specifics hash.
  bool MatchesBaseData(const EntityData& data) const;

  // Increment sequence number in the metadata. This will also update the
  // base_specifics_hash if the entity was not already unsynced.
  void IncrementSequenceNumber(base::Time modification_time);

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  friend class ProcessorEntityTest;

  // The constructor swaps the data from the passed metadata.
  ProcessorEntity(const std::string& storage_key,
                  sync_pb::EntityMetadata metadata);

  // Check whether |specifics| matches the stored specifics_hash.
  bool MatchesSpecificsHash(const sync_pb::EntitySpecifics& specifics) const;

  // Update hash string for EntitySpecifics in the metadata.
  void UpdateSpecificsHash(const sync_pb::EntitySpecifics& specifics);

  // Storage key. Should always be available.
  std::string storage_key_;

  // Serializable Sync metadata.
  sync_pb::EntityMetadata metadata_;

  // Sync data that exists for items being committed only. The data is moved
  // away when sending the commit request.
  std::unique_ptr<EntityData> commit_data_;

  // The sequence number of the last item sent to the sync thread.
  int64_t commit_requested_sequence_number_;

  // The time when this entity transition from being synced to being unsynced
  // (i.e. a local change happened).
  base::Time unsynced_time_;

  std::map<int64_t, base::Time> unsynced_time_per_committed_server_version_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_PROCESSOR_ENTITY_H_
