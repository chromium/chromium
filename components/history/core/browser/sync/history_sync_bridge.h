// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_BRIDGE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_BRIDGE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/history/core/browser/history_backend_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/sync/history_backend_for_sync.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/service/sync_service.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
class MetadataChangeList;
}  // namespace syncer

namespace history {

class HistorySyncMetadataDatabase;
class VisitIDRemapper;

class HistorySyncBridge : public syncer::DataTypeSyncBridge,
                          public HistoryBackendObserver {
 public:
  // `history_backend` must not be null.
  // `sync_metadata_store` may be null, but if non-null, must outlive this.
  HistorySyncBridge(
      HistoryBackendForSync* history_backend,
      HistorySyncMetadataDatabase* sync_metadata_store,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  HistorySyncBridge(const HistorySyncBridge&) = delete;
  HistorySyncBridge& operator=(const HistorySyncBridge&) = delete;

  ~HistorySyncBridge() override;

  // syncer::DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  syncer::ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const syncer::EntityData& remote_data) const override;

  // HistoryBackendObserver:
  void OnURLVisited(HistoryBackend* history_backend,
                    const URLRow& url_row,
                    const VisitRow& visit_row) override;
  void OnURLsModified(HistoryBackend* history_backend,
                      const URLRows& changed_urls,
                      bool is_from_expiration) override;
  void OnHistoryDeletions(HistoryBackend* history_backend,
                          bool all_history,
                          bool expired,
                          const URLRows& deleted_rows,
                          const std::set<GURL>& favicon_urls) override;
  void OnVisitUpdated(const VisitRow& visit_row,
                      VisitUpdateReason reason) override;
  void OnVisitDeleted(const VisitRow& visit_row) override;

  void SetSyncTransportState(syncer::SyncService::TransportState state);

  // Called by HistoryBackend when database error is reported through
  // DatabaseErrorCallback.
  void OnDatabaseError();

 private:
  // Synchronously loads sync metadata from the HistorySyncMetadataDatabase and
  // passes it to the processor so that it can start tracking changes.
  void LoadMetadata();

  // Whether local history changes should be sent to Sync ("committed") right
  // now. This includes conditions like Sync being enabled, no error state, etc.
  bool ShouldCommitRightNow() const;

  // Checks various conditions on `visit_row` and the overall Sync state, and if
  // all are fulfilled, sends changes corresponding to the new/updated visit(s)
  // to Sync.
  void MaybeCommit(const VisitRow& visit_row);

  // Queries the redirect chain ending in `final_visit` from the HistoryBackend,
  // and creates the corresponding EntityData(s). Typically returns a single
  // EntityData, but in some cases the redirect chain may have to be split up
  // into multiple entities. May return no entities at all in case of
  // HistoryBackend failure (e.g. corrupted DB). The local visit IDs that are
  // included in the entity data will be appended to `included_visit_ids`, if it
  // is not nullptr.
  std::vector<std::unique_ptr<syncer::EntityData>>
  QueryRedirectChainAndMakeEntityData(const VisitRow& final_visit,
                                      std::vector<VisitID>* included_visit_ids);

  GURL GetURLForVisit(VisitID visit_id);

  // Adds visit(s) corresponding to the `specifics` to the HistoryBackend.
  // Returns true on success, or false in case of backend errors.
  bool AddEntityInBackend(VisitIDRemapper* id_remapper,
                          const sync_pb::HistorySpecifics& specifics);

  // Updates the visit(s) corresponding to the `specifics` in the
  // HistoryBackend. Returns true on success, or false in case of errors (most
  // commonly, because no matching entry exists in the backend).
  bool UpdateEntityInBackend(VisitIDRemapper* id_remapper,
                             const sync_pb::HistorySpecifics& specifics);

  // Untracks all entities from the processor, and clears their (persisted)
  // metadata.
  void UntrackAndClearMetadataForAllEntities();

  // Untracks all entities from the processor, and clears their (persisted)
  // metadata, except for entities that are "unsynced", i.e. that are waiting to
  // be committed.
  void UntrackAndClearMetadataForSyncedEntities();

  // Returns the cache GUID of the Sync client on this device. Must only be
  // called after `change_processor()->IsTrackingMetadata()` returns true
  // (because before that, the cache GUID isn't known).
  std::string GetLocalCacheGuid() const;

  std::unique_ptr<syncer::DataBatch> GetDataImpl(StorageKeyList storage_keys);

  // A non-owning pointer to the backend, which we're syncing local changes from
  // and sync changes to. Never null.
  const raw_ptr<HistoryBackendForSync, DanglingUntriaged> history_backend_;

  // The state of Sync as a whole (not necessarily for this data type). Data
  // will only be sent to the Sync server if this is *not* DISABLED or PAUSED.
  // Initially, the Sync state is not known, but DISABLED is a safe default
  // assumption. This gets set to a proper value early during profile
  // startup.
  syncer::SyncService::TransportState sync_transport_state_ =
      syncer::SyncService::TransportState::DISABLED;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any local url changes, since we triggered them.
  bool processing_syncer_changes_ = false;

  // A non-owning pointer to the database, which is for storing sync metadata
  // and state. Can be null in case of unrecoverable database errors.
  raw_ptr<HistorySyncMetadataDatabase> sync_metadata_database_;

  // HistoryBackend uses SequencedTaskRunner, so this makes sure
  // HistorySyncBridge is used on the correct sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Tracks observed history backend, for receiving updates from history
  // backend.
  base::ScopedObservation<HistoryBackendForSync, HistoryBackendObserver>
      history_backend_observation_{this};
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_BRIDGE_H_
