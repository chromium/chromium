// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/personal_context_metrics.h"
#include "components/autofill/core/browser/webdata/personal_context/entity_suppression_sync_util.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/autofill_entity_suppression_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace autofill {

namespace {

// The maximum number of suppression records. When exceeded, the oldest
// records are deleted from local storage and Sync.
constexpr size_t kMaxSuppressions = 1000;

// Encrypts `AutofillEntitySuppressionSpecifics` into an encrypted string for
// disk storage. This can't fail because the bridge only keeps encryptors for
// which encryption is available (see `OnEncryptorReady()`).
std::string EncryptSuppressionSpecifics(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics,
    const os_crypt_async::Encryptor& encryptor) {
  std::string encrypted_value;
  CHECK(
      encryptor.EncryptString(specifics.SerializeAsString(), &encrypted_value));
  return encrypted_value;
}

// Decrypts an encrypted value from disk into
// `AutofillEntitySuppressionSpecifics`. Returns the reason for failure if
// decryption fails or the plaintext cannot be parsed.
base::expected<sync_pb::AutofillEntitySuppressionSpecifics,
               EntitySuppressionLoadDropReason>
DecryptSuppressionSpecifics(const std::string& encrypted_value,
                            const os_crypt_async::Encryptor& encryptor) {
  std::string plaintext;
  if (!encryptor.DecryptString(encrypted_value, &plaintext)) {
    return base::unexpected(EntitySuppressionLoadDropReason::kDecryptionFailed);
  }

  sync_pb::AutofillEntitySuppressionSpecifics specifics;
  if (!specifics.ParseFromString(plaintext)) {
    return base::unexpected(EntitySuppressionLoadDropReason::kParseFailed);
  }

  return specifics;
}

}  // namespace

EntitySuppressionSyncBridge::EntitySuppressionSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory,
    os_crypt_async::OSCryptAsync* os_crypt_async)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  CHECK(os_crypt_async);
  // Reading and decrypting local suppression records requires both the
  // `DataTypeStore` and the `os_crypt_async::Encryptor`. Since both are
  // initialized asynchronously, wait for both callbacks to complete before
  // reading data from the store.
  base::RepeatingClosure barrier = base::BarrierClosure(
      /*num_closures=*/2,
      base::BindOnce(&EntitySuppressionSyncBridge::OnRequiredServicesReady,
                     weak_ptr_factory_.GetWeakPtr()));
  os_crypt_async->GetInstance(
      base::BindOnce(&EntitySuppressionSyncBridge::OnEncryptorReady,
                     weak_ptr_factory_.GetWeakPtr(), barrier));
  std::move(store_factory)
      .Run(syncer::AUTOFILL_ENTITY_SUPPRESSION,
           base::BindOnce(&EntitySuppressionSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr(), barrier));
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

  if (IsSuppressed(entry)) {
    return false;
  }

  const EntitySuppressionKey key = GetEntitySuppressionHash(entry);
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::Time creation_time = base::Time::Now();
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionKey(
          guid, key, creation_time,
          GetPossiblyTrimmedEntitySuppressionSpecifics(guid));

  const std::string encrypted_value =
      EncryptSuppressionSpecifics(specifics, *encryptor_);

  metadata_by_entry_.insert_or_assign(
      key, EntryMetadata{.guid = guid, .creation_time = creation_time});

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  batch->WriteData(guid, encrypted_value);
  change_processor()->Put(
      guid, CreateEntityDataFromEntitySuppressionSpecifics(specifics),
      batch->GetMetadataChangeList());
  DeleteOldestSuppressionsAboveLimit(*batch);

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

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  auto delete_record = [&](const EntitySuppressionKey& key) {
    auto it = metadata_by_entry_.find(key);
    if (it == metadata_by_entry_.end()) {
      return false;
    }
    batch->DeleteData(it->second.guid);
    change_processor()->Delete(it->second.guid,
                               syncer::DeletionOrigin::Unspecified(),
                               batch->GetMetadataChangeList());
    metadata_by_entry_.erase(it);
    return true;
  };
  bool deleted =
      delete_record(EntitySuppressionKey(GetEntitySuppressionHash(entry)));
  // TODO(crbug.com/501036619): Remove once legacy entries are no longer
  // supported.
  deleted |=
      delete_record(EntitySuppressionKey(LegacyEntitySuppressionEntry(entry)));
  if (!deleted) {
    return false;
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&EntitySuppressionSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  NotifySuppressionsChanged();
  return true;
}

