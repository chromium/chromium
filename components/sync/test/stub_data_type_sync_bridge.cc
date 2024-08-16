// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/stub_data_type_sync_bridge.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/sync/model/metadata_change_list.h"

namespace syncer {

StubDataTypeSyncBridge::StubDataTypeSyncBridge(
    std::unique_ptr<DataTypeLocalChangeProcessor> change_processor)
    : DataTypeSyncBridge(std::move(change_processor)) {}

StubDataTypeSyncBridge::~StubDataTypeSyncBridge() = default;

std::unique_ptr<MetadataChangeList>
StubDataTypeSyncBridge::CreateMetadataChangeList() {
  return nullptr;
}

std::optional<ModelError> StubDataTypeSyncBridge::MergeFullSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  return {};
}

std::optional<ModelError> StubDataTypeSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  return {};
}

std::unique_ptr<DataBatch> StubDataTypeSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  return nullptr;
}

std::unique_ptr<DataBatch> StubDataTypeSyncBridge::GetAllDataForDebugging() {
  return nullptr;
}

std::string StubDataTypeSyncBridge::GetClientTag(
    const EntityData& entity_data) {
  return std::string();
}

std::string StubDataTypeSyncBridge::GetStorageKey(
    const EntityData& entity_data) {
  return std::string();
}

}  // namespace syncer
