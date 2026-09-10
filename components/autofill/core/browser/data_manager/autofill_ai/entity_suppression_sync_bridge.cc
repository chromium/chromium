// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/webdata/personal_context/entity_suppression_sync_util.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace autofill {

namespace {

// Encrypts `AutofillEntitySuppressionSpecifics` into an encrypted string for
// disk storage. Returns `std::nullopt` if encryption fails.
std::optional<std::string> EncryptSuppressionSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics,
    const os_crypt_async::Encryptor& encryptor) {
  std::string encrypted_value;
  if (!encryptor.EncryptString(specifics.SerializeAsString(),
                               &encrypted_value)) {
    return std::nullopt;
  }
  return encrypted_value;
}

// Decrypts an encrypted value from disk into
// `AutofillEntitySuppressionSpecifics`. Returns `std::nullopt` if decryption
// fails or the plaintext cannot be parsed.
std::optional<sync_pb::AutofillEntitySuppressionSpecifics>
DecryptSuppressionSpecifics(const std::string& encrypted_value,
                            const os_crypt_async::Encryptor& encryptor) {
  std::string plaintext;
  if (!encryptor.DecryptString(encrypted_value, &plaintext)) {
    return std::nullopt;
  }

  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  if (!specifics.ParseFromString(plaintext)) {
    return std::nullopt;
  }

  return specifics;
}

}  // namespace

EntitySuppressionSyncBridge::EntitySuppressionSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory,
    scoped_refptr<const os_crypt_async::Encryptor> encryptor)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      encryptor_(std::move(encryptor)) {
  CHECK(encryptor_);
  std::move(store_factory)
      .Run(syncer::AUTOFILL_ENTITY_SUPPRESSION,
           base::BindOnce(&EntitySuppressionSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

EntitySuppressionSyncBridge::~EntitySuppressionSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EntitySuppressionSyncBridge::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void EntitySuppressionSyncBridge::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

bool EntitySuppressionSyncBridge::Suppress(
    const EntitySuppressionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());

  if (!change_processor()->IsTrackingMetadata()) {
    return false;
  }

  if (guids_by_entry_.contains(entry)) {
    return false;
  }

  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionEntry(guid, entry);

  std::optional<std::string> encrypted_value =
      EncryptSuppressionSpecifics(specifics, *encryptor_);
  if (!encrypted_value) {
    return false;
  }

  guids_by_entry_.insert_or_assign(entry, guid);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(guid, *encrypted_value);
  change_processor()->Put(
      guid, CreateEntityDataFromEntitySuppressionSpecifics(specifics),
      batch->GetMetadataChangeList());

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&EntitySuppressionSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  NotifySuppressionsChanged();
  return true;
}

bool EntitySuppressionSyncBridge::Unsuppress(
    const EntitySuppressionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());

  if (!change_processor()->IsTrackingMetadata()) {
    return false;
  }

  const std::string* stored_guid = base::FindOrNull(guids_by_entry_, entry);
  if (!stored_guid) {
    return false;
  }

  const std::string guid = *stored_guid;
  guids_by_entry_.erase(entry);

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->DeleteData(guid);
  change_processor()->Delete(guid, syncer::DeletionOrigin::Unspecified(),
                             batch->GetMetadataChangeList());

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&EntitySuppressionSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  NotifySuppressionsChanged();
  return true;
}

base::flat_set<EntitySuppressionEntry>
EntitySuppressionSyncBridge::GetSuppressions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {base::sorted_unique,
          base::ToVector(guids_by_entry_,
                         [](const auto& pair) { return pair.first; })};
}

bool EntitySuppressionSyncBridge::IsLoaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_loaded_;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
EntitySuppressionSyncBridge::GetControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return change_processor()->GetControllerDelegate();
}

std::unique_ptr<syncer::MetadataChangeList>
EntitySuppressionSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError>
EntitySuppressionSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/501036619): Implement.
  return std::nullopt;
}

std::optional<syncer::ModelError>
EntitySuppressionSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/501036619): Implement.
  return std::nullopt;
}

void EntitySuppressionSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/501036619): Implement.
}

std::unique_ptr<syncer::MutableDataBatch>
EntitySuppressionSyncBridge::GetAllData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [entry, guid] : guids_by_entry_) {
    batch->Put(guid, CreateEntityDataFromEntitySuppressionEntry(guid, entry));
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
EntitySuppressionSyncBridge::GetDataForCommit(StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  absl::flat_hash_set<std::string> keys_set(std::from_range, storage_keys);
  std::unique_ptr<syncer::DataBatch> all_data = GetAllData();
  while (all_data->HasNext()) {
    syncer::KeyAndData item = all_data->Next();
    if (keys_set.contains(item.first)) {
      batch->Put(item.first, std::move(item.second));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
EntitySuppressionSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetAllData();
}

std::string EntitySuppressionSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string EntitySuppressionSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.autofill_entity_suppression().guid();
}

bool EntitySuppressionSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.has_autofill_entity_suppression() &&
         AreEntitySuppressionSpecificsValid(
             entity_data.specifics.autofill_entity_suppression());
}

sync_pb::EntitySpecifics
EntitySuppressionSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  return sync_pb::EntitySpecifics();
}

void EntitySuppressionSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&EntitySuppressionSyncBridge::OnReadAllDataAndMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EntitySuppressionSyncBridge::OnReadAllDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const syncer::DataTypeStore::Record& record : *data_records) {
    if (std::optional<EntitySuppressionEntry> entry =
            DecryptSuppressionSpecifics(record.value, *encryptor_)
                .and_then(CreateEntitySuppressionEntryFromSpecifics)) {
      guids_by_entry_.insert_or_assign(std::move(*entry), record.id);
    }
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  is_loaded_ = true;

  NotifySuppressionsChanged();
}

void EntitySuppressionSyncBridge::ReportErrorIfSet(
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void EntitySuppressionSyncBridge::NotifySuppressionsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnSuppressionsChanged();
  }
}

}  // namespace autofill
