// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SYNC_BRIDGE_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SYNC_BRIDGE_H_

#include <optional>
#include <unordered_map>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/uuid.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/model_error.h"

namespace syncer {
class MetadataChangeList;
}  // namespace syncer

namespace data_sharing::personal_collaboration_data {

// Sync bridge implementation for SHARED_TAB_GROUP_ACCOUNT_DATA data type.
class PersonalCollaborationDataSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Called when the bridge(database) has been loaded and is syncing. Will be
    // called immediately if the bridge has already initialized.
    virtual void OnInitialized() {}

    // Called when specifics have changed.
    virtual void OnEntityAddedOrUpdatedFromSync(
        const std::string& storage_key,
        const sync_pb::SharedTabGroupAccountDataSpecifics& data) {}

    // Called when specifics have been removed.
    virtual void OnEntityRemovedFromSync(const std::string& storage_key) {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  explicit PersonalCollaborationDataSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory data_type_store_factory);
  ~PersonalCollaborationDataSyncBridge() override;

  // Disallow copy/assign.
  PersonalCollaborationDataSyncBridge(
      const PersonalCollaborationDataSyncBridge&) = delete;
  PersonalCollaborationDataSyncBridge& operator=(
      const PersonalCollaborationDataSyncBridge&) = delete;

  // DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_change_list) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;
  syncer::ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const syncer::EntityData& remote_data) const override;

  // Returns whether the sync bridge has initialized and is syncing.
  bool IsInitialized() const;

  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>
  GetSpecificsForStorageKey(const std::string& storage_key) const;

  std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>
  GetTrimmedRemoteSpecifics(const std::string& storage_key) const;

  const std::unordered_map<std::string,
                           sync_pb::SharedTabGroupAccountDataSpecifics>&
  GetAllSpecifics() const;

  // Update the local copy and sync with the new data.
  void CreateOrUpdateSpecifics(
      const std::string& storage_key,
      const sync_pb::SharedTabGroupAccountDataSpecifics& specifics);

  // Delete an entity from sync. Also deletes from local storage and in-memory
  // cache.
  void RemoveSpecifics(const std::string& storage_key);

 private:
  // Loads the data already stored in the DataTypeStore.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);

  // Calls ModelReadyToSync if there are no errors to report and propagators the
  // stored entries to `on_load_callback`.
  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  void OnDataTypeStoreCommit(const std::optional<syncer::ModelError>& error);

  // Write a new entity to sync. This is used when the model is updated
  // with a new value and sync needs to be triggered.
  void WriteEntityToSync(const std::string& storage_key,
                         std::unique_ptr<syncer::EntityData> entity);

  void MaybeNotifyObserversInitialized();

  SEQUENCE_CHECKER(sequence_checker_);

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<syncer::DataTypeStore> store_;

  // Set to true once data is loaded from disk into the in-memory cache.
  bool is_initialized_ = false;

  // Set to true once observers have been notified of initialization.
  bool notified_observers_initialized_ = false;

  // In-memory data cache of specifics, keyed by its storage key.
  std::unordered_map<std::string, sync_pb::SharedTabGroupAccountDataSpecifics>
      specifics_;

  // List of observers.
  base::ObserverList<PersonalCollaborationDataSyncBridge::Observer> observers_;

  // Allows safe temporary use of the PersonalCollaborationDataSyncBridge
  // object if it exists at the time of use.
  base::WeakPtrFactory<PersonalCollaborationDataSyncBridge> weak_ptr_factory_{
      this};
};

}  // namespace data_sharing::personal_collaboration_data

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SYNC_BRIDGE_H_
