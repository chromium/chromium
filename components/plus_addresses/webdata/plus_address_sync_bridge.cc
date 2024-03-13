// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_sync_bridge.h"

#include <memory>
#include <optional>

#include "base/notreached.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/webdata/common/web_database_backend.h"

namespace plus_addresses {

PlusAddressSyncBridge::PlusAddressSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    scoped_refptr<WebDatabaseBackend> db_backend)
    : ModelTypeSyncBridge(std::move(change_processor)),
      db_backend_(std::move(db_backend)) {}

PlusAddressSyncBridge::~PlusAddressSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
PlusAddressSyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
}

std::optional<syncer::ModelError> PlusAddressSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<syncer::ModelError>
PlusAddressSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void PlusAddressSyncBridge::GetData(StorageKeyList storage_keys,
                                    DataCallback callback) {
  NOTIMPLEMENTED();
}

void PlusAddressSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string PlusAddressSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

std::string PlusAddressSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

}  // namespace plus_addresses
