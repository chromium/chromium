// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"

namespace sync_pb {
class DeviceInfoSpecifics;
}  // namespace sync_pb

namespace syncer {

class DeviceInfoPrefs;

// Sync bridge implementation for DEVICE_INFO model type. Handles storage of
// device info and associated sync metadata, applying/merging foreign changes,
// and allows public read access.
class DeviceInfoSyncBridge : public ModelTypeSyncBridge,
                             public DeviceInfoTracker {
 public:
  DeviceInfoSyncBridge(
      std::unique_ptr<MutableLocalDeviceInfoProvider>
          local_device_info_provider,
      OnceModelTypeStoreFactory store_factory,
      std::unique_ptr<ModelTypeChangeProcessor> change_processor,
      std::unique_ptr<DeviceInfoPrefs> device_info_prefs);
  ~DeviceInfoSyncBridge() override;

  LocalDeviceInfoProvider* GetLocalDeviceInfoProvider();

  // Refresh local copy of device info in memory, and informs sync of the
  // change. Used when the caller knows a property of local device info has
  // changed (e.g. SharingInfo), and must be sync-ed to other devices as soon as
  // possible, without waiting for the periodic commits.
  void RefreshLocalDeviceInfo();

  // ModelTypeSyncBridge implementation.
  void OnSyncStarting(const DataTypeActivationRequest& request) override;
  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  base::Optional<ModelError> MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_data) override;
  base::Optional<ModelError> ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
  void ApplyStopSyncChanges(
      std::unique_ptr<MetadataChangeList> delete_metadata_change_list) override;

  // DeviceInfoTracker implementation.
  bool IsSyncing() const override;
  std::unique_ptr<DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const override;
  std::vector<std::unique_ptr<DeviceInfo>> GetAllDeviceInfo() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  int CountActiveDevices() const override;
  bool IsRecentLocalCacheGuid(const std::string& cache_guid) const override;

  // For testing only.
  bool IsPulseTimerRunningForTest() const;
  void ForcePulseForTest() override;

 private:
  // Cache of all syncable and local data, stored by device cache guid.
  using ClientIdToSpecifics =
      std::map<std::string, std::unique_ptr<sync_pb::DeviceInfoSpecifics>>;

  // Store SyncData in the cache and durable storage.
  void StoreSpecifics(std::unique_ptr<sync_pb::DeviceInfoSpecifics> specifics,
                      ModelTypeStore::WriteBatch* batch);
  // Delete SyncData from the cache and durable storage, returns true if there
  // was actually anything at the given tag.
  bool DeleteSpecifics(const std::string& tag,
                       ModelTypeStore::WriteBatch* batch);

  // Returns the device name based on |sync_mode_|. For transport only mode,
  // the device model name is returned. For full sync mode,
  // |local_personalizable_device_name_| is returned.
  std::string GetLocalClientName() const;

  // Notify all registered observers.
  void NotifyObservers();

  // Methods used as callbacks given to DataTypeStore.
  void OnStoreCreated(const base::Optional<syncer::ModelError>& error,
                      std::unique_ptr<ModelTypeStore> store);
  void OnHardwareInfoRetrieved(base::SysInfo::HardwareInfo hardware_info);
  void OnReadAllData(std::unique_ptr<ClientIdToSpecifics> all_data,
                     std::unique_ptr<std::string> session_name,
                     const base::Optional<syncer::ModelError>& error);
  void OnReadAllMetadata(const base::Optional<syncer::ModelError>& error,
                         std::unique_ptr<MetadataBatch> metadata_batch);
  void OnCommit(const base::Optional<syncer::ModelError>& error);

  // Performs reconciliation between the locally provided device info and the
  // stored device info data. If the sets of data differ, then we consider this
  // a local change and we send it to the processor.
  void ReconcileLocalAndStored();

  // Stores the updated version of the local copy of device info in durable
  // storage, in memory, and informs sync of the change. Must not be called
  // before the provider and processor have initialized.
  void SendLocalData();

  // Same as above but allows callers to specify a WriteBatch
  void SendLocalDataWithBatch(
      std::unique_ptr<ModelTypeStore::WriteBatch> batch);

  // Persists the changes in the given aggregators and notifies observers if
  // indicated to do as such.
  void CommitAndNotify(std::unique_ptr<ModelTypeStore::WriteBatch> batch,
                       bool should_notify);

  // Counts the number of active devices relative to |now|. The activeness of a
  // device depends on the amount of time since it was updated, which means
  // comparing it against the current time. |now| is passed into this method to
  // allow unit tests to control expected results.
  int CountActiveDevices(const base::Time now) const;

  // Deletes locally old data and metadata entries without issuing tombstones.
  void ExpireOldEntries();

  const std::unique_ptr<MutableLocalDeviceInfoProvider>
      local_device_info_provider_;

  std::string local_cache_guid_;
  std::string local_personalizable_device_name_;
  ClientIdToSpecifics all_data_;

  // TODO(crbug.com/1019689): Replace hardware info with a custom data type.
  base::SysInfo::HardwareInfo local_hardware_info_;

  base::Optional<SyncMode> sync_mode_;

  // Registered observers, not owned.
  base::ObserverList<Observer, true>::Unchecked observers_;

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<ModelTypeStore> store_;

  // Used to update our local device info once every pulse interval.
  base::OneShotTimer pulse_timer_;

  const std::unique_ptr<DeviceInfoPrefs> device_info_prefs_;

  base::WeakPtrFactory<DeviceInfoSyncBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoSyncBridge);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_SYNC_BRIDGE_H_
