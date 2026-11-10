// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_bridge.h"

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
#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/personal_context_metrics.h"
#include "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_util.h"
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

PersonalContextEntityMetadataSyncBridge::
    PersonalContextEntityMetadataSyncBridge(
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
      base::BindOnce(
          &PersonalContextEntityMetadataSyncBridge::OnRequiredServicesReady,
          weak_ptr_factory_.GetWeakPtr()));
  os_crypt_async->GetInstance(
      base::BindOnce(&PersonalContextEntityMetadataSyncBridge::OnEncryptorReady,
                     weak_ptr_factory_.GetWeakPtr(), barrier));
  std::move(store_factory)
      .Run(syncer::AUTOFILL_ENTITY_SUPPRESSION,
           base::BindOnce(
               &PersonalContextEntityMetadataSyncBridge::OnStoreCreated,
               weak_ptr_factory_.GetWeakPtr(), barrier));
}

PersonalContextEntityMetadataSyncBridge::
    ~PersonalContextEntityMetadataSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PersonalContextEntityMetadataSyncBridge::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void PersonalContextEntityMetadataSyncBridge::RemoveObserver(
    Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

bool PersonalContextEntityMetadataSyncBridge::Suppress(
    const EntitySuppressionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());

  if (!change_processor()->IsTrackingMetadata()) {
    return false;
  }

  if (IsSuppressed(entry)) {
    return false;
  }

  const EntitySuppressionHash hash = GetEntitySuppressionHash(entry);
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::Time creation_time = base::Time::Now();
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionHash(
          guid, hash, creation_time,
          GetPossiblyTrimmedEntitySuppressionSpecifics(guid));

  suppression_metadata_by_entry_.insert_or_assign(
      hash, EntryMetadata{.guid = guid, .creation_time = creation_time});

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  WriteEntryToStoreAndSync(specifics, *batch);
  DeleteOldestEntriesAboveLimit(suppression_metadata_by_entry_,
                                kMaxSuppressions, *batch);

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&PersonalContextEntityMetadataSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  NotifySuppressionsChanged();
  return true;
}

bool PersonalContextEntityMetadataSyncBridge::Unsuppress(
    const EntitySuppressionEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());

  if (!change_processor()->IsTrackingMetadata()) {
    return false;
  }

  auto it =
      suppression_metadata_by_entry_.find(GetEntitySuppressionHash(entry));
  if (it == suppression_metadata_by_entry_.end()) {
    return false;
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  DeleteEntryFromStoreAndSync(it->second.guid, *batch);
  suppression_metadata_by_entry_.erase(it);

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&PersonalContextEntityMetadataSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  NotifySuppressionsChanged();
  return true;
}

bool PersonalContextEntityMetadataSyncBridge::ClearSuppressions(
    base::Time delete_begin,
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
      suppression_metadata_by_entry_,
      [&](const std::pair<EntitySuppressionHash, EntryMetadata>& pair) {
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
      base::BindOnce(&PersonalContextEntityMetadataSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  NotifySuppressionsChanged();
  return true;
}

base::flat_set<EntitySuppressionHash>
PersonalContextEntityMetadataSyncBridge::GetSuppressions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {base::sorted_unique,
          base::ToVector(suppression_metadata_by_entry_,
                         [](const auto& pair) { return pair.first; })};
}

bool PersonalContextEntityMetadataSyncBridge::IsSuppressed(
    const EntitySuppressionEntry& entry) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return suppression_metadata_by_entry_.contains(
      GetEntitySuppressionHash(entry));
}

size_t PersonalContextEntityMetadataSyncBridge::CountSuppressionsCreatedBetween(
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
      suppression_metadata_by_entry_,
      [&](const std::pair<EntitySuppressionHash, EntryMetadata>& pair) {
        const base::Time creation_time = pair.second.creation_time;
        return creation_time >= begin && creation_time <= end;
      });
}

bool PersonalContextEntityMetadataSyncBridge::IsLoaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_loaded_;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
PersonalContextEntityMetadataSyncBridge::GetControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return change_processor()->GetControllerDelegate();
}

std::unique_ptr<syncer::MetadataChangeList>
PersonalContextEntityMetadataSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

// static
PersonalContextEntityMetadataSyncBridge::EntryMetadata
PersonalContextEntityMetadataSyncBridge::CreateEntryMetadata(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics) {
  EntryMetadata metadata{
      .guid = specifics.guid(),
      .creation_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(specifics.creation_time_windows_epoch_micros())),
  };
  if (specifics.has_entity_suppression_key()) {
    metadata.legacy_key = specifics.entity_suppression_key();
  }
  return metadata;
}

