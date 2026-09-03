// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_SYNC_BRIDGE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/history/core/browser/history_backend_observer.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "url/gurl.h"

namespace history {
class HistoryBackend;
}  // namespace history

namespace history::journeys {

class HistoryBackendForJourneysSync;
class JourneysSyncMetadataDatabase;

// DataTypeSyncBridge implementation for JOURNEY sync data.
class JourneysSyncBridge : public syncer::DataTypeSyncBridge,
                           public HistoryBackendObserver {
 public:
  // `backend` must not be null.
  // `sync_metadata_database` may be null, but if non-null, must outlive this.
  JourneysSyncBridge(
      HistoryBackendForJourneysSync* backend,
      JourneysSyncMetadataDatabase* sync_metadata_database,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  JourneysSyncBridge(const JourneysSyncBridge&) = delete;
  JourneysSyncBridge& operator=(const JourneysSyncBridge&) = delete;

  ~JourneysSyncBridge() override;

  // syncer::DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

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
  void OnVisitUpdated(const VisitRow& visit, VisitUpdateReason reason) override;
  void OnVisitDeleted(const VisitRow& visit) override;

  // Called when the database encounters an error.
  void OnDatabaseError();

 private:
  void LoadMetadata();

  // Untracks all entities from the processor, and clears their (persisted)
  // metadata.
  void UntrackAndClearMetadataForAllEntities();

  const raw_ref<HistoryBackendForJourneysSync> backend_;
  raw_ptr<JourneysSyncMetadataDatabase> sync_metadata_database_;
  base::ScopedObservation<HistoryBackendForJourneysSync, HistoryBackendObserver>
      history_backend_observation_{this};
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace history::journeys

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_SYNC_BRIDGE_H_
