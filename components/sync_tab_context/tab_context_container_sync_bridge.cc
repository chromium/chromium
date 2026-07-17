// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_container_sync_bridge.h"

#include <utility>

#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/uuid.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync_tab_context/container_id.h"

namespace sync_tab_context {

namespace {

sync_pb::EncryptedTabContextContainerSpecifics ToSpecifics(
    const ContainerId& container_id,
    const syncer::AgileSymmetricKeySet& key_set) {
  sync_pb::EncryptedTabContextContainerSpecifics specifics;
  specifics.set_uuid(container_id.value().AsLowercaseString());
  *specifics.mutable_encryption_key() = key_set.ToProto();
  return specifics;
}

bool AreSpecificsValid(
    const sync_pb::EncryptedTabContextContainerSpecifics& specifics) {
  if (!base::Uuid::ParseCaseInsensitive(specifics.uuid()).is_valid()) {
    return false;
  }

  std::unique_ptr<syncer::AgileSymmetricKeySet> encryption_key =
      syncer::AgileSymmetricKeySet::FromProto(specifics.encryption_key());
  return encryption_key && encryption_key->size() != 0;
}

}  // namespace

TabContextContainerSyncBridge::TabContextContainerSyncBridge(
    syncer::OnceDataTypeStoreFactory store_factory,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  std::move(store_factory)
      .Run(syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER,
           base::BindOnce(&TabContextContainerSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

TabContextContainerSyncBridge::~TabContextContainerSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::optional<ContainerId> TabContextContainerSyncBridge::CreateContainer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!change_processor()->IsTrackingMetadata()) {
    return std::nullopt;
  }

  CHECK(store_);

  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set =
      syncer::AgileSymmetricKeySet::CreateEmpty();
  key_set->RotatePrimaryToNewlyGeneratedRandomKey();

  sync_pb::EncryptedTabContextContainerSpecifics specifics =
      ToSpecifics(container_id, *key_set);
  const std::string storage_key = container_id.value().AsLowercaseString();

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  write_batch->WriteData(storage_key, specifics.SerializeAsString());

  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_encrypted_tab_context_container() = specifics;
  entity_data->name = storage_key;

  change_processor()->Put(storage_key, std::move(entity_data),
                          write_batch->GetMetadataChangeList());

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&TabContextContainerSyncBridge::OnStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));

  entries_[container_id] = std::move(key_set);
  return container_id;
}

const syncer::AgileSymmetricKeySet*
TabContextContainerSyncBridge::GetEncryptionKeyForContainer(
    const ContainerId& container_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FindPtrOrNull(entries_, container_id);
}

bool TabContextContainerSyncBridge::IsTrackingMetadataForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return change_processor()->IsTrackingMetadata();
}

std::unique_ptr<syncer::MetadataChangeList>
TabContextContainerSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
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
  CHECK(store_);
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch(std::move(metadata_change_list));

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const sync_pb::EncryptedTabContextContainerSpecifics& specifics =
            change->data().specifics.encrypted_tab_context_container();
        CHECK(AreSpecificsValid(specifics));
        CHECK_EQ(specifics.uuid(), change->storage_key());

        const ContainerId container_id(
            base::Uuid::ParseCaseInsensitive(specifics.uuid()));
        std::unique_ptr<syncer::AgileSymmetricKeySet> encryption_key =
            syncer::AgileSymmetricKeySet::FromProto(specifics.encryption_key());
        write_batch->WriteData(specifics.uuid(), specifics.SerializeAsString());
        entries_[container_id] = std::move(encryption_key);
        break;
      }
      case syncer::EntityChange::ACTION_DELETE: {
        const ContainerId container_id(
            base::Uuid::ParseCaseInsensitive(change->storage_key()));
        write_batch->DeleteData(change->storage_key());
        entries_.erase(container_id);
        break;
      }
    }
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&TabContextContainerSyncBridge::OnStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));
  return std::nullopt;
}

void TabContextContainerSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(store_);
  entries_.clear();
  store_->DeleteAllDataAndMetadata(std::move(delete_metadata_change_list),
                                   base::DoNothing());
}

std::unique_ptr<syncer::DataBatch>
TabContextContainerSyncBridge::GetDataForCommit(StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& key : storage_keys) {
    const ContainerId container_id(base::Uuid::ParseCaseInsensitive(key));
    if (const syncer::AgileSymmetricKeySet* key_set =
            base::FindPtrOrNull(entries_, container_id)) {
      auto entity_data = std::make_unique<syncer::EntityData>();
      *entity_data->specifics.mutable_encrypted_tab_context_container() =
          ToSpecifics(container_id, *key_set);
      entity_data->name = key;
      batch->Put(key, std::move(entity_data));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
TabContextContainerSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [container_id, key_set] : entries_) {
    auto entity_data = std::make_unique<syncer::EntityData>();
    *entity_data->specifics.mutable_encrypted_tab_context_container() =
        ToSpecifics(container_id, *key_set);
    const std::string key = container_id.value().AsLowercaseString();
    entity_data->name = key;
    batch->Put(key, std::move(entity_data));
  }
  return batch;
}

std::string TabContextContainerSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.encrypted_tab_context_container().uuid();
}

std::string TabContextContainerSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.encrypted_tab_context_container().uuid();
}

sync_pb::EntitySpecifics
TabContextContainerSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_pb::EntitySpecifics();
}

bool TabContextContainerSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AreSpecificsValid(
      entity_data.specifics.encrypted_tab_context_container());
}

void TabContextContainerSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&TabContextContainerSyncBridge::OnReadAllDataAndMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabContextContainerSyncBridge::OnReadAllDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const syncer::DataTypeStore::Record& record : *data_records) {
    sync_pb::EncryptedTabContextContainerSpecifics specifics;
    if (!specifics.ParseFromString(record.value) ||
        !AreSpecificsValid(specifics)) {
      change_processor()->ReportError(
          {FROM_HERE, syncer::ModelError::Type::
                          kTabContextContainerFailedToDeserializeSpecifics});
      return;
    }
    const ContainerId container_id(
        base::Uuid::ParseCaseInsensitive(specifics.uuid()));
    std::unique_ptr<syncer::AgileSymmetricKeySet> encryption_key =
        syncer::AgileSymmetricKeySet::FromProto(specifics.encryption_key());
    entries_[container_id] = std::move(encryption_key);
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void TabContextContainerSyncBridge::OnStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
  }
}

}  // namespace sync_tab_context