// static
template <typename Key>
PersonalContextEntityMetadataSyncBridge::InsertOrResolveConflictResult
PersonalContextEntityMetadataSyncBridge::InsertOrResolveConflict(
    Key key,
    EntryMetadata new_metadata,
    base::flat_map<Key, EntryMetadata>& metadata_map) {
  EntryMetadata* existing_metadata = base::FindOrNull(metadata_map, key);
  if (!existing_metadata) {
    // New entry not yet tracked in `metadata_map`.
    metadata_map.insert_or_assign(std::move(key), std::move(new_metadata));
    return {.entry_added = true, .kept_new_entry = true};
  }

  if (existing_metadata->guid == new_metadata.guid) {
    // Same record (e.g. a local entry also returned by Sync during initial
    // merge); refresh its metadata in place.
    *existing_metadata = std::move(new_metadata);
    return {.entry_added = false, .kept_new_entry = true};
  }

  // Two distinct records with different GUIDs map to the same `key` (e.g. two
  // clients independently suppressed the same entity). Smaller GUID wins so all
  // clients converge on the same record.
  if (new_metadata.guid < existing_metadata->guid) {
    std::string guid_to_evict = std::move(existing_metadata->guid);
    *existing_metadata = std::move(new_metadata);
    return {
        .entry_added = false,
        .kept_new_entry = true,
        .guid_to_evict = std::move(guid_to_evict),
    };
  }

  return {
      .entry_added = false,
      .kept_new_entry = false,
      .guid_to_evict = std::move(new_metadata.guid),
  };
}

template <typename Key>
bool PersonalContextEntityMetadataSyncBridge::ApplyRemoteEntry(
    const std::string& storage_key,
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics,
    Key key,
    base::flat_map<Key, EntryMetadata>& metadata_map,
    syncer::DataTypeStore::WriteBatch& batch) {
  CHECK_EQ(storage_key, specifics.guid());
  InsertOrResolveConflictResult result = InsertOrResolveConflict(
      std::move(key), CreateEntryMetadata(specifics), metadata_map);
  if (result.guid_to_evict) {
    DeleteEntryFromStoreAndSync(*result.guid_to_evict, batch);
  }
  if (result.kept_new_entry) {
    batch.WriteData(storage_key,
                    EncryptSuppressionSpecifics(specifics, *encryptor_));
  }
  return result.entry_added;
}

void PersonalContextEntityMetadataSyncBridge::WriteEntryToStoreAndSync(
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics,
    syncer::DataTypeStore::WriteBatch& batch) {
  batch.WriteData(specifics.guid(),
                  EncryptSuppressionSpecifics(specifics, *encryptor_));
  change_processor()->Put(
      specifics.guid(),
      CreateEntityDataFromEntitySuppressionSpecifics(specifics),
      batch.GetMetadataChangeList());
}

void PersonalContextEntityMetadataSyncBridge::DeleteEntryFromStoreAndSync(
    const std::string& guid,
    syncer::DataTypeStore::WriteBatch& batch) {
  batch.DeleteData(guid);
  // This is a no-op for local-only keys during `MergeFullSyncData()` and
  // before the initial sync, since the change processor doesn't track them.
  change_processor()->Delete(guid, syncer::DeletionOrigin::Unspecified(),
                             batch.GetMetadataChangeList());
}

template <typename Key>
bool PersonalContextEntityMetadataSyncBridge::DeleteOldestEntriesAboveLimit(
    base::flat_map<Key, EntryMetadata>& metadata_map,
    size_t max_entries,
    syncer::DataTypeStore::WriteBatch& batch) {
  if (metadata_map.size() <= max_entries) {
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
  // Moves the oldest `delete_count` entries to the front.
  const size_t delete_count = metadata_map.size() - max_entries;
  std::vector<const EntryMetadata*> metadata = base::ToVector(
      metadata_map, [](const auto& pair) { return &pair.second; });
  std::ranges::partial_sort(metadata, metadata.begin() + delete_count,
                            compare_creation_time);
  const base::flat_set<std::string> guids_to_delete =
      base::MakeFlatSet<std::string>(base::span(metadata).first(delete_count),
                                     {}, &EntryMetadata::guid);

  base::EraseIf(metadata_map, [&](const auto& pair) {
    const std::string& guid = pair.second.guid;
    if (!guids_to_delete.contains(guid)) {
      return false;
    }
    DeleteEntryFromStoreAndSync(guid, batch);
    return true;
  });
  return true;
}

std::optional<syncer::ModelError>
PersonalContextEntityMetadataSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch(std::move(metadata_change_list));

  absl::flat_hash_set<std::string> synced_remote_guids;
  synced_remote_guids.reserve(entity_data.size());
  bool added_remote_suppressions = false;

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    const sync_pb::AutofillEntitySuppressionSpecifics& specifics =
        change->data().specifics.autofill_entity_suppression();
    std::optional<EntitySuppressionHash> hash =
        CreateEntitySuppressionHashFromSpecifics(specifics);
    CHECK(hash);

    synced_remote_guids.insert(change->storage_key());
    if (ApplyRemoteEntry(change->storage_key(), specifics, *hash,
                         suppression_metadata_by_entry_, *batch)) {
      added_remote_suppressions = true;
    }
  }

  // Clients without the limit might have synced more suppressions. This
  // happens before the upload below, so deleted local entries aren't uploaded.
  const bool deleted_suppressions = DeleteOldestEntriesAboveLimit(
      suppression_metadata_by_entry_, kMaxSuppressions, *batch);

  // Upload local-only entries to Sync (including any local entries that won
  // conflict resolution against remote duplicates).
  for (const auto& [hash, metadata] : suppression_metadata_by_entry_) {
    if (!synced_remote_guids.contains(metadata.guid)) {
      change_processor()->Put(metadata.guid,
                              CreateSuppressionEntityData(hash, metadata),
                              batch->GetMetadataChangeList());
    }
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&PersonalContextEntityMetadataSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  if (added_remote_suppressions || deleted_suppressions) {
    NotifySuppressionsChanged();
  }

  return std::nullopt;
}

