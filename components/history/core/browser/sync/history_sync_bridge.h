// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_BRIDGE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_BRIDGE_H_

#include <memory>
#include <set>
#include <string>

#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_observer.h"
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
      HistoryBackend* history_backend,
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
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_BRIDGE_H_
