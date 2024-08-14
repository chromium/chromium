// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/entity_change.h"

namespace sync_pb {
class EntitySpecifics;
class UniquePosition;
}  // namespace sync_pb

namespace syncer {

enum class ConflictResolution;
class DataBatch;
struct DataTypeActivationRequest;
struct EntityData;
class MetadataChangeList;
class ModelError;

// Interface implemented by data types to receive updates from sync via a
// DataTypeLocalChangeProcessor. Provides a way for sync to update the data and
// metadata for entities, as well as the data type state. Sync bridge
// implementations must provide their change_processor() with metadata through
// ModelReadyToSync() as soon as possible. Once this is called, sync will
// immediately begin locally tracking changes and can start syncing with the
// server soon afterward. If an error occurs during startup, the processor's
// ReportError() method should be called instead of ModelReadyToSync().
// Lives on the model sequence.
class DataTypeSyncBridge {
 public:
  using DataCallback = base::OnceCallback<void(std::unique_ptr<DataBatch>)>;
  using StorageKeyList = std::vector<std::string>;

  // When commit fails, all entities from the commit request are in transient
  // state. This state is normally (by default) kept until the next successful
  // commit of the data type or until the change processor is disconnected.
  // However, the bridge can return the preferred option to reset state of all
  // entities from the last failed commit.
  enum class CommitAttemptFailedBehavior {
    // Keep transient state of entities from the last commit request.
    kDontRetryOnNextCycle,

    // The processor should reset the transient state and include all entities
    // into the next sync cycle.
    kShouldRetryOnNextCycle,
  };

  explicit DataTypeSyncBridge(
      std::unique_ptr<DataTypeLocalChangeProcessor> change_processor);

  virtual ~DataTypeSyncBridge();

  // Called by the processor as a notification that sync has been started by the
  // DataTypeController.
  virtual void OnSyncStarting(const DataTypeActivationRequest& request);

  // Creates an object used to communicate changes in the sync metadata to the
  // data type store.
  virtual std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() = 0;

  // Perform the initial merge between local and sync data.
  //
  // If the bridge supports incremental updates, this is only called when a data
  // type is first enabled to start syncing, and there is no sync metadata.
  // In this case, best effort should be made to match local and sync data.
  //
  // For datatypes that do not support incremental updates, the processor will
  // call this method every time it gets new sync data from the server. It is
  // then the responsibility of the bridge to clear all existing sync data, and
  // replace it with the passed in `entity_data`.
  //
  // Storage key in entity_data elements will be set to result of
  // GetStorageKey() call if the bridge supports it. Otherwise it will be left
  // empty, bridge is responsible for updating storage keys of new entities with
  // change_processor()->UpdateStorageKey() in this case.
  //
  // If a local and sync data should match/merge but disagree on storage key,
  // the bridge should delete one of the records (preferably local). Any local
  // pieces of data that are not present in sync should immediately be Put(...)
  // to the processor before returning. The same MetadataChangeList that was
  // passed into this function can be passed to Put(...) calls. Delete(...) can
  // also be called but should not be needed for most data types. Durable
  // storage writes, if not able to combine all change atomically, should save
  // the metadata after the data changes, so that this merge will be re-driven
  // by sync if is not completely saved during the current run.
  virtual std::optional<ModelError> MergeFullSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_data) = 0;

