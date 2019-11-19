// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BRIDGE_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace history {
class HistoryService;
}  // namespace history

namespace syncer {
class DeviceInfoTracker;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace base {
class Clock;
}  // namespace base

namespace send_tab_to_self {

struct TargetDeviceInfo;

// Interface for a persistence layer for send tab to self.
// All interface methods have to be called on main thread.
class SendTabToSelfBridge : public syncer::ModelTypeSyncBridge,
                            public SendTabToSelfModel,
                            public history::HistoryServiceObserver {
 public:
  // The caller should ensure that all raw pointers are not null and will
  // outlive this object. This is not guaranteed by this class.
  SendTabToSelfBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      base::Clock* clock,
      syncer::OnceModelTypeStoreFactory create_store_callback,
      history::HistoryService* history_service,
      syncer::DeviceInfoTracker* device_info_tracker);
  ~SendTabToSelfBridge() override;

  // syncer::ModelTypeSyncBridge overrides.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyStopSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                delete_metadata_change_list) override;

  // SendTabToSelfModel overrides.
  std::vector<std::string> GetAllGuids() const override;
  void DeleteAllEntries() override;
  const SendTabToSelfEntry* GetEntryByGUID(
      const std::string& guid) const override;
  const SendTabToSelfEntry* AddEntry(
      const GURL& url,
      const std::string& title,
      base::Time navigation_time,
      const std::string& target_device_cache_guid) override;
  void DeleteEntry(const std::string& guid) override;
  void DismissEntry(const std::string& guid) override;
  void MarkEntryOpened(const std::string& guid) override;
  bool IsReady() override;
  bool HasValidTargetDevice() override;
  std::vector<TargetDeviceInfo> GetTargetDeviceInfoSortedList() override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // For testing only.
  static std::unique_ptr<syncer::ModelTypeStore> DestroyAndStealStoreForTest(
      std::unique_ptr<SendTabToSelfBridge> bridge);
  bool ShouldUpdateTargetDeviceInfoListForTest();
  void SetLocalDeviceNameForTest(const std::string& local_device_name);

 private:
  using SendTabToSelfEntries =
      std::map<std::string, std::unique_ptr<SendTabToSelfEntry>>;

  // Notify all observers of any added |new_entries| when they are added the the
  // model via sync.
  void NotifyRemoteSendTabToSelfEntryAdded(
      const std::vector<const SendTabToSelfEntry*>& new_entries);

  // Notify all observers when the entries with |guids| have been removed from
  // the model via sync or via history deletion.
  void NotifyRemoteSendTabToSelfEntryDeleted(
      const std::vector<std::string>& guids);

  // Notify all observers when any new or existing |opened_entries| have been
  // marked as opened in the model via sync.
  void NotifyRemoteSendTabToSelfEntryOpened(
      const std::vector<const SendTabToSelfEntry*>& opened_entries);

  // Notify all observers that the model is loaded;
  void NotifySendTabToSelfModelLoaded();

  // Methods used as callbacks given to DataTypeStore.
  void OnStoreCreated(const base::Optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);
  void OnReadAllData(std::unique_ptr<SendTabToSelfEntries> initial_entries,
                     std::unique_ptr<std::string> local_device_name,
                     const base::Optional<syncer::ModelError>& error);
  void OnReadAllMetadata(const base::Optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommit(const base::Optional<syncer::ModelError>& error);

  // Persists the changes in the given aggregators
  void Commit(std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch);

  // Returns a specific entry for editing. Returns null if the entry does not
  // exist.
  SendTabToSelfEntry* GetMutableEntryByGUID(const std::string& guid) const;

  // Delete expired entries.
  void DoGarbageCollection();

  // Returns whether the target device info list should be updated.
  bool ShouldUpdateTargetDeviceInfoList() const;

  // Sets the target device info list.
  void SetTargetDeviceInfoList();

  // Remove entry with |guid| from entries, but doesn't call Commit on provided
  // |batch|. This allows multiple for deletions without duplicate batch calls.
  void DeleteEntryWithBatch(const std::string& guid,
                            syncer::ModelTypeStore::WriteBatch* batch);

  // Delete all of the entries that match the URLs provided.
  void DeleteEntries(const std::vector<GURL>& urls);

  // |entries_| is keyed by GUIDs.
  SendTabToSelfEntries entries_;

  // |clock_| isn't owned.
  const base::Clock* const clock_;

  // |history_service_| isn't owned.
  history::HistoryService* const history_service_;

  // |device_info_tracker_| isn't owned.
  syncer::DeviceInfoTracker* const device_info_tracker_;

  // The name of this local device.
  std::string local_device_name_;

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  // A pointer to the most recently used entry used for deduplication.
  const SendTabToSelfEntry* mru_entry_;

  // A list of target devices and their associated cache information.
  std::vector<TargetDeviceInfo> target_device_name_to_cache_info_;

  // The following two variables are used to determine whether we should update
  // the target device name to cache guid map.
  base::Time oldest_non_expired_device_timestamp_;
  size_t number_of_devices_ = 0;

  base::WeakPtrFactory<SendTabToSelfBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfBridge);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BRIDGE_H_
