// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SYNC_BRIDGE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SYNC_BRIDGE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class SavedTabGroupModel;

namespace syncer {
class MutableDataBatch;
class MetadataBatch;
class ModelError;
}  // namespace syncer

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
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);

  SavedTabGroupSyncBridge(const SavedTabGroupSyncBridge&) = delete;
  SavedTabGroupSyncBridge& operator=(const SavedTabGroupSyncBridge&) = delete;

  ~SavedTabGroupSyncBridge() override;

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  absl::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAddedLocally(const base::Uuid& guid) override;
  void SavedTabGroupRemovedLocally(const SavedTabGroup* removed_group) override;
  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const absl::optional<base::Uuid>& tab_guid = absl::nullopt) override;
  void SavedTabGroupReorderedLocally() override;

 private:
  // Updates and/or adds the specifics into the ModelTypeStore.
  void UpsertEntitySpecific(
      std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specifics,
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
  void AddEntryToBatch(
      syncer::MutableDataBatch* batch,
      std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specifics);

  // Inform the processor of a new or updated SavedTabGroupSpecifics and add the
  // necessary metadata changes into `metadata_change_list`.
  void SendToSync(std::unique_ptr<sync_pb::SavedTabGroupSpecifics> specifics,
                  syncer::MetadataChangeList* metadata_change_list);

  // Loads the data already stored in the ModelTypeStore.
  void OnStoreCreated(const absl::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::ModelTypeStore> store);

  // Loads all sync_pb::SavedTabGroupSpecifics stored in `entries` passing the
  // specifics into OnReadAllMetadata.
  void OnDatabaseLoad(
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries);

  // React to store failures if a save was not successful.
  void OnDatabaseSave(const absl::optional<syncer::ModelError>& error);

  // Calls ModelReadyToSync if there are no errors to report and loads the
  // stored entries into `model_`.
  void OnReadAllMetadata(
      std::unique_ptr<syncer::ModelTypeStore::RecordList> entries,
      const absl::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  // The ModelTypeStore used for local storage.
  std::unique_ptr<syncer::ModelTypeStore> store_;

  // The Model we used to represent the current state of SavedTabGroups.
  raw_ptr<SavedTabGroupModel> model_;

  // Used to store tabs whose groups were not added locally yet.
  std::vector<sync_pb::SavedTabGroupSpecifics> tabs_missing_groups_;

  // Observes the SavedTabGroupModel.
  base::ScopedObservation<SavedTabGroupModel, SavedTabGroupModelObserver>
      observation_{this};

  // Allows safe temporary use of the SavedTabGroupSyncBridge object if it
  // exists at the time of use.
  base::WeakPtrFactory<SavedTabGroupSyncBridge> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_SYNC_BRIDGE_H_
