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
  if (!encryptor_->IsEncryptionAvailable()) {
    // TODO(crbug.com/501036619): Report a ModelError when encryption is
    // unavailable.
    return;
  }
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

bool EntitySuppressionSyncBridge::ClearAllSuppressions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());

  if (guids_by_entry_.empty()) {
    return false;
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  for (const auto& [entry, guid] : guids_by_entry_) {
    batch->DeleteData(guid);
    change_processor()->Delete(guid, syncer::DeletionOrigin::Unspecified(),
                               batch->GetMetadataChangeList());
  }
  guids_by_entry_.clear();

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

bool EntitySuppressionSyncBridge::IsSuppressed(
    const EntitySuppressionEntry& entry) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return guids_by_entry_.contains(entry);
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

bool EntitySuppressionSyncBridge::ApplyRemoteEntry(
    const std::string& storage_key,
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics,
    EntitySuppressionEntry entry,
    syncer::DataTypeStore::WriteBatch& batch) {
  std::optional<std::string> encrypted_value =
      EncryptSuppressionSpecifics(specifics, *encryptor_);
  if (!encrypted_value) {
    // TODO(crbug.com/501036619): Report a ModelError when encryption is
    // unavailable.
    return false;
  }

  const std::string* existing_guid = base::FindOrNull(guids_by_entry_, entry);
  if (!existing_guid) {
    batch.WriteData(storage_key, *encrypted_value);
    guids_by_entry_.insert_or_assign(std::move(entry), storage_key);
    return true;
  }

  if (*existing_guid == storage_key) {
    batch.WriteData(storage_key, *encrypted_value);
    return false;
  }

  auto delete_entry = [&](const std::string& guid) {
    batch.DeleteData(guid);
    change_processor()->Delete(guid, syncer::DeletionOrigin::Unspecified(),
                               batch.GetMetadataChangeList());
  };

  // Duplicate GUIDs for the same suppression entry. Smaller GUID
  // lexicographically wins.
  if (storage_key < *existing_guid) {
    delete_entry(*existing_guid);
    batch.WriteData(storage_key, *encrypted_value);
    guids_by_entry_.insert_or_assign(std::move(entry), storage_key);
  } else {
    delete_entry(storage_key);
  }

  return false;
}

std::optional<syncer::ModelError>
EntitySuppressionSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());
  // TODO(crbug.com/501036619): Consider schema_version when merging.
  // TODO(crbug.com/501036619): Limit the maximum number of suppressions.
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch(std::move(metadata_change_list));

  absl::flat_hash_set<std::string> synced_remote_guids;
  synced_remote_guids.reserve(entity_data.size());
  bool added_remote_suppressions = false;

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics =
        change->data().specifics.autofill_entity_suppression();
    std::optional<EntitySuppressionEntry> entry =
        CreateEntitySuppressionEntryFromSpecifics(specifics);
    CHECK(entry);

    synced_remote_guids.insert(change->storage_key());
    if (ApplyRemoteEntry(change->storage_key(), specifics, std::move(*entry),
                         *batch)) {
      added_remote_suppressions = true;
    }
  }

  // Upload local-only entries to Sync (including any local entries that won
  // conflict resolution against remote duplicates).
  for (const auto& [entry, guid] : guids_by_entry_) {
    if (!synced_remote_guids.contains(guid)) {
      change_processor()->Put(
          guid, CreateEntityDataFromEntitySuppressionEntry(guid, entry),
          batch->GetMetadataChangeList());
    }
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&EntitySuppressionSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  if (added_remote_suppressions) {
    NotifySuppressionsChanged();
  }

  return std::nullopt;
}

std::optional<syncer::ModelError>
EntitySuppressionSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch(std::move(metadata_change_list));

  bool suppressions_changed = false;

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD: {
        const sync_pb::AutofillEntitySuppressionSpecifics& specifics =
            change->data().specifics.autofill_entity_suppression();
        std::optional<EntitySuppressionEntry> entry =
            CreateEntitySuppressionEntryFromSpecifics(specifics);
        CHECK(entry);

        if (ApplyRemoteEntry(change->storage_key(), specifics,
                             std::move(*entry), *batch)) {
          suppressions_changed = true;
        }
        break;
      }
      case syncer::EntityChange::ACTION_UPDATE:
        // Entity suppressions are immutable; updates are ignored.
        break;
      case syncer::EntityChange::ACTION_DELETE: {
        batch->DeleteData(change->storage_key());
        const size_t removed_count =
            base::EraseIf(guids_by_entry_, [&](const auto& pair) {
              return pair.second == change->storage_key();
            });
        if (removed_count > 0) {
          suppressions_changed = true;
        }
        break;
      }
    }
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&EntitySuppressionSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  if (suppressions_changed) {
    NotifySuppressionsChanged();
  }

  return std::nullopt;
}

void EntitySuppressionSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());
  store_->DeleteAllDataAndMetadata(
      std::move(delete_metadata_change_list),
      base::BindOnce(&EntitySuppressionSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  const bool had_entries = !guids_by_entry_.empty();
  guids_by_entry_.clear();

  if (had_entries) {
    NotifySuppressionsChanged();
  }
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

  is_loaded_ = true;
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

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