bool EntitySuppressionSyncBridge::ClearSuppressions(base::Time delete_begin,
                                                    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());

  if (delete_begin.is_null()) {
    delete_begin = base::Time::Min();
  }
  if (delete_end.is_null()) {
    delete_end = base::Time::Max();
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  const size_t removed_count = base::EraseIf(
      metadata_by_entry_,
      [&](const std::pair<EntitySuppressionKey, EntryMetadata>& pair) {
        const EntryMetadata& metadata = pair.second;
        if (metadata.creation_time < delete_begin ||
            metadata.creation_time > delete_end) {
          return false;
        }
        batch->DeleteData(metadata.guid);
        change_processor()->Delete(metadata.guid,
                                   syncer::DeletionOrigin::Unspecified(),
                                   batch->GetMetadataChangeList());
        return true;
      });

  if (removed_count == 0) {
    return false;
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&EntitySuppressionSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  NotifySuppressionsChanged();
  return true;
}

base::flat_set<EntitySuppressionKey>
EntitySuppressionSyncBridge::GetSuppressions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {base::sorted_unique,
          base::ToVector(metadata_by_entry_,
                         [](const auto& pair) { return pair.first; })};
}

bool EntitySuppressionSyncBridge::IsSuppressed(
    const EntitySuppressionEntry& entry) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (metadata_by_entry_.contains(
          EntitySuppressionKey(GetEntitySuppressionHash(entry)))) {
    return true;
  }
  // TODO(crbug.com/501036619): Remove once legacy entries are no longer
  // supported.
  return metadata_by_entry_.contains(
      EntitySuppressionKey(LegacyEntitySuppressionEntry(entry)));
}

size_t EntitySuppressionSyncBridge::CountSuppressionsCreatedBetween(
    base::Time begin,
    base::Time end) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (begin.is_null()) {
    begin = base::Time::Min();
  }
  if (end.is_null()) {
    end = base::Time::Max();
  }
  return std::ranges::count_if(
      metadata_by_entry_,
      [&](const std::pair<EntitySuppressionKey, EntryMetadata>& pair) {
        const base::Time creation_time = pair.second.creation_time;
        return creation_time >= begin && creation_time <= end;
      });
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
    EntitySuppressionKey key,
    syncer::DataTypeStore::WriteBatch& batch) {
  const std::string encrypted_value =
      EncryptSuppressionSpecifics(specifics, *encryptor_);
  const base::Time creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics.creation_time_windows_epoch_micros()));

  EntryMetadata* existing_metadata = base::FindOrNull(metadata_by_entry_, key);
  if (!existing_metadata) {
    batch.WriteData(storage_key, encrypted_value);
    metadata_by_entry_.insert_or_assign(
        std::move(key),
        EntryMetadata{.guid = storage_key, .creation_time = creation_time});
    return true;
  }

  if (existing_metadata->guid == storage_key) {
    batch.WriteData(storage_key, encrypted_value);
    existing_metadata->creation_time = creation_time;
    return false;
  }

  auto delete_entry = [&](const std::string& guid) {
    batch.DeleteData(guid);
    change_processor()->Delete(guid, syncer::DeletionOrigin::Unspecified(),
                               batch.GetMetadataChangeList());
  };

  // Duplicate GUIDs for the same suppression entry. Smaller GUID
  // lexicographically wins.
  if (storage_key < existing_metadata->guid) {
    delete_entry(existing_metadata->guid);
    batch.WriteData(storage_key, encrypted_value);
    *existing_metadata =
        EntryMetadata{.guid = storage_key, .creation_time = creation_time};
  } else {
    delete_entry(storage_key);
  }

  return false;
}

