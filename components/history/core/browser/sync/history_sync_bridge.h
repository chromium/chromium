// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_BRIDGE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_BRIDGE_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/history/core/browser/history_backend_observer.h"
#include "components/history/core/browser/sync/history_backend_for_sync.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
class MetadataChangeList;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace history {

class HistorySyncMetadataDatabase;

class HistorySyncBridge : public syncer::ModelTypeSyncBridge,
                          public HistoryBackendObserver {
 public:
  // `sync_metadata_store` is owned by `history_backend`, and must outlive
  // HistorySyncBridge.
  HistorySyncBridge(
      HistoryBackendForSync* history_backend,
      HistorySyncMetadataDatabase* sync_metadata_store,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);

  HistorySyncBridge(const HistorySyncBridge&) = delete;
  HistorySyncBridge& operator=(const HistorySyncBridge&) = delete;

  ~HistorySyncBridge() override;

  // syncer::ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  absl::optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // HistoryBackendObserver:
  void OnURLVisited(HistoryBackend* history_backend,
                    ui::PageTransition transition,
                    const URLRow& row,
                    base::Time visit_time) override;
  void OnURLsModified(HistoryBackend* history_backend,
                      const URLRows& changed_urls,
                      bool is_from_expiration) override;
  void OnURLsDeleted(HistoryBackend* history_backend,
                     bool all_history,
                     bool expired,
                     const URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Called by HistoryBackend when database error is reported through
  // DatabaseErrorCallback.
  void OnDatabaseError();

 private:
  // Synchronously loads sync metadata from the HistorySyncMetadataDatabase and
  // passes it to the processor so that it can start tracking changes.
  void LoadMetadata();

  // Adds visit(s) corresponding to the `specifics` to the HistoryBackend.
  // Returns true on success, or false in case of backend errors.
  bool AddEntityInBackend(const sync_pb::HistorySpecifics& specifics);

  // Updates the visit(s) corresponding to the `specifics` in the
  // HistoryBackend. Returns true on success, or false in case of errors (most
  // commonly, because no matching entry exists in the backend).
  bool UpdateEntityInBackend(const sync_pb::HistorySpecifics& specifics);

  // A non-owning pointer to the backend, which we're syncing local changes from
  // and sync changes to. Never null.
  const raw_ptr<HistoryBackendForSync> history_backend_;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any local url changes, since we triggered them.
  bool processing_syncer_changes_ = false;

  // A non-owning pointer to the database, which is for storing sync metadata
  // and state. Can be null in case of unrecoverable database errors.
  raw_ptr<HistorySyncMetadataDatabase> sync_metadata_database_;

  // HistoryBackend uses SequencedTaskRunner, so this makes sure
  // HistorySyncBridge is used on the correct sequence.
  base::SequenceChecker sequence_checker_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_BRIDGE_H_
