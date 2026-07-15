// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_item_sync_bridge.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/uuid.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync_tab_context/container_id.h"

namespace sync_tab_context {
namespace {

std::string GetClientTag(const ContainerId& container_id,
                         const std::string& item_id) {
  return base::StrCat({container_id.value().AsLowercaseString(), ":", item_id});
}

}  // namespace

TabContextItemSyncBridge::TabContextItemSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  this->change_processor()->ModelReadyToSync(
      std::make_unique<syncer::MetadataBatch>());
}

TabContextItemSyncBridge::~TabContextItemSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool TabContextItemSyncBridge::UploadItem(
    const ContainerId& container_id,
    const std::string& item_id,
    sync_pb::EncryptedData encrypted_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(container_id.value().is_valid());
  CHECK(!item_id.empty());
  if (!change_processor()->IsTrackingMetadata()) {
    return false;
  }

  sync_pb::EncryptedTabContextItemSpecifics specifics;
  specifics.set_container_id(container_id.value().AsLowercaseString());
  specifics.set_item_id(item_id);
  *specifics.mutable_encrypted_data() = std::move(encrypted_data);

  const std::string storage_key =
      sync_tab_context::GetClientTag(container_id, item_id);

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_encrypted_tab_context_item() =
      std::move(specifics);
  entity_data->name = storage_key;

  change_processor()->Put(storage_key, std::move(entity_data),
                          metadata_change_list.get());
  return true;
}

std::unique_ptr<syncer::MetadataChangeList>
TabContextItemSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> TabContextItemSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_changes));
}

std::optional<syncer::ModelError>
TabContextItemSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (metadata_change_list) {
    metadata_change_list->DropAllChanges();
  }
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> TabContextItemSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This being a commit-only datatype without even tracking in-flight changes
  // (and definitely no persistence) at the bridge level means there is no data
  // to return. This code should be usually unreachable, except for edge cases
  // like the browser having been restarted while in-flight changes existed. In
  // such scenarios, such in-flight changes will be lost.
  return std::make_unique<syncer::MutableDataBatch>();
}

std::unique_ptr<syncer::DataBatch>
TabContextItemSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This being a commit-only datatype without even tracking in-flight changes
  // at the bridge level means there are no entities to return.
  return std::make_unique<syncer::MutableDataBatch>();
}

std::string TabContextItemSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const sync_pb::EncryptedTabContextItemSpecifics& specifics =
      entity_data.specifics.encrypted_tab_context_item();
  return sync_tab_context::GetClientTag(
      ContainerId(base::Uuid::ParseCaseInsensitive(specifics.container_id())),
      specifics.item_id());
}

std::string TabContextItemSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClientTag(entity_data);
}

sync_pb::EntitySpecifics
TabContextItemSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_pb::EntitySpecifics();
}

bool TabContextItemSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const sync_pb::EncryptedTabContextItemSpecifics& specifics =
      entity_data.specifics.encrypted_tab_context_item();
  return base::Uuid::ParseCaseInsensitive(specifics.container_id())
             .is_valid() &&
         !specifics.item_id().empty() && specifics.has_encrypted_data();
}

}  // namespace sync_tab_context
