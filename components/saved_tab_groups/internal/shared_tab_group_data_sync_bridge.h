// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_DATA_SYNC_BRIDGE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_DATA_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/proto/shared_tab_group_data.pb.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sync/base/collaboration_id.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/collaboration_metadata.h"
#include "components/sync/protocol/unique_position.pb.h"

class PrefService;

namespace data_sharing {
class Logger;
}  // namespace data_sharing

namespace syncer {
class DataTypeLocalChangeProcessor;
class MetadataChangeList;
}  // namespace syncer

namespace sync_pb {
class EntitySpecifics;
class SharedTabGroupDataSpecifics;
}  // namespace sync_pb

namespace tab_groups {
class SavedTabGroup;
class SyncBridgeTabGroupModelWrapper;

// Sync bridge implementation for SHARED_TAB_GROUP_DATA data type.
class SharedTabGroupDataSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  // `model_wrapper` and `pref_service` must not be null and must outlive the
  // current object.
  SharedTabGroupDataSyncBridge(
      SyncBridgeTabGroupModelWrapper* model_wrapper,
      syncer::OnceDataTypeStoreFactory create_store_callback,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      PrefService* pref_service,
      data_sharing::Logger* logger);

  SharedTabGroupDataSyncBridge(const SharedTabGroupDataSyncBridge&) = delete;
  SharedTabGroupDataSyncBridge& operator=(const SharedTabGroupDataSyncBridge&) =
      delete;
  ~SharedTabGroupDataSyncBridge() override;

  // DataTypeSyncBridge implementation.
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
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  bool SupportsGetClientTag() const override;
  bool SupportsGetStorageKey() const override;
  bool SupportsIncrementalUpdates() const override;
  bool SupportsUniquePositions() const override;
  sync_pb::UniquePosition GetUniquePosition(
      const sync_pb::EntitySpecifics& specifics) const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

  // Process creation of a new shared group. The added group must be shared.
  void SavedTabGroupAddedLocally(const base::Uuid& guid);

  // Process update to the existing group or tab (including moved tab). The
  // group must be shared.
  void SavedTabGroupUpdatedLocally(const base::Uuid& group_guid,
                                   const std::optional<base::Uuid>& tab_guid);

  // Process shared group deletion, the removed group must be shared.
  void SavedTabGroupRemovedLocally(const SavedTabGroup& removed_group);

  // Process updated local ID for the group.
  void ProcessTabGroupLocalIdChanged(const base::Uuid& group_guid);

  void UntrackEntitiesForCollaboration(
      const syncer::CollaborationId& collaboration_id);

 private:
  // Loads the data already stored in the DataTypeStore.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);

  // Calls ModelReadyToSync if there are no errors to report and propagaters the
  // stored entries to `on_load_callback`.
  void OnReadAllDataAndMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::DataTypeStore::RecordList> entries,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  // React to store failures if a save was not successful.
  void OnDatabaseSave(const std::optional<syncer::ModelError>& error);

  // Adds `specifics` into local storage (SavedTabGroupModel, and
  // DataTypeStore) and resolves any conflicts if `specifics` already exists
  // locally.  Additionally, the list of changes may not be complete and tabs
  // may have been sent before their groups have arrived. In this case, the tabs
  // are saved in the DataTypeStore but not in the model (and instead cached in
  // this class).
  std::optional<syncer::ModelError> AddGroupToLocalStorage(
      const sync_pb::SharedTabGroupDataSpecifics& specifics,
      const syncer::CollaborationMetadata& collaboration_metadata,
      base::Time creation_time,
      syncer::MetadataChangeList* metadata_change_list,
      syncer::DataTypeStore::WriteBatch& write_batch);
  std::optional<syncer::ModelError> ApplyRemoteTabUpdate(
      const sync_pb::SharedTabGroupDataSpecifics& specifics,
      syncer::MetadataChangeList* metadata_change_list,
      syncer::DataTypeStore::WriteBatch& write_batch,
      const std::set<base::Uuid>& tab_ids_with_pending_model_update,
      const syncer::CollaborationMetadata& collaboration_metadata,
      base::Time creation_time,
      base::Time modification_time);

  // Removes all data assigned to `storage_key` from local storage
  // (SavedTabGroupModel, and DataTypeStore). If a group is removed, all its
  // tabs will be removed in addition to the group.
  void DeleteDataFromLocalStorage(
      const std::string& storage_key,
      GaiaId removed_by,
      syncer::DataTypeStore::WriteBatch& write_batch);

  // Inform the processor of a new or updated Shared Tab Group or Tab.
  void SendToSync(sync_pb::SharedTabGroupDataSpecifics specific,
                  syncer::CollaborationMetadata collaboration_metadata,
                  base::Time creation_time,
                  syncer::MetadataChangeList* metadata_change_list);

  // Process local tab changes (add, remove, update), excluding changing tab's
  // position.
  void ProcessTabLocalChange(const SavedTabGroup& group,
                             const base::Uuid& tab_id,
                             syncer::DataTypeStore::WriteBatch& write_batch);

  // Removes the specifics pointed to by `guid` from the `store_`.
  void RemoveEntitySpecifics(const base::Uuid& guid,
                             syncer::DataTypeStore::WriteBatch& write_batch);

