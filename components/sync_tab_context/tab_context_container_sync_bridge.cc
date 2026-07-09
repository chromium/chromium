// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_container_sync_bridge.h"

#include <utility>

#include "base/containers/map_util.h"
#include "base/notimplemented.h"
#include "base/uuid.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace sync_tab_context {

TabContextContainerSyncBridge::TabContextContainerSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  this->change_processor()->ModelReadyToSync(
      std::make_unique<syncer::MetadataBatch>());
}

TabContextContainerSyncBridge::~TabContextContainerSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<syncer::MetadataChangeList>
TabContextContainerSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError>
TabContextContainerSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_changes));
}

std::optional<syncer::ModelError>
TabContextContainerSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const base::Uuid uuid =
            base::Uuid::ParseCaseInsensitive(change->storage_key());
        CHECK(uuid.is_valid());
        entries_[uuid] =
            change->data().specifics.encrypted_tab_context_container();
        break;
      }
      case syncer::EntityChange::ACTION_DELETE: {
        const base::Uuid uuid =
            base::Uuid::ParseCaseInsensitive(change->storage_key());
        CHECK(uuid.is_valid());
        entries_.erase(uuid);
        break;
      }
    }
  }
  if (metadata_change_list) {
    metadata_change_list->DropAllChanges();
  }
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
TabContextContainerSyncBridge::GetDataForCommit(StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& key : storage_keys) {
    const base::Uuid uuid = base::Uuid::ParseCaseInsensitive(key);
    if (const sync_pb::EncryptedTabContextContainerSpecifics* specifics =
            base::FindOrNull(entries_, uuid)) {
      auto entity_data = std::make_unique<syncer::EntityData>();
      *entity_data->specifics.mutable_encrypted_tab_context_container() =
          *specifics;
      entity_data->name = key;
      batch->Put(key, std::move(entity_data));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
TabContextContainerSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  // TODO(crbug.com/527991726): Implement debugging data retrieval.
  return nullptr;
}

std::string TabContextContainerSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  // TODO(crbug.com/527991726): Implement GetClientTag.
  return std::string();
}

std::string TabContextContainerSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  // TODO(crbug.com/527991726): Implement GetStorageKey.
  return std::string();
}

sync_pb::EntitySpecifics
TabContextContainerSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_specifics;
}

bool TabContextContainerSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  // TODO(crbug.com/527991726): Implement IsEntityDataValid.
  return true;
}

}  // namespace sync_tab_context
