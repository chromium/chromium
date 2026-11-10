// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace sync_pb {
class AutofillEntitySuppressionSpecifics;
}  // namespace sync_pb

namespace os_crypt_async {
class Encryptor;
class OSCryptAsync;
}  // namespace os_crypt_async

namespace syncer {
class DataTypeControllerDelegate;
class DataTypeLocalChangeProcessor;
class MetadataChangeList;
class MutableDataBatch;
}  // namespace syncer

namespace autofill {

// Sync bridge for the `AUTOFILL_ENTITY_SUPPRESSION` data type.
// Handles encrypted persistence at rest and Sync integration.
// Owned by `PersonalContextEntityMetadataManager`, one instance per profile.
class PersonalContextEntityMetadataSyncBridge
    : public syncer::DataTypeSyncBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when suppressions have been loaded or changed.
    virtual void OnSuppressionsChanged() {}
  };

  PersonalContextEntityMetadataSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory,
      os_crypt_async::OSCryptAsync* os_crypt_async);
  PersonalContextEntityMetadataSyncBridge(
      const PersonalContextEntityMetadataSyncBridge&) = delete;
  PersonalContextEntityMetadataSyncBridge& operator=(
      const PersonalContextEntityMetadataSyncBridge&) = delete;
  ~PersonalContextEntityMetadataSyncBridge() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // TODO(crbug.com/501036619): Make `Suppress()`, `Unsuppress()` and
  // `IsSuppressed()` take an `EntitySuppressionHash` once legacy entries are no
  // longer supported.

  // Adds a suppression `entry`, generates a GUID, encrypts and persists it,
  // and notifies Sync. If this exceeds the maximum number of suppressions, the
  // oldest ones are deleted. Returns `false` if Sync is not tracking metadata
  // or the `entry` was already suppressed. Returns `true` if suppression status
  // was modified.
  bool Suppress(const EntitySuppressionEntry& entry);

  // Deletes the suppression records of `entry`, so that it's no longer
  // suppressed. Returns `false` if Sync is not tracking metadata or there are
  // no such records. Returns `true` if suppression status was modified.
  bool Unsuppress(const EntitySuppressionEntry& entry);

  // Deletes suppression records whose creation timestamp falls within the
  // inclusive range `[delete_begin, delete_end]` from local storage and Sync,
  // and notifies observers if any record was removed. Returns `false` if Sync
  // is not tracking metadata or no entries matched the time range. Returns
  // `true` if suppression status was modified.
  bool ClearSuppressions(base::Time delete_begin, base::Time delete_end);

  // Returns the keys of all currently active suppressions.
  base::flat_set<EntitySuppressionKey> GetSuppressions() const;

  // Returns true if `entry` is currently suppressed.
  bool IsSuppressed(const EntitySuppressionEntry& entry) const;

  // Returns the number of suppression records whose creation timestamp falls
  // within the inclusive range `[begin, end]`. Null `begin`/`end` are treated
  // as `base::Time::Min()`/`base::Time::Max()`, matching `ClearSuppressions()`.
  size_t CountSuppressionsCreatedBetween(base::Time begin,
                                         base::Time end) const;

  bool IsLoaded() const;

  // Returns a weak pointer to the sync controller delegate.
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate();

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

 private:
  struct EntryMetadata {
    std::string guid;
    base::Time creation_time;
  };

  static EntryMetadata CreateEntryMetadata(
      const sync_pb::AutofillEntitySuppressionSpecifics& specifics);

  // Outcome of inserting an entry into an in-memory metadata map via
  // `InsertOrResolveConflict()`.
  struct InsertOrResolveConflictResult {
    // True if `key` was not already present in `metadata_map`.
    bool entry_added = false;
    // True if `new_metadata` was inserted or replaced the existing entry.
    bool kept_new_entry = false;
    // Set to the losing GUID if two distinct GUIDs mapped to the same `key`.
    std::optional<std::string> guid_to_evict;
  };

  // Inserts `(key, new_metadata)` into `metadata_map`, resolving conflicts
  // deterministically if an entry with the same `key` already exists under a
  // different GUID.
  template <typename Key>
  static InsertOrResolveConflictResult InsertOrResolveConflict(
      Key key,
      EntryMetadata new_metadata,
      base::flat_map<Key, EntryMetadata>& metadata_map);

  // Applies a remote entry to persistent storage and in-memory cache:
  // inserts `key` into `metadata_map`, deletes any evicted duplicate from
  // `batch` and Sync, and writes encrypted `specifics` to `batch` if kept.
  // Returns `true` if `key` was not already in `metadata_map`.
  template <typename Key>
  bool ApplyRemoteEntry(
      const std::string& storage_key,
      const sync_pb::AutofillEntitySuppressionSpecifics& specifics,
      Key key,
      base::flat_map<Key, EntryMetadata>& metadata_map,
      syncer::DataTypeStore::WriteBatch& batch);

  // Stages the encrypted write of `specifics` to local storage (via `batch`)
  // and notifies Sync (via `change_processor()`).
  void WriteEntryToStoreAndSync(
      const sync_pb::AutofillEntitySuppressionSpecifics& specifics,
      syncer::DataTypeStore::WriteBatch& batch);

  // Stages the deletion of `guid` from local storage (via `batch`) and
  // notifies Sync (via `change_processor()`).
  void DeleteEntryFromStoreAndSync(const std::string& guid,
                                   syncer::DataTypeStore::WriteBatch& batch);

  // Deletes the oldest entries from `metadata_map`, local storage (via `batch`)
  // and Sync until at most `max_entries` remain. Of entries with equal
  // creation times, the one with the smaller GUID is kept, so that all clients
  // delete the same entries. Returns whether any entry was deleted.
  template <typename Key>
  bool DeleteOldestEntriesAboveLimit(
      base::flat_map<Key, EntryMetadata>& metadata_map,
      size_t max_entries,
      syncer::DataTypeStore::WriteBatch& batch);

  void OnStoreCreated(base::RepeatingClosure barrier,
                      const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);
  void OnEncryptorReady(base::RepeatingClosure barrier,
                        scoped_refptr<os_crypt_async::Encryptor> encryptor);
  void OnRequiredServicesReady();
  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> data_records,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void ApplyPostLoadCleanup(
      base::span<const std::string> guids_to_evict,
      base::span<const sync_pb::AutofillEntitySuppressionSpecifics>
          records_to_migrate);
  void ReportErrorIfSet(const std::optional<syncer::ModelError>& error);

  // Queries all suppression data and returns it as a
  // `syncer::MutableDataBatch`.
  std::unique_ptr<syncer::MutableDataBatch> GetAllData();

  // If available, returns cached possibly trimmed
  // `AutofillEntitySuppressionSpecifics` for given `storage_key`. By default,
  // empty `AutofillEntitySuppressionSpecifics` is returned.
  const sync_pb::AutofillEntitySuppressionSpecifics&
  GetPossiblyTrimmedEntitySuppressionSpecifics(
      const std::string& storage_key) const;

  void NotifySuppressionsChanged();

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<const os_crypt_async::Encryptor> encryptor_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  bool is_loaded_ = false;

  base::ObserverList<Observer> observers_;

  // In-memory mapping from `EntitySuppressionKey` to its metadata.
  base::flat_map<EntitySuppressionKey, EntryMetadata>
      suppression_metadata_by_entry_;

  base::WeakPtrFactory<PersonalContextEntityMetadataSyncBridge>
      weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PERSONAL_CONTEXT_PERSONAL_CONTEXT_ENTITY_METADATA_SYNC_BRIDGE_H_