  // Returns unique position for the given tab in the `group`. `tab_index` must
  // be valid.
  sync_pb::UniquePosition CalculateUniquePosition(const SavedTabGroup& group,
                                                  size_t tab_index) const;

  // Calculates the position to insert a remote tab with the given unique
  // position. Always returns a valid value regardless input validness.
  // `tab_ids_to_ignore` is used to exclude tabs which will be updated later
  // from the comparison (see details in the implementation).
  size_t PositionToInsertRemoteTab(
      const sync_pb::UniquePosition& remote_unique_position,
      const SavedTabGroup& group,
      const std::set<base::Uuid>& tab_ids_to_ignore) const;

  // Creates `ongoing_write_batch_` if needed and returns a scoped closure
  // runner that will destroy the batch if needed when it goes out of scope.
  // When `store_write_batch_on_destroy` is false, the write batch is not
  // committed to the store when destroyed, and the caller is responsible for
  // committing it when needed. `store_write_batch_on_destroy` has no impact if
  // there is an ongoing write batch.
  base::ScopedClosureRunner CreateWriteBatchWithDestroyClosure(
      bool store_write_batch_on_destroy);

  // Destroys the ongoing write batch and commits it to the store if
  // `store_write_batch_on_destroy` is true.
  void DestroyOngoingWriteBatch(bool store_write_batch_on_destroy);

  // Returns true of the bridge is ready to sync and accept new local changes.
  bool IsReadyToSync() const;

  // Notifies the model on committed tab groups if there are any.
  void ProcessCommittedTabGroups();

  // Migration method to run after DB init when the shared tab group feature is
  // enabled from a disabled state. Clears out any non-empty local tab group ID
  // for the shared tab group entries and persists to DB. Run before the data is
  // published to the model. See comments in the method for more details.
  void FixLocalTabGroupIDsForSharedGroupsDuringFeatureEnabling(
      std::vector<proto::SharedTabGroupData>& stored_entries);

  // Resolves tabs missing groups by adding them to the model if a corresponding
  // group exists in the model.
  std::optional<syncer::ModelError> ResolveTabsMissingGroups(
      syncer::MetadataChangeList& metadata_change_list);

  // Converts a `group` to a `SharedTabGroupDataSpecifics` proto. The returned
  // specifics also contains unsupported fields that are stored in sync
  // metadata.
  sync_pb::SharedTabGroupDataSpecifics SharedTabGroupToSpecifics(
      const SavedTabGroup& group) const;

  // Converts a `tab` to a `SharedTabGroupDataSpecifics` proto. The returned
  // specifics also contains unsupported fields that are stored in sync
  // metadata.
  sync_pb::SharedTabGroupDataSpecifics SharedTabGroupTabToSpecifics(
      const SavedTabGroupTab& tab,
      sync_pb::UniquePosition unique_position) const;

  SEQUENCE_CHECKER(sequence_checker_);

  // In charge of actually persisting changes to disk, or loading previous data.
  std::unique_ptr<syncer::DataTypeStore> store_;

  // The write batch used to allow reentrancy calls during the processing of
  // remote updates. If `ongoing_write_batch_` is not null, it means that there
  // is likely an ongoing remote update processing. In this case methods should
  // reuse this object instead of creating a new write batch. Otherwise, the
  // write batch from processing remote updates may overwrite the changes once
  // committed.
  //
  // Use CreateWriteBatchWithDestroyClosure() to create this object because it
  // returns a scoped closure runner that will destroy the batch when it goes
  // out of scope. DestroyOngoingWriteBatch() can be called to destroy the batch
  // explicitly, but this is normally required only during remote updates.
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> ongoing_write_batch_;

  // The model wrapper used to access and mutate SavedTabGroupModel.
  raw_ptr<SyncBridgeTabGroupModelWrapper> model_wrapper_;

  // Whether shared tab group was enabled in last session. Used for migration.
  const bool did_enable_shared_tab_groups_in_last_session_;

  // Logger for logging to debug UI.
  raw_ptr<data_sharing::Logger> logger_ = nullptr;

  // List of tab groups waiting for being committed to the server.
  std::vector<base::Uuid> tab_groups_waiting_for_commit_;

  // Used to store tabs whose groups were not added locally yet. These tabs will
  // be removed after the TTL expires.
  struct TabMissingGroup {
    TabMissingGroup(sync_pb::SharedTabGroupDataSpecifics specifics,
                    syncer::CollaborationMetadata collaboration_metadata,
                    base::Time creation_time,
                    base::Time modification_time);
    TabMissingGroup(const TabMissingGroup&);
    TabMissingGroup& operator=(const TabMissingGroup&);
    TabMissingGroup(TabMissingGroup&&);
    TabMissingGroup& operator=(TabMissingGroup&&);
    ~TabMissingGroup();

    sync_pb::SharedTabGroupDataSpecifics specifics;
    syncer::CollaborationMetadata collaboration_metadata;
    base::Time creation_time;
    base::Time modification_time;
  };
  std::map<base::Uuid, TabMissingGroup> tabs_missing_groups_;

  // Allows safe temporary use of the SharedTabGroupDataSyncBridge object if it
  // exists at the time of use.
  base::WeakPtrFactory<SharedTabGroupDataSyncBridge> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_INTERNAL_SHARED_TAB_GROUP_DATA_SYNC_BRIDGE_H_