bool EntitySuppressionSyncBridge::DeleteOldestSuppressionsAboveLimit(
    syncer::DataTypeStore::WriteBatch& batch) {
  if (metadata_by_entry_.size() <= kMaxSuppressions) {
    return false;
  }

  // Orders from oldest to newest. For equal creation times, larger GUIDs come
  // first, so that smaller ones are kept.
  auto compare_creation_time = [](const EntryMetadata* a,
                                  const EntryMetadata* b) {
    return a->creation_time != b->creation_time
               ? a->creation_time < b->creation_time
               : a->guid > b->guid;
  };
  // Moves the oldest `delete_count` suppressions to the front.
  const size_t delete_count = metadata_by_entry_.size() - kMaxSuppressions;
  std::vector<const EntryMetadata*> metadata = base::ToVector(
      metadata_by_entry_, [](const auto& pair) { return &pair.second; });
  std::ranges::partial_sort(metadata, metadata.begin() + delete_count,
                            compare_creation_time);
  const base::flat_set<std::string> guids_to_delete =
      base::MakeFlatSet<std::string>(base::span(metadata).first(delete_count),
                                     {}, &EntryMetadata::guid);

  base::EraseIf(metadata_by_entry_, [&](const auto& pair) {
    const std::string& guid = pair.second.guid;
    if (!guids_to_delete.contains(guid)) {
      return false;
    }
    batch.DeleteData(guid);
    // This is a no-op for local-only keys during `MergeFullSyncData()` and
    // before the initial sync, since the change processor doesn't track them.
    change_processor()->Delete(guid, syncer::DeletionOrigin::Unspecified(),
                               batch.GetMetadataChangeList());
    return true;
  });
  return true;
}

std::optional<syncer::ModelError>
EntitySuppressionSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());
  // TODO(crbug.com/501036619): Consider schema_version when merging.
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch(std::move(metadata_change_list));

  absl::flat_hash_set<std::string> synced_remote_guids;
  synced_remote_guids.reserve(entity_data.size());
  bool added_remote_suppressions = false;

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics =
        change->data().specifics.autofill_entity_suppression();
    std::optional<EntitySuppressionKey> key =
        CreateEntitySuppressionKeyFromSpecifics(specifics);
    CHECK(key);

    synced_remote_guids.insert(change->storage_key());
    if (ApplyRemoteEntry(change->storage_key(), specifics, std::move(*key),
                         *batch)) {
      added_remote_suppressions = true;
    }
  }

  // Clients without the limit might have synced more suppressions. This
  // happens before the upload below, so deleted local entries aren't uploaded.
  const bool deleted_suppressions = DeleteOldestSuppressionsAboveLimit(*batch);

  // Upload local-only entries to Sync (including any local entries that won
  // conflict resolution against remote duplicates).
  for (const auto& [key, metadata] : metadata_by_entry_) {
    if (!synced_remote_guids.contains(metadata.guid)) {
      change_processor()->Put(
          metadata.guid,
          CreateEntityDataFromEntitySuppressionKey(
              metadata.guid, key, metadata.creation_time,
              GetPossiblyTrimmedEntitySuppressionSpecifics(metadata.guid)),
          batch->GetMetadataChangeList());
    }
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&EntitySuppressionSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  if (added_remote_suppressions || deleted_suppressions) {
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
        std::optional<EntitySuppressionKey> key =
            CreateEntitySuppressionKeyFromSpecifics(specifics);
        CHECK(key);

        if (ApplyRemoteEntry(change->storage_key(), specifics, std::move(*key),
                             *batch)) {
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
            base::EraseIf(metadata_by_entry_, [&](const auto& pair) {
              return pair.second.guid == change->storage_key();
            });
        if (removed_count > 0) {
          suppressions_changed = true;
        }
        break;
      }
    }
  }

  // Clients without the limit might have added more suppressions.
  if (DeleteOldestSuppressionsAboveLimit(*batch)) {
    suppressions_changed = true;
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

  const bool had_entries = !metadata_by_entry_.empty();
  metadata_by_entry_.clear();

  if (had_entries) {
    NotifySuppressionsChanged();
  }
}

