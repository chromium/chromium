// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_CLIENT_TAG_BASED_DATA_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_CLIENT_TAG_BASED_DATA_TYPE_PROCESSOR_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_stop_metadata_fate.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/data_type_processor.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/processor_entity_tracker.h"

namespace sync_pb {
class DataTypeState;
class DataTypeState_Invalidation;
}  // namespace sync_pb

namespace syncer {

class CommitQueue;

// A sync component embedded on the data type's sequence that tracks entity
// metadata in the model store and coordinates communication between sync and
// data type sequences. All changes in flight (either incoming from the server
// or local changes reported by the bridge) must specify a client tag.
// Lives on the model sequence.
//
// See
// //docs/website/site/developers/design-documents/sync/client-tag-based-data-type-processor/index.md
// for a more thorough description.
class ClientTagBasedDataTypeProcessor : public DataTypeProcessor,
                                        public DataTypeLocalChangeProcessor,
                                        public DataTypeControllerDelegate {
 public:
  ClientTagBasedDataTypeProcessor(DataType type,
                                  const base::RepeatingClosure& dump_stack);

  ClientTagBasedDataTypeProcessor(const ClientTagBasedDataTypeProcessor&) =
      delete;
  ClientTagBasedDataTypeProcessor& operator=(
      const ClientTagBasedDataTypeProcessor&) = delete;

  ~ClientTagBasedDataTypeProcessor() override;

  // Returns true if the handshake with sync sequence is complete.
  bool IsConnected() const;

  // DataTypeLocalChangeProcessor implementation.
  void Put(const std::string& storage_key,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override;
  void Delete(const std::string& storage_key,
              const DeletionOrigin& origin,
              MetadataChangeList* metadata_change_list) override;
  void UpdateStorageKey(const EntityData& entity_data,
                        const std::string& storage_key,
                        MetadataChangeList* metadata_change_list) override;
  void UntrackEntityForStorageKey(const std::string& storage_key) override;
  void UntrackEntityForClientTagHash(
      const ClientTagHash& client_tag_hash) override;
  std::vector<std::string> GetAllTrackedStorageKeys() const override;
  bool IsEntityUnsynced(const std::string& storage_key) const override;
  base::Time GetEntityCreationTime(
      const std::string& storage_key) const override;
  base::Time GetEntityModificationTime(
      const std::string& storage_key) const override;
  void OnModelStarting(DataTypeSyncBridge* bridge) override;
  void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) override;
  bool IsTrackingMetadata() const override;
  std::string TrackedAccountId() const override;
  std::string TrackedCacheGuid() const override;
  void ReportError(const ModelError& error) override;
  std::optional<ModelError> GetError() const override;
  base::WeakPtr<DataTypeControllerDelegate> GetControllerDelegate() override;
  const sync_pb::EntitySpecifics& GetPossiblyTrimmedRemoteSpecifics(
      const std::string& storage_key) const override;
  sync_pb::UniquePosition UniquePositionAfter(
      const std::string& storage_key_before,
      const ClientTagHash& target_client_tag_hash) const override;
  sync_pb::UniquePosition UniquePositionBefore(
      const std::string& storage_key_after,
      const ClientTagHash& target_client_tag_hash) const override;
  sync_pb::UniquePosition UniquePositionBetween(
      const std::string& storage_key_before,
      const std::string& storage_key_after,
      const ClientTagHash& target_client_tag_hash) const override;
  sync_pb::UniquePosition UniquePositionForInitialEntity(
      const ClientTagHash& target_client_tag_hash) const override;
  sync_pb::UniquePosition GetUniquePositionForStorageKey(
      const std::string& storage_key) const override;
  base::WeakPtr<DataTypeLocalChangeProcessor> GetWeakPtr() override;

  // DataTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       GetLocalChangesCallback callback) override;
  void OnCommitCompleted(
      const sync_pb::DataTypeState& type_state,
      const CommitResponseDataList& committed_response_list,
      const FailedCommitResponseDataList& error_response_list) override;
  void OnCommitFailed(SyncCommitError commit_error) override;
  void OnUpdateReceived(
      const sync_pb::DataTypeState& type_state,
      UpdateResponseDataList updates,
      std::optional<sync_pb::GarbageCollectionDirective> gc_directive) override;
  void StorePendingInvalidations(
      std::vector<sync_pb::DataTypeState_Invalidation> invalidations_to_store)
      override;

  // DataTypeControllerDelegate implementation.
  // |start_callback| will never be called synchronously.
  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override;
  void HasUnsyncedData(base::OnceCallback<void(bool)> callback) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const TypeEntitiesCount&)> callback)
      const override;
  void RecordMemoryUsageAndCountsHistograms() override;
  void ClearMetadataIfStopped() override;
  void ReportBridgeErrorForTest() override;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  bool HasLocalChangesForTest() const;

  bool IsTrackingEntityForTest(const std::string& storage_key) const;

  bool IsModelReadyToSyncForTest() const;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Public for tests.
  // LINT.IfChange(SyncDataTypeErrorSite)
  enum class ErrorSite {
    kReportedByBridge = 0,
    kApplyFullUpdates = 1,
    kApplyIncrementalUpdates = 2,
    kApplyUpdatesOnCommitResponse = 3,
    kSupportsIncrementalUpdatesMismatch = 4,
    kMaxValue = kSupportsIncrementalUpdatesMismatch,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncDataTypeErrorSite)

 private:
  friend class DataTypeDebugInfo;
  friend class ClientTagBasedDataTypeProcessorTest;

  // Directs the bridge to clear the persisted metadata as known to the entity
  // tracker / `metadata_map`. In addition, it resets the state of the processor
  // incl. the entity tracker.
  void ClearAllTrackedMetadataAndResetState();
  void ClearAllProvidedMetadataAndResetState(
      const EntityMetadataMap& metadata_map);
  // Implementation for the functions above, `change_list` is assumed to delete
  // all known metadata.
  void ClearAllMetadataAndResetStateImpl(
      std::unique_ptr<MetadataChangeList> change_list);

  // Whether the processor is allowing changes to its data type. If this is
  // false, the bridge should not allow any changes to its data.
  bool IsAllowingChanges() const;

  // If preconditions are met, inform sync that we are ready to connect.
  void ConnectIfReady();

  // Validates the update specified by the input parameters and returns whether
  // it should get further processed. If the update is incorrect, this function
  // also reports an error.
  bool ValidateUpdate(
      const sync_pb::DataTypeState& data_type_state,
      const UpdateResponseDataList& updates,
      const std::optional<sync_pb::GarbageCollectionDirective>& gc_directive);

  // Handle the first update received from the server after being enabled. If
  // the data type does not support incremental updates, this will be called for
  // any server update.
  std::optional<ModelError> OnFullUpdateReceived(
      const sync_pb::DataTypeState& type_state,
      UpdateResponseDataList updates,
      std::optional<sync_pb::GarbageCollectionDirective> gc_directive);

  // Handle any incremental updates received from the server after being
  // enabled.
  std::optional<ModelError> OnIncrementalUpdateReceived(
      const sync_pb::DataTypeState& type_state,
      UpdateResponseDataList updates,
      std::optional<sync_pb::GarbageCollectionDirective> gc_directive);

  // Caches EntityData from the |data_batch| in the entity and checks
  // that every entity in |storage_keys_to_load| was successfully loaded (or is
  // not tracked by the processor any more). Reports failed checks to UMA.
  void ConsumeDataBatch(std::unordered_set<std::string> storage_keys_to_load,
                        std::unique_ptr<DataBatch> data_batch);

  // Prepares Commit requests and passes them to the GetLocalChanges callback.
  void CommitLocalChanges(size_t max_entries, GetLocalChangesCallback callback);

  // Nudges worker if there are any local entities to be committed.
  void NudgeForCommitIfNeeded();

  // Looks up the client tag hash for the given |storage_key|, and regenerates
  // with |data| if the lookup finds nothing. Does not update the storage key to
  // client tag hash mapping.
  ClientTagHash GetClientTagHash(const std::string& storage_key,
                                 const EntityData& data) const;

  // Removes metadata for all entries unless they are unsynced.
  // This is used to limit the amount of data stored in sync, and this does not
  // tell the bridge to delete the actual data.
  void ExpireAllEntries(MetadataChangeList* metadata_changes);

  // Removes entity with specified |storage_key| and clears metadata for it from
  // |metadata_change_list|. |storage_key| must not be empty.
  void RemoveEntity(const std::string& storage_key,
                    MetadataChangeList* metadata_change_list);

  // Resets the internal state of the processor to the one right after calling
  // the ctor (with the exception of |bridge_| which remains intact).
  // TODO(jkrcal): Replace the helper function by grouping the state naturally
  // into a few structs / nested classes so that the state can be reset by
  // resetting these structs.
  void ResetState(SyncStopMetadataFate metadata_fate);

  // Adds metadata to all data returned by the bridge.
  // TODO(jkrcal): Mark as const (together with functions it depends on such as
  // GetEntityForStorageKey, GetEntityForTagHash and maybe more).
  void MergeDataWithMetadataForDebugging(AllNodesCallback callback,
                                         std::unique_ptr<DataBatch> batch);

  // Verifies that the persisted DataTypeState (in `entity_tracker_`) is valid.
  // May modify the state (incl. the persisted data) or even clear it entirely
  // if it is invalid.
  void ClearPersistedMetadataIfInconsistentWithActivationRequest();

  // Verifies that the passed-in metadata (DataTypeState plus entity metadata)
  // is valid, and clears it (incl. the persisted data) if not. Returns whether
  // the metadata was cleared.
  bool ClearPersistedMetadataIfInvalid(const MetadataBatch& metadata);

  // Reports error and records a metric about |site| where the error occurred.
  void ReportErrorImpl(const ModelError& error, ErrorSite site);

  // Generates some consistent unique position on best effort if it can't be
  // calculated. Unique positions are stored in sync metadata and loaded from
  // the disk on browser startup, so they should not be CHECKed for validness.
  sync_pb::UniquePosition GenerateFallbackUniquePosition(
      const ClientTagHash& client_tag_hash) const;

  /////////////////////
  // Processor state //
  /////////////////////

  // The data type this object syncs.
  const DataType type_;

  // DataTypeSyncBridge linked to this processor. The bridge owns this
  // processor instance so the pointer should never become invalid.
  raw_ptr<DataTypeSyncBridge, DanglingUntriaged> bridge_ = nullptr;

  // Function to capture and upload a stack trace when an error occurs.
  const base::RepeatingClosure dump_stack_;

  // Whether there is an ongoing processing of incoming updates, used to detect
  // local updates based on remote changes.
  bool processing_incremental_updates_ = false;

  /////////////////
  // Model state //
  /////////////////

  // The first model error that occurred, if any. Stored to track model state
  // and so it can be passed to sync if it happened prior to sync being ready.
  std::optional<ModelError> model_error_;

  // Whether the model has initialized its internal state for sync (and provided
  // metadata).
  bool model_ready_to_sync_ = false;

  // Marks whether metadata should be cleared upon ModelReadyToSync(). True if
  // ClearMetadataIfStopped() is called before ModelReadyToSync().
  bool pending_clear_metadata_ = false;

  ////////////////
  // Sync state //
  ////////////////

  // Stores the start callback in between OnSyncStarting() and ReadyToConnect().
  // |start_callback_| will never be called synchronously.
  StartCallback start_callback_;

  // The request context passed in as part of OnSyncStarting().
  DataTypeActivationRequest activation_request_;

  // Reference to the CommitQueue. Note that in practice, this is typically a
  // proxy object to the actual CommitQueue implementation (aka the worker),
  // which lives on the sync sequence.
  std::unique_ptr<CommitQueue> worker_;

  //////////////////
  // Entity state //
  //////////////////

  std::unique_ptr<ProcessorEntityTracker> entity_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  // WeakPtrFactory for this processor for DataTypeController (only gets
  // invalidated during destruction).
  base::WeakPtrFactory<ClientTagBasedDataTypeProcessor>
      weak_ptr_factory_for_controller_{this};

  // WeakPtrFactory for this processor which will be sent to sync sequence.
  base::WeakPtrFactory<ClientTagBasedDataTypeProcessor>
      weak_ptr_factory_for_worker_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_CLIENT_TAG_BASED_DATA_TYPE_PROCESSOR_H_
