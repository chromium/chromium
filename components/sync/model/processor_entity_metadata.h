// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_PROCESSOR_ENTITY_METADATA_H_
#define COMPONENTS_SYNC_MODEL_PROCESSOR_ENTITY_METADATA_H_

#include <memory>
#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/protocol/entity_metadata.pb.h"

namespace sync_pb {
class EntitySpecifics;
class UniquePosition;
}  // namespace sync_pb

namespace syncer {

struct EntityData;
class CollaborationMetadata;
struct CommitResponseData;
struct UpdateResponseData;
class DeletionOrigin;

// Class representing the metadata associated to a sync entity tracked
// internally by a processor in-memory.
class ProcessorEntityMetadata {
 public:
  // Checks if the given `metadata` is valid.
  static bool IsValid(const sync_pb::EntityMetadata& metadata);

  // Creates a new ProcessorEntityMetadata with default values.
  static ProcessorEntityMetadata CreateNew(const ClientTagHash& client_tag_hash,
                                           std::string server_id,
                                           base::Time creation_time);

  // Creates a ProcessorEntityMetadata from a proto. Returns null if the proto
  // is invalid.
  static std::unique_ptr<ProcessorEntityMetadata> FromProto(
      sync_pb::EntityMetadata metadata);

  ~ProcessorEntityMetadata();

  ProcessorEntityMetadata(const ProcessorEntityMetadata&) = delete;
  ProcessorEntityMetadata& operator=(const ProcessorEntityMetadata&) = delete;
  ProcessorEntityMetadata(ProcessorEntityMetadata&&) = default;
  ProcessorEntityMetadata& operator=(ProcessorEntityMetadata&&) = default;

  // Returns the underlying proto metadata.
  const sync_pb::EntityMetadata& proto() const { return metadata_; }

  // Returns true if the entity is deleted.
  bool IsDeleted() const;

  // Returns true if the entity has local changes that have not been
  // acknowledged by the server yet.
  bool IsUnsynced() const;

  // Returns true if the entity was created locally and has not been committed
  // to the server yet.
  bool IsUnsyncedLocalCreation() const;

  // Returns true if the server version of this entity is greater than or equal
  // to `update_version`.
  bool IsVersionAlreadyKnown(int64_t update_version) const;

  // Overrides the server ID and version.
  void OverrideServerMetadata(const std::string& server_id,
                              int64_t server_version);

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  // Returns the client tag hash of the entity.
  ClientTagHash GetClientTagHash() const;

  // Increments the sequence number of the entity, indicating a local change.
  // This will also update the base_specifics_hash if the entity was not already
  // unsynced.
  void IncrementSequenceNumber();

  // Returns true if the entity's specifics hash (sync-ed or unsynced) matches
  // the specifics in `data`. Also checks that deletion status matches.
  bool MatchesData(const EntityData& data) const;

  // Returns true if the entity's base specifics hash matches the specifics in
  // `data`. Base data represents the last specifics known by both the client
  // and server.
  bool MatchesBaseData(const EntityData& data) const;

  // Returns true if the entity's specifics hash matches its base specifics
  // hash. This is true if the entity is in sync or if the local changes did not
  // modify the specifics (e.g. only metadata changed).
  bool MatchesOwnBaseData() const;

  // Returns true if the entity's specifics hash matches the hash of
  // `specifics`. The entity must not be deleted.
  bool MatchesSpecificsHash(const sync_pb::EntitySpecifics& specifics) const;

  // Returns true if `favicon_png_bytes` matches the tracked favicon hash.
  // The entity must not be deleted.
  bool MatchesFaviconHash(const std::string& favicon_png_bytes) const;

  // Updates the specifics hash in the metadata based on `specifics`.
  void UpdateSpecificsHash(const sync_pb::EntitySpecifics& specifics);

  // Records that a remote update was ignored. Updates server_id and
  // server_version.
  void RecordIgnoredRemoteUpdate(const UpdateResponseData& update);

  // Records that a remote update was accepted. Updates all metadata fields.
  void RecordAcceptedRemoteUpdate(
      const UpdateResponseData& update,
      sync_pb::EntitySpecifics trimmed_specifics,
      std::optional<sync_pb::UniquePosition> unique_position);

  // Records a forced remote update (conflict resolution where server wins).
  // Acks all pending commits and accepts the update.
  void RecordForcedRemoteUpdate(
      const UpdateResponseData& update,
      sync_pb::EntitySpecifics trimmed_specifics,
      std::optional<sync_pb::UniquePosition> unique_position);

  // Records a commit response.
  void RecordCommitResponse(const CommitResponseData& data);

  // Updates the metadata fields (sequence_number, specifics_hash,
  // modification_time, is_deleted, unique_position) for a local update.
  // Used by bookmarks to perform custom local updates.
  // TODO(crbug.com/40823197): Consider removing this method and having
  // bookmarks use RecordLocalUpdate() instead.
  void UpdateMetadataForLocalUpdate(
      const sync_pb::EntitySpecifics& specifics,
      base::Time modification_time,
      std::optional<sync_pb::UniquePosition> unique_position);

  // Performs a local update based on the provided `data`, updating metadata
  // fields.
  void RecordLocalUpdate(
      const EntityData& data,
      sync_pb::EntitySpecifics trimmed_specifics,
      std::optional<sync_pb::UniquePosition> unique_position);

  // Performs a local deletion, updating metadata fields.
  void RecordLocalDeletion(const DeletionOrigin& origin);

  // Sets the possibly trimmed base specifics.
  void SetPossiblyTrimmedBaseSpecifics(sync_pb::EntitySpecifics specifics);

  // Sets the creation time.
  void SetCreationTime(base::Time time);

 private:
  explicit ProcessorEntityMetadata(sync_pb::EntityMetadata metadata);

  sync_pb::EntityMetadata metadata_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_PROCESSOR_ENTITY_METADATA_H_
