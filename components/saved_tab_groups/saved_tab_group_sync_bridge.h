// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SYNC_BRIDGE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "components/saved_tab_groups/proto/saved_tab_group_data.pb.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/saved_tab_group_specifics.pb.h"

class PrefService;

namespace syncer {
class MutableDataBatch;
class MetadataBatch;
class ModelError;
}  // namespace syncer

namespace tab_groups {
class SavedTabGroupModel;

// The SavedTabGroupSyncBridge is responsible for synchronizing and resolving
// conflicts between the data stored in the sync server and what is currently
// stored in the SavedTabGroupModel. Once synchronized, this data is stored in
// the ModelTypeStore for local persistence across sessions.
class SavedTabGroupSyncBridge : public syncer::ModelTypeSyncBridge,
                                public SavedTabGroupModelObserver {
 public:
  explicit SavedTabGroupSyncBridge(
      SavedTabGroupModel* model,
      syncer::OnceModelTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      PrefService* pref_service,
      std::map<base::Uuid, LocalTabGroupID> migrated_android_local_ids);

  SavedTabGroupSyncBridge(const SavedTabGroupSyncBridge&) = delete;
  SavedTabGroupSyncBridge& operator=(const SavedTabGroupSyncBridge&) = delete;

  ~SavedTabGroupSyncBridge() override;

  // syncer::ModelTypeSyncBridge:
  void OnSyncStarting(
      const syncer::DataTypeActivationRequest& request) override;
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  syncer::ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const syncer::EntityData& remote_data) const override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  void GetDataForCommit(StorageKeyList storage_keys,
                        DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup* removed_group) override;
  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid = std::nullopt) override;
  void SavedTabGroupTabsReorderedLocally(const base::Uuid& group_guid) override;
  void SavedTabGroupReorderedLocally() override;
  void SavedTabGroupLocalIdChanged(const base::Uuid& group_guid) override;
  void SavedTabGroupLastUserInteractionTimeUpdated(
      const base::Uuid& group_guid) override;

  const std::vector<proto::SavedTabGroupData>&
  GetTabsMissingGroupsForTesting() {
    return tabs_missing_groups_;
  }

  // Returns the cache guid the change processor holds if metadata is tracked.
  // otherwise returns a nullopt.
  std::optional<std::string> GetLocalCacheGuid() const;

  // Whether the sync is currently enabled and syncing for saved tab groups.
  // False before bridge initialization is completed.
  bool IsSyncing() const;

  static SavedTabGroup SpecificsToSavedTabGroupForTest(
      const sync_pb::SavedTabGroupSpecifics& specifics);
  static sync_pb::SavedTabGroupSpecifics SavedTabGroupToSpecificsForTest(
      const SavedTabGroup& group);
  static SavedTabGroupTab SpecificsToSavedTabGroupTabForTest(
      const sync_pb::SavedTabGroupSpecifics& specifics);
  static sync_pb::SavedTabGroupSpecifics SavedTabGroupTabToSpecificsForTest(
      const SavedTabGroupTab& tab);

  static SavedTabGroup DataToSavedTabGroupForTest(
      const proto::SavedTabGroupData& data);
  static proto::SavedTabGroupData SavedTabGroupToDataForTest(
      const SavedTabGroup& group);
  static SavedTabGroupTab DataToSavedTabGroupTabForTest(
      const proto::SavedTabGroupData& data);
  static proto::SavedTabGroupData SavedTabGroupTabToDataForTest(
      const SavedTabGroupTab& tab);

 private:
  // Updates and/or adds the specifics into the ModelTypeStore.
  void UpsertEntitySpecific(const proto::SavedTabGroupData& data,
                            syncer::ModelTypeStore::WriteBatch* write_batch);

  // Removes the specifics pointed to by `guid` from the ModelTypeStore.
  void RemoveEntitySpecific(const base::Uuid& guid,
                            syncer::ModelTypeStore::WriteBatch* write_batch);

  // Adds `specifics` into local storage (SavedTabGroupModel, and
  // ModelTypeStore) and resolves any conflicts if `specifics` already exists
  // locally. `notify_sync` is true when MergeFullSyncData is called and there
  // is a conflict between the received and local data. Accordingly, after the
  // conflict has been resolved, we will want to update sync with this merged
  // data. `notify_sync` is false in cases that would cause a cycle such as when
  // ApplyIncrementalSyncChanges is called. Additionally, the list of changes
  // may not be complete and tabs may have been sent before their groups have
  // arrived. In this case, the tabs are saved in the ModelTypeStore but not in
  // the model (and instead cached in this class).
  void AddDataToLocalStorage(const sync_pb::SavedTabGroupSpecifics& specifics,
                             syncer::MetadataChangeList* metadata_change_list,
                             syncer::ModelTypeStore::WriteBatch* write_batch,
                             bool notify_sync);

  // Removes all data assigned to `guid` from local storage (SavedTabGroupModel,
  // and ModelTypeStore). If this guid represents a group, all tabs will be
  // removed in addition to the group.
  void DeleteDataFromLocalStorage(
      const base::Uuid& guid,
      syncer::ModelTypeStore::WriteBatch* write_batch);

  // Attempts to add the tabs found in `tabs_missing_groups_` to local storage.
  void ResolveTabsMissingGroups(
      syncer::ModelTypeStore::WriteBatch* write_batch);

  // Adds the entry into `batch`.
  void AddEntryToBatch(syncer::MutableDataBatch* batch,
                       proto::SavedTabGroupData specifics);

  // Inform the processor of a new or updated SavedTabGroupSpecifics and add the
  // necessary metadata changes into `metadata_change_list`.
  void SendToSync(sync_pb::SavedTabGroupSpecifics specifics,
                  syncer::MetadataChangeList* metadata_change_list);

  // Loads the data already stored in the ModelTypeStore.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);

  // Loads all sync_pb::SavedTabGroupSpecifics stored in `entries` passing the
  // specifics into OnReadAllMetadata.
  void OnDatabaseLoad(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries);

  // React to store failures if a save was not successful.
  void OnDatabaseSave(const std::optional<syncer::ModelError>& error);

  // Calls ModelReadyToSync if there are no errors to report and loads the
  // stored entries into `model_`.
  void OnReadAllMetadata(
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries,
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  // Callback passed to `LoadStoredEntries` to start observing model after
  // loading the stored entries.
  void StartObservingModel();

  // Called to migrate the SavedTabGroupSpecfics to SavedTabGroupData.
  void MigrateSpecificsToSavedTabGroupData(
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries);
  void OnSpecificsToDataMigrationComplete(
      const std::optional<syncer::ModelError>& error);

  // Called to migrate the Android local IDs from shared prefs to
  // SavedTabGroupData.
  void MigrateAndroidLocalIds(
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries);
  void OnAndroidLocalIdMigrationComplete(
      const std::optional<syncer::ModelError>& error);

  // Called to update the cache guid of groups and tabs with latest cache guid
  // and subsequently writes the updated data to storage.
  void UpdateLocalCacheGuidForGroups(
      syncer::ModelTypeStore::WriteBatch* write_batch);

  // Helper method to determine if a tab group was created from a remote device
  // based on the group's cache guid.
  bool IsRemoteGroup(const SavedTabGroup& group);

  // The ModelTypeStore used for local storage.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  // The Model we used to represent the current state of SavedTabGroups.
  raw_ptr<SavedTabGroupModel> model_;

  // The pref service for storing migration status.
  raw_ptr<PrefService> pref_service_;

  // Used to store tabs whose groups were not added locally yet.
  std::vector<proto::SavedTabGroupData> tabs_missing_groups_;

  // Observes the SavedTabGroupModel.
  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};

  // Sync to local ID map for Android during shared prefs migration. This will
  // be non-empty during first time migration and always empty after subsequent
  // restart.
  std::map<base::Uuid, LocalTabGroupID> migrated_android_local_ids_;

  // Only for metrics. Used to ensure that a certain metrics is recorded at max
  // once per chrome session.
  bool migration_already_complete_recorded_ = false;

  // Allows safe temporary use of the SavedTabGroupSyncBridge object if it
  // exists at the time of use.
  base::WeakPtrFactory<SavedTabGroupSyncBridge> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SYNC_BRIDGE_H_
