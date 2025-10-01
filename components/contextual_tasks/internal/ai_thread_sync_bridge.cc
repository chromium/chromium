// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/ai_thread_sync_bridge.h"

namespace contextual_tasks {

AiThreadSyncBridge::AiThreadSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : DataTypeSyncBridge(std::move(change_processor)) {}

AiThreadSyncBridge::~AiThreadSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
AiThreadSyncBridge::CreateMetadataChangeList() {
  return nullptr;
}

std::optional<syncer::ModelError> AiThreadSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_change_list) {
  return std::nullopt;
}

std::optional<syncer::ModelError>
AiThreadSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> AiThreadSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  return nullptr;
}

std::unique_ptr<syncer::DataBatch>
AiThreadSyncBridge::GetAllDataForDebugging() {
  return nullptr;
}

std::string AiThreadSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return std::string();
}

std::string AiThreadSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return std::string();
}

void AiThreadSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {}

bool AiThreadSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return false;
}

void AiThreadSyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AiThreadSyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace contextual_tasks
