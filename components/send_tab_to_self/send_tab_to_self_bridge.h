// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BRIDGE_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
}  // namespace syncer

namespace base {
class Clock;
}  // namespace base

namespace send_tab_to_self {

struct TargetDeviceInfo;

// Interface for a persistence layer for send tab to self.
// All interface methods have to be called on main thread.
class SendTabToSelfBridge : public syncer::DataTypeSyncBridge,
                            public SendTabToSelfModel,
                            public syncer::DeviceInfoTracker::Observer,
                            public history::HistoryServiceObserver {
 public:
  // The caller should ensure that all raw pointers are not null and will
  // outlive this object. This is not guaranteed by this class.
  SendTabToSelfBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      base::Clock* clock,
      syncer::OnceDataTypeStoreFactory create_store_callback,
      history::HistoryService* history_service,
      syncer::DeviceInfoTracker* device_info_tracker);

  SendTabToSelfBridge(const SendTabToSelfBridge&) = delete;
  SendTabToSelfBridge& operator=(const SendTabToSelfBridge&) = delete;

  ~SendTabToSelfBridge() override;

  // syncer::DataTypeSyncBridge overrides.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

  // SendTabToSelfModel overrides.
  std::vector<std::string> GetAllGuids() const override;
  const SendTabToSelfEntry* GetEntryByGUID(
      const std::string& guid) const override;
  const SendTabToSelfEntry* AddEntry(
      const GURL& url,
      const std::string& title,
      const std::string& target_device_cache_guid) override;
  void DeleteEntry(const std::string& guid) override;
  void DismissEntry(const std::string& guid) override;
  void MarkEntryOpened(const std::string& guid) override;
  bool IsReady() override;
  bool HasValidTargetDevice() override;
  std::vector<TargetDeviceInfo> GetTargetDeviceInfoSortedList() override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // syncer::DeviceInfoTracker::Observer overrides.
  void OnDeviceInfoChange() override;

  // For testing only.
  static std::unique_ptr<syncer::DataTypeStore> DestroyAndStealStoreForTest(
      std::unique_ptr<SendTabToSelfBridge> bridge);
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
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);
  void OnReadAllData(std::unique_ptr<SendTabToSelfEntries> initial_entries,
                     std::unique_ptr<std::string> local_device_name,
                     const std::optional<syncer::ModelError>& error);
  void OnReadAllMetadata(const std::optional<syncer::ModelError>& error,
                         std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnCommit(const std::optional<syncer::ModelError>& error);

  // Persists the changes in the given aggregators
  void Commit(std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch);

  // Returns a specific entry for editing. Returns null if the entry does not
  // exist.
  SendTabToSelfEntry* GetMutableEntryByGUID(const std::string& guid) const;

  // Delete expired entries.
  void DoGarbageCollection();

  void ComputeTargetDeviceInfoSortedList();

  // Remove entry with |guid| from entries, but doesn't call Commit on provided
  // |batch|. This allows multiple for deletions without duplicate batch calls.
  void DeleteEntryWithBatch(const std::string& guid,
                            syncer::DataTypeStore::WriteBatch* batch);

  // Delete all of the entries that match the URLs provided.
  void DeleteEntries(const std::vector<GURL>& urls);

  void DeleteAllEntries();

  void EraseEntryInBatch(const std::string& guid,
                         syncer::DataTypeStore::WriteBatch* batch);

  // |entries_| is keyed by GUIDs.
  SendTabToSelfEntries entries_;

  // Stores guids of entries that have been opened from a layer other than
  // SendTabToSelfModel. Once the bridge receives the respective entries, they
  // will be marked opened. Entries are in-memory only and will be lost on
  // browser restart.
  base::flat_set<std::string> unknown_opened_entries_;

  // |clock_| isn't owned.
  const raw_ptr<const base::Clock> clock_;

  // |history_service_| isn't owned.
  const raw_ptr<history::HistoryService> history_service_;

  // |device_info_tracker_| isn't owned.
  const raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;

  // The name of this local device.
  std::string local_device_name_;

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<syncer::DataTypeStore> store_;

  // A pointer to the most recently used entry used for deduplication.
  raw_ptr<const SendTabToSelfEntry, DanglingUntriaged> mru_entry_;

  // The list of target devices, deduplicated and sorted by most recently used.
  std::vector<TargetDeviceInfo> target_device_info_sorted_list_;

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};
  base::ScopedObservation<syncer::DeviceInfoTracker,
                          syncer::DeviceInfoTracker::Observer>
      device_info_tracker_observation_{this};

  base::WeakPtrFactory<SendTabToSelfBridge> weak_ptr_factory_{this};
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BRIDGE_H_