  // Apply changes from the sync server locally.
  // Please note that `entity_changes` might have fewer entries than
  // `metadata_change_list` in case when some of the data changes are filtered
  // out, or even be empty in case when a commit confirmation is processed and
  // only the metadata needs to persisted.
  virtual std::optional<ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) = 0;

  // Retrieves the corresponding sync data for `storage_keys`. This must return
  // a non-null result, except if a model error occurred, in which case the
  // processor's ReportError() method must be called.
  // Used only to commit the data.
  virtual std::unique_ptr<DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) = 0;

  // Retrieves all of the local sync data. In case of errors, the processor's
  // ReportError method should be called, and this should return nullptr.
  // Used for getting all data in Sync Node Browser of chrome://sync-internals.
  virtual std::unique_ptr<DataBatch> GetAllDataForDebugging() = 0;

  // Must not be called unless SupportsGetClientTag() returns true.
  //
  // Get or generate a client tag for `entity_data`. This must be the same tag
  // that was/would have been generated in the SyncableService/Directory world
  // for backward compatibility with pre-USS clients. The only time this
  // theoretically needs to be called is on the creation of local data.
  //
  // If a data type was never launched pre-USS, then method does not need to be
  // different from GetStorageKey(). Only the hash of this value is kept.
  //
  // IsEntityDataValid() is guaranteed to hold for the `entity_data`.
  virtual std::string GetClientTag(const EntityData& entity_data) = 0;

  // Must not be called unless SupportsGetStorageKey() returns true.
  //
  // Get or generate a storage key for `entity_data`. This will only ever be
  // called once when first encountering a remote entity. Local changes will
  // provide their storage keys directly to Put instead of using this method.
  // Theoretically this function doesn't need to be stable across multiple calls
  // on the same or different clients, but to keep things simple, it probably
  // should be. Storage keys are kept in memory at steady state, so each model
  // type should strive to keep these keys as small as possible.
  // Returning an empty string means the remote creation should be ignored (i.e.
  // it contains invalid data).
  //
  // IsEntityDataValid() is guaranteed to hold for the `entity_data`.
  virtual std::string GetStorageKey(const EntityData& entity_data) = 0;

  // Whether or not the bridge is capable of producing a client tag from
  // `EntityData` (usually remote changes), via GetClientTag(). Most bridges do,
  // but in rare cases including commit-only types and read-only types, it may
  // not.
  virtual bool SupportsGetClientTag() const;

  // By returning true in this function datatype indicates that it can generate
  // storage key from EntityData. In this case for all new entities received
  // from server, change processor will call GetStorageKey and update
  // EntityChange structures before passing them to MergeFullSyncData and
  // ApplyIncrementalSyncChanges.
  //
  // This function should return false when datatype's native storage is not
  // indexed by some combination of values from EntityData, when key into the
  // storage is obtained at the time the record is inserted into it (e.g. ROWID
  // in SQLite). In this case entity changes for new entities passed to
  // MergeFullSyncData and ApplyIncrementalSyncChanges will have empty
  // storage_key. It is datatype's responsibility to call UpdateStorageKey for
  // such entities.
  virtual bool SupportsGetStorageKey() const;

  // By returning true in this function, the datatype indicates that it supports
  // receiving partial (incremental) updates. If it returns false, the type
  // indicates that it requires the full data set to be sent to it through
  // MergeFullSyncData for any change to the data set.
  virtual bool SupportsIncrementalUpdates() const;

  // By returning true in this function, the datatype indicates that it supports
  // sync_pb.UniquePosition for ordering. The bridge should use helper methods
  // from the change processor to generate such positions and implement
  // GetUniquePosition() method so the processor would be able to extract the
  // unique position from specifics. Dealing with unique positions is a rather
  // advanced feature, by default the method returns false.
  virtual bool SupportsUniquePositions() const;

  // Extract unique position from specifics, called for non-deletions only.
  // Unique position is supposed to be stored in specifics (and hence, generated
  // before the Put() call) but locally it's stored in sync metadata by the
  // processor. Returns the default proto if the entity does not have / require
  // unique position.
  virtual sync_pb::UniquePosition GetUniquePosition(
      const sync_pb::EntitySpecifics& specifics) const;

  // Resolve a conflict between the client and server versions of data. They are
  // guaranteed not to match (both be deleted or have identical specifics). A
  // default implementation chooses the server data unless it is a deletion.
  virtual ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const EntityData& remote_data) const;

  // Similar to ApplyIncrementalSyncChanges() but called by the processor when
  // sync is in the process of being disabled and metadata needs to cleared.
  // `delete_metadata_change_list` must not be null, and contains a change list
  // to remove all metadata that the processor knows about (the bridge may
  // decide to implement deletion by other means).
  virtual void ApplyDisableSyncChanges(
      std::unique_ptr<MetadataChangeList> delete_metadata_change_list);

  // Invoked when the datatype is stopped without clearing metadata. This is
  // usually interesting for bridges that have advanced logic in
  // OnSyncStarting().
  virtual void OnSyncPaused();

  // Called only when some items in a commit haven't been committed due to an
  // error.
  virtual void OnCommitAttemptErrors(
      const syncer::FailedCommitResponseDataList& error_response_list);

  // Called only when a commit failed due to server error.
  virtual CommitAttemptFailedBehavior OnCommitAttemptFailed(
      SyncCommitError commit_error);

  // Returns an estimate of memory usage attributed to sync (that is, excludes
  // the actual model). Because the resulting UMA metrics are often used to
  // compare with the non-USS equivalent implementations (SyncableService), it's
  // a good idea to account for overhead that would also get accounted for the
  // SyncableService by other means.
  virtual size_t EstimateSyncOverheadMemoryUsage() const;

  // Returns a copy of `entity_specifics` with fields that need to be preserved,
  // resulting in caching them in EntityMetadata and allowing to use them on
  // commits to the Sync server in order to prevent the data loss.
  // This means that a data-specific bridge must override this function with the
  // implementation that clears all supported proto fields (i.e. fields that are
  // actively used by the implementation and fully launched).
  // Fields that should not be marked as supported (cleared) include:
  // * Unknown fields in the current browser version
  // * Known fields that are just defined in the proto and not actively used
  // (e.g. a partially-implemented functionality or a functionality guarded by a
  // feature toggle).
  // TODO(crbug.com/40253395): Consider changing the default to preserve unknown
  // fields at least.
  // By default, empty EntitySpecifics is returned.
  virtual sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const;

  // Returns true if the provided `entity_data` is valid. This method should be
  // implemented by the bridges and can be used to validate the incoming remote
  // updates.
  // TODO(crbug.com/40677711): Mark this method as pure virtual to force all the
  // bridges to implement this.
  virtual bool IsEntityDataValid(const EntityData& entity_data) const;

  // Needs to be informed about any model change occurring via Delete() and
  // Put(). The changing metadata should be stored to persistent storage
  // before or atomically with the model changes.
  DataTypeLocalChangeProcessor* change_processor();
  const DataTypeLocalChangeProcessor* change_processor() const;

 private:
  std::unique_ptr<DataTypeLocalChangeProcessor> change_processor_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_SYNC_BRIDGE_H_
