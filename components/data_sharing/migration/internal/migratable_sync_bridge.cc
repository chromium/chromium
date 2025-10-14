// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/migration/public/migratable_sync_bridge.h"

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/metadata_change_list.h"

namespace data_sharing::migration {

MigratableSyncBridge::MigratableSyncBridge(
    syncer::DataType data_type,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    PrefService* pref_service,
    bool is_shared_bridge)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      pref_service_(pref_service),
      is_shared_bridge_(is_shared_bridge) {
  CHECK(pref_service_);
  std::move(create_store_callback)
      .Run(data_type, base::BindOnce(&MigratableSyncBridge::OnStoreCreated,
                                     weak_ptr_factory_.GetWeakPtr()));
}

MigratableSyncBridge::~MigratableSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MigratableSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  store_ = std::move(store);
  OnStoreCreatedAndReady();
}

std::optional<syncer::ModelError> MigratableSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Delegate the core logic to the feature-specific implementation.
  return ProcessSyncChanges(std::move(metadata_change_list),
                            std::move(entity_data));
}

std::optional<syncer::ModelError>
MigratableSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Delegate the core logic to the feature-specific implementation.
  return ProcessSyncChanges(std::move(metadata_change_list),
                            std::move(entity_changes));
}

void MigratableSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

}  // namespace data_sharing::migration
