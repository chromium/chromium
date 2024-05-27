// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/settings/plus_address_setting_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/notreached.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store.h"

namespace plus_addresses {

PlusAddressSettingSyncBridge::PlusAddressSettingSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory store_factory)
    : ModelTypeSyncBridge(std::move(change_processor)) {}

std::unique_ptr<syncer::MetadataChangeList>
PlusAddressSettingSyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
}

std::optional<syncer::ModelError>
PlusAddressSettingSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<syncer::ModelError>
PlusAddressSettingSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void PlusAddressSettingSyncBridge::GetData(StorageKeyList storage_keys,
                                           DataCallback callback) {
  NOTIMPLEMENTED();
}

void PlusAddressSettingSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string PlusAddressSettingSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

std::string PlusAddressSettingSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

}  // namespace plus_addresses