std::optional<syncer::ModelError>
PersonalContextEntityMetadataSyncBridge::ApplyIncrementalSyncChanges(
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
        std::optional<EntitySuppressionHash> hash =
            CreateEntitySuppressionHashFromSpecifics(specifics);
        CHECK(hash);

        if (ApplyRemoteEntry(change->storage_key(), specifics, *hash,
                             suppression_metadata_by_entry_, *batch)) {
          suppressions_changed = true;
        }
        break;
      }
      case syncer::EntityChange::ACTION_UPDATE:
        // Entity suppressions are logically immutable. Remote updates (e.g.
        // legacy format migrations from other clients) are intentionally
        // ignored to avoid ping-pong; each client migrates any unmigrated local
        // records on startup.
        break;
      case syncer::EntityChange::ACTION_DELETE: {
        batch->DeleteData(change->storage_key());
        const size_t removed_count = base::EraseIf(
            suppression_metadata_by_entry_, [&](const auto& pair) {
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
  if (DeleteOldestEntriesAboveLimit(suppression_metadata_by_entry_,
                                    kMaxSuppressions, *batch)) {
    suppressions_changed = true;
  }

  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&PersonalContextEntityMetadataSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  if (suppressions_changed) {
    NotifySuppressionsChanged();
  }

  return std::nullopt;
}

void PersonalContextEntityMetadataSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());
  store_->DeleteAllDataAndMetadata(
      std::move(delete_metadata_change_list),
      base::BindOnce(&PersonalContextEntityMetadataSyncBridge::ReportErrorIfSet,
                     weak_ptr_factory_.GetWeakPtr()));

  const bool had_entries = !suppression_metadata_by_entry_.empty();
  suppression_metadata_by_entry_.clear();

  if (had_entries) {
    NotifySuppressionsChanged();
  }
}

std::unique_ptr<syncer::EntityData>
PersonalContextEntityMetadataSyncBridge::CreateSuppressionEntityData(
    EntitySuppressionHash hash,
    const EntryMetadata& metadata) const {
  sync_pb::AutofillEntitySuppressionSpecifics specifics =
      CreateSpecificsFromEntitySuppressionHash(
          metadata.guid, hash, metadata.creation_time,
          GetPossiblyTrimmedEntitySuppressionSpecifics(metadata.guid));
  if (metadata.legacy_key) {
    *specifics.mutable_entity_suppression_key() = *metadata.legacy_key;
  }
  return CreateEntityDataFromEntitySuppressionSpecifics(specifics);
}

std::unique_ptr<syncer::MutableDataBatch>
PersonalContextEntityMetadataSyncBridge::GetAllData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsLoaded());
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [hash, metadata] : suppression_metadata_by_entry_) {
    batch->Put(metadata.guid, CreateSuppressionEntityData(hash, metadata));
  }
  return batch;
}

const sync_pb::AutofillEntitySuppressionSpecifics&
PersonalContextEntityMetadataSyncBridge::
    GetPossiblyTrimmedEntitySuppressionSpecifics(
        const std::string& storage_key) const {
  return change_processor()
      ->GetPossiblyTrimmedRemoteSpecifics(storage_key)
      .autofill_entity_suppression();
}

std::unique_ptr<syncer::DataBatch>
PersonalContextEntityMetadataSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
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
PersonalContextEntityMetadataSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetAllData();
}

std::string PersonalContextEntityMetadataSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string PersonalContextEntityMetadataSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entity_data.specifics.autofill_entity_suppression().guid();
}

bool PersonalContextEntityMetadataSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.has_autofill_entity_suppression() &&
         AreEntitySuppressionSpecificsValid(
             entity_data.specifics.autofill_entity_suppression());
}

sync_pb::EntitySpecifics PersonalContextEntityMetadataSyncBridge::
    TrimAllSupportedFieldsFromRemoteSpecifics(
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

void PersonalContextEntityMetadataSyncBridge::OnStoreCreated(
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

void PersonalContextEntityMetadataSyncBridge::OnEncryptorReady(
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

void PersonalContextEntityMetadataSyncBridge::OnRequiredServicesReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!store_ || !encryptor_) {
    return;
  }
  store_->ReadAllDataAndMetadata(base::BindOnce(
      &PersonalContextEntityMetadataSyncBridge::OnReadAllDataAndMetadata,
      weak_ptr_factory_.GetWeakPtr()));
}

void PersonalContextEntityMetadataSyncBridge::OnReadAllDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  std::vector<std::string> guids_to_evict;
  std::vector<sync_pb::AutofillEntitySuppressionSpecifics> records_to_migrate;
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
    std::optional<EntitySuppressionHash> hash =
        CreateEntitySuppressionHashFromSpecifics(*specifics);
    // In practice, all data should be valid, but it could happen that some
    // local disk data gets corrupted, which is why these conditions are handled
    // gracefully here.
    if (!hash || record.id != specifics->guid()) {
      LogEntitySuppressionLoadDropReason(
          EntitySuppressionLoadDropReason::kInvalidEntry);
      ++dropped_record_count;
      continue;
    }
    InsertOrResolveConflictResult result = InsertOrResolveConflict(
        *hash, CreateEntryMetadata(*specifics), suppression_metadata_by_entry_);
    if (result.guid_to_evict) {
      guids_to_evict.push_back(std::move(*result.guid_to_evict));
    }
    if (result.kept_new_entry) {
      if (std::optional<sync_pb::AutofillEntitySuppressionSpecifics> migrated =
              MigrateLegacySuppressionSpecificsIfNeeded(*specifics)) {
        records_to_migrate.push_back(std::move(*migrated));
      }
    }
  }
  LogEntitySuppressionLoadDroppedRecordCount(dropped_record_count);

  is_loaded_ = true;
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  // Cleanup can only happen after calling `ModelReadyToSync()`.
  ApplyPostLoadCleanup(guids_to_evict, records_to_migrate);
  NotifySuppressionsChanged();
}

void PersonalContextEntityMetadataSyncBridge::ApplyPostLoadCleanup(
    base::span<const std::string> guids_to_evict,
    base::span<const sync_pb::AutofillEntitySuppressionSpecifics>
        records_to_migrate) {
  if (suppression_metadata_by_entry_.empty()) {
    // Either nothing was loaded, or `ModelReadyToSync()` cleared invalid or
    // stopped metadata via `ApplyDisableSyncChanges()`, which wiped the store.
    return;
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  bool batch_modified = false;

  for (const std::string& guid : guids_to_evict) {
    DeleteEntryFromStoreAndSync(guid, *batch);
    batch_modified = true;
  }

  // Builds without the limit might have stored more suppressions.
  batch_modified |= DeleteOldestEntriesAboveLimit(
      suppression_metadata_by_entry_, kMaxSuppressions, *batch);

  if (!records_to_migrate.empty()) {
    // Only migrate records that survived both duplicate resolution (an entry in
    // `records_to_migrate` may have been evicted by a subsequent record in
    // `OnReadAllDataAndMetadata()`) and `DeleteOldestEntriesAboveLimit()`.
    const base::flat_set<std::string> surviving_guids =
        base::MakeFlatSet<std::string>(suppression_metadata_by_entry_, {},
                                       [](const auto& entry_and_metadata) {
                                         return entry_and_metadata.second.guid;
                                       });
    for (const sync_pb::AutofillEntitySuppressionSpecifics& migrated :
         records_to_migrate) {
      if (!surviving_guids.contains(migrated.guid())) {
        continue;
      }
      WriteEntryToStoreAndSync(migrated, *batch);
      batch_modified = true;
    }
  }

  if (batch_modified) {
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(
            &PersonalContextEntityMetadataSyncBridge::ReportErrorIfSet,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void PersonalContextEntityMetadataSyncBridge::ReportErrorIfSet(
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void PersonalContextEntityMetadataSyncBridge::NotifySuppressionsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observers_) {
    observer.OnSuppressionsChanged();
  }
}

}  // namespace autofill