std::unique_ptr<syncer::MutableDataBatch>
EntitySuppressionSyncBridge::GetAllData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [key, metadata] : metadata_by_entry_) {
    batch->Put(
        metadata.guid,
        CreateEntityDataFromEntitySuppressionKey(
            metadata.guid, key, metadata.creation_time,
            GetPossiblyTrimmedEntitySuppressionSpecifics(metadata.guid)));
  }
  return batch;
}

const sync_pb::AutofillEntitySuppressionSpecifics&
EntitySuppressionSyncBridge::GetPossiblyTrimmedEntitySuppressionSpecifics(
    const std::string& storage_key) const {
  return change_processor()
      ->GetPossiblyTrimmedRemoteSpecifics(storage_key)
      .autofill_entity_suppression();
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
  sync_pb::AutofillEntitySuppressionSpecifics
      trimmed_entity_suppression_specifics =
          TrimAutofillEntitySuppressionSpecificsDataForCaching(
              entity_specifics.autofill_entity_suppression());

  // If all fields are cleared from the entity suppression specifics, return a
  // fresh EntitySpecifics to avoid caching a few residual bytes.
  if (trimmed_entity_suppression_specifics.ByteSizeLong() == 0u) {
    return sync_pb::EntitySpecifics();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_autofill_entity_suppression() =
      std::move(trimmed_entity_suppression_specifics);

  return trimmed_entity_specifics;
}

void EntitySuppressionSyncBridge::OnStoreCreated(
    base::RepeatingClosure barrier,
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
  } else {
    CHECK(store);
    store_ = std::move(store);
  }
  barrier.Run();
}

void EntitySuppressionSyncBridge::OnEncryptorReady(
    base::RepeatingClosure barrier,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(encryptor);
  if (encryptor->IsEncryptionAvailable()) {
    encryptor_ = std::move(encryptor);
  } else {
    // Suppressions can't be stored without encryption. Report an error so that
    // Sync doesn't wait for this data type to load.
    change_processor()->ReportError(
        {FROM_HERE, syncer::ModelError::Type::
                        kAutofillEntitySuppressionEncryptionUnavailable});
  }
  barrier.Run();
}

void EntitySuppressionSyncBridge::OnRequiredServicesReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!store_ || !encryptor_) {
    return;
  }
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

  size_t dropped_record_count = 0;
  for (const syncer::DataTypeStore::Record& record : *data_records) {
    base::expected<sync_pb::AutofillEntitySuppressionSpecifics,
                   EntitySuppressionLoadDropReason>
        specifics = DecryptSuppressionSpecifics(record.value, *encryptor_);
    if (!specifics.has_value()) {
      LogEntitySuppressionLoadDropReason(specifics.error());
      ++dropped_record_count;
      continue;
    }
    std::optional<EntitySuppressionKey> key =
        CreateEntitySuppressionKeyFromSpecifics(*specifics);
    if (!key) {
      LogEntitySuppressionLoadDropReason(
          EntitySuppressionLoadDropReason::kInvalidEntry);
      ++dropped_record_count;
      continue;
    }
    metadata_by_entry_.insert_or_assign(
        std::move(*key),
        EntryMetadata{
            .guid = record.id,
            .creation_time =
                base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
                    specifics->creation_time_windows_epoch_micros()))});
  }
  LogEntitySuppressionLoadDroppedRecordCount(dropped_record_count);

  is_loaded_ = true;
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  // Builds without the limit might have stored more suppressions.
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  if (DeleteOldestSuppressionsAboveLimit(*batch)) {
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(&EntitySuppressionSyncBridge::ReportErrorIfSet,
                       weak_ptr_factory_.GetWeakPtr()));
  }

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
