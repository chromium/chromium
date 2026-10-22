// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
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
// Owned by `EntitySuppressionManager`, one instance per profile.
// TODO(crbug.com/501036619): Move under webdata/personal_context.
class EntitySuppressionSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when suppressions have been loaded or changed.
    virtual void OnSuppressionsChanged() {}
  };

  EntitySuppressionSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory,
      os_crypt_async::OSCryptAsync* os_crypt_async);
  EntitySuppressionSyncBridge(const EntitySuppressionSyncBridge&) = delete;
  EntitySuppressionSyncBridge& operator=(const EntitySuppressionSyncBridge&) =
      delete;
  ~EntitySuppressionSyncBridge() override;

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

  // Applies a remote suppression to persistent storage and in-memory cache:
  // Encrypts `specifics` and writes it to `batch` under `storage_key`, and
  // indexes the parsed `key` in `metadata_by_entry_`. Resolves conflicts if
  // a suppression with the same key already exists under a different GUID.
  // Returns `true` if `key` was not already suppressed, `false` otherwise.
  bool ApplyRemoteEntry(
      const std::string& storage_key,
      const sync_pb::AutofillEntitySuppressionSpecifics& specifics,
      EntitySuppressionKey key,
      syncer::DataTypeStore::WriteBatch& batch);

  // Deletes the oldest suppressions from `metadata_by_entry_`, local storage
  // (via `batch`) and Sync until at most `kMaxSuppressions` remain. Of
  // suppressions with equal creation times, the one with the smaller GUID is
  // kept, so that all clients delete the same suppressions. Returns whether any
  // suppression was deleted.
  bool DeleteOldestSuppressionsAboveLimit(
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
  base::flat_map<EntitySuppressionKey, EntryMetadata> metadata_by_entry_;

  base::WeakPtrFactory<EntitySuppressionSyncBridge> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_SYNC_BRIDGE_H_
