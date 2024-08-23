// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/model_error.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_device_info/local_device_info_util.h"

namespace sync_pb {
class DeviceInfoSpecifics;
enum SyncEnums_DeviceType : int;
}  // namespace sync_pb

namespace syncer {

class DeviceInfoPrefs;

// Sync bridge implementation for DEVICE_INFO data type. Handles storage of
// device info and associated sync metadata, applying/merging foreign changes,
// and allows public read access.
class DeviceInfoSyncBridge : public DataTypeSyncBridge,
                             public DeviceInfoTracker {
 public:
  DeviceInfoSyncBridge(
      std::unique_ptr<MutableLocalDeviceInfoProvider>
          local_device_info_provider,
      OnceDataTypeStoreFactory store_factory,
      std::unique_ptr<DataTypeLocalChangeProcessor> change_processor,
      std::unique_ptr<DeviceInfoPrefs> device_info_prefs);

  DeviceInfoSyncBridge(const DeviceInfoSyncBridge&) = delete;
  DeviceInfoSyncBridge& operator=(const DeviceInfoSyncBridge&) = delete;

  ~DeviceInfoSyncBridge() override;

  LocalDeviceInfoProvider* GetLocalDeviceInfoProvider();

  // Refresh local copy of device info in memory, and informs sync of the
  // change. Used when the caller knows a property of local device info has
  // changed (e.g. SharingInfo), and must be sync-ed to other devices as soon as
  // possible, without waiting for the periodic commits. The device info will be
  // compared with the local copy. If the data has been updated, then it will be
  // committed. Otherwise nothing happens.
  void RefreshLocalDeviceInfoIfNeeded();

  // The |callback| will be invoked on each successful commit with newly enabled
  // data types list. This is needed to invoke an additional GetUpdates request
  // for the data types which have been just enabled and subscribed for new
  // invalidations.
  void SetCommittedAdditionalInterestedDataTypesCallback(
      base::RepeatingCallback<void(const DataTypeSet&)> callback);

  // DataTypeSyncBridge implementation.
  void OnSyncStarting(const DataTypeActivationRequest& request) override;
  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  std::optional<ModelError> MergeFullSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_data) override;
  std::optional<ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;
  std::unique_ptr<DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
  void ApplyDisableSyncChanges(
      std::unique_ptr<MetadataChangeList> delete_metadata_change_list) override;
  DataTypeSyncBridge::CommitAttemptFailedBehavior OnCommitAttemptFailed(
      syncer::SyncCommitError commit_error) override;

  // DeviceInfoTracker implementation.
  bool IsSyncing() const override;
  const DeviceInfo* GetDeviceInfo(const std::string& client_id) const override;
  std::vector<const DeviceInfo*> GetAllDeviceInfo() const override;
  std::vector<const DeviceInfo*> GetAllChromeDeviceInfo() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::map<DeviceInfo::FormFactor, int> CountActiveDevicesByType()
      const override;
  bool IsRecentLocalCacheGuid(const std::string& cache_guid) const override;

  // For testing only.
  bool IsPulseTimerRunningForTest() const;
  void ForcePulseForTest() override;

 private:
  class ImmutableDeviceInfoAndSpecifics {
   public:
    explicit ImmutableDeviceInfoAndSpecifics(
        sync_pb::DeviceInfoSpecifics specifics);

    const sync_pb::DeviceInfoSpecifics& specifics() const { return specifics_; }

    const DeviceInfo& device_info() const { return device_info_; }

   private:
    const sync_pb::DeviceInfoSpecifics specifics_;
    const DeviceInfo device_info_;
  };

  // Cache of all syncable and local data, stored by device cache guid.
  using ClientIdToDeviceInfo =
      std::map<std::string, ImmutableDeviceInfoAndSpecifics>;

  // Parses the content of |record_list| into |*all_data|. The output
  // parameter is first for binding purposes.
  static std::optional<ModelError> ParseSpecificsOnBackendSequence(
      ClientIdToDeviceInfo* all_data,
      std::unique_ptr<DataTypeStore::RecordList> record_list);

  // Store SyncData in the cache and durable storage.
  void StoreSpecifics(sync_pb::DeviceInfoSpecifics specifics,
                      DataTypeStore::WriteBatch* batch);
  // Delete SyncData from the cache and durable storage, returns true if there
  // was actually anything at the given tag.
  bool DeleteSpecifics(const std::string& tag,
                       DataTypeStore::WriteBatch* batch);

  // Returns the device name based on |sync_mode_|. For transport only mode,
  // the device model name is returned. For full sync mode,
  // |local_personalizable_device_name_| is returned.
  std::string GetLocalClientName() const;

  // Notify all registered observers.
  void NotifyObservers();

  // Methods used as callbacks given to DataTypeStore.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<DataTypeStore> store);
  void OnLocalDeviceNameInfoRetrieved(
      LocalDeviceNameInfo local_device_name_info);
  void OnReadAllData(std::unique_ptr<ClientIdToDeviceInfo> all_data,
                     const std::optional<syncer::ModelError>& error);
  void OnSyncInvalidationsInitialized();
  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<MetadataBatch> metadata_batch);
  void OnCommit(const std::optional<syncer::ModelError>& error);

  // Performs reconciliation between the locally provided device info and the
  // stored device info data. If the sets of data differ, then we consider this
  // a local change and we send it to the processor. Returns true if the local
  // data has been changed and sent to the processor.
  bool ReconcileLocalAndStored();

  // Stores the updated version of the local copy of device info in durable
  // storage, in memory, and informs sync of the change. Must not be called
  // before the provider and processor have initialized.
  void SendLocalData();

  // Same as above but allows callers to specify a WriteBatch
  void SendLocalDataWithBatch(std::unique_ptr<DataTypeStore::WriteBatch> batch);

  // Persists the changes in the given aggregators and notifies observers if
  // indicated to do as such.
  void CommitAndNotify(std::unique_ptr<DataTypeStore::WriteBatch> batch,
                       bool should_notify);

  // Deletes locally old data and metadata entries without issuing tombstones.
  void ExpireOldEntries();

  const std::unique_ptr<MutableLocalDeviceInfoProvider>
      local_device_info_provider_;

  std::string local_cache_guid_;
  ClientIdToDeviceInfo all_data_;

  LocalDeviceNameInfo local_device_name_info_;

  std::optional<SyncMode> sync_mode_;

  // Used to restrict reuploads of local device info on incoming tombstones.
  // This is necessary to prevent uncontrolled commits based on incoming
  // updates.
  bool reuploaded_on_tombstone_ = false;

  // Registered observers, not owned.
  base::ObserverList<Observer, true>::Unchecked observers_;

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<DataTypeStore> store_;

  // Used to update our local device info once every pulse interval.
  base::OneShotTimer pulse_timer_;

  // Used to force upload of local device info after initialization. Used in
  // tests only.
  bool force_reupload_for_test_ = false;

  std::vector<base::OnceClosure> device_info_synced_callback_list_;

  // Called when a new interested data type list has been committed. Only newly
  // enabled data types will be passed. May be empty.
  base::RepeatingCallback<void(const DataTypeSet&)>
      new_interested_data_types_callback_;

  const std::unique_ptr<DeviceInfoPrefs> device_info_prefs_;

  base::WeakPtrFactory<DeviceInfoSyncBridge> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_BRIDGE_H_
