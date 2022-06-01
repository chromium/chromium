// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_sync_bridge.h"

#include "components/sync/model/metadata_change_list.h"

namespace history {

HistorySyncBridge::HistorySyncBridge(
    HistoryBackend* history_backend,
    HistorySyncMetadataDatabase* sync_metadata_database,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)) {
  NOTIMPLEMENTED();
}

HistorySyncBridge::~HistorySyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
HistorySyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return {};
}

absl::optional<syncer::ModelError> HistorySyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return {};
}

absl::optional<syncer::ModelError> HistorySyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return {};
}

void HistorySyncBridge::GetData(StorageKeyList storage_keys,
                                DataCallback callback) {
  NOTIMPLEMENTED();
}

void HistorySyncBridge::GetAllDataForDebugging(DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string HistorySyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return {};
}

std::string HistorySyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return {};
}

void HistorySyncBridge::OnURLVisited(HistoryBackend* history_backend,
                                     ui::PageTransition transition,
                                     const URLRow& row,
                                     base::Time visit_time) {
  NOTIMPLEMENTED();
}

void HistorySyncBridge::OnURLsModified(HistoryBackend* history_backend,
                                       const URLRows& changed_urls,
                                       bool is_from_expiration) {
  NOTIMPLEMENTED();
}

void HistorySyncBridge::OnURLsDeleted(HistoryBackend* history_backend,
                                      bool all_history,
                                      bool expired,
                                      const URLRows& deleted_rows,
                                      const std::set<GURL>& favicon_urls) {
  NOTIMPLEMENTED();
}

void HistorySyncBridge::OnDatabaseError() {
  NOTIMPLEMENTED();
}

}  // namespace history
