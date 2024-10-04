// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_MODEL_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_MODEL_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/data_sharing/internal/collaboration_group_sync_bridge.h"
#include "components/data_sharing/internal/group_data_store.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/public/protocol/data_sharing_sdk.pb.h"

namespace data_sharing {

// This class manages GroupData and ensures it is synchronized:
// * Provides in-memory and persistent storage for GroupData by incapsulating
// database that stores known GroupData.
// * Observes changes in CollaborationGroupSyncBridge and reflects them in
// cache/DB, retrieving data from SDK when needed.
class GroupDataModel : public CollaborationGroupSyncBridge::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;

    // TODO(crbug.com/301390275): Revisit observer methods, in particular
    // include something around update deltas.

    // Indicates that data is loaded from the disk, it can still be stale
    // though. GetGroup() / GetAllGroups() returns no data prior to this call.
    virtual void OnModelLoaded() = 0;

    virtual void OnGroupAdded(const GroupId& group_id) = 0;
    virtual void OnGroupUpdated(const GroupId& group_id) = 0;
    virtual void OnGroupDeleted(const GroupId& group_id) = 0;
  };

  // `collaboration_group_sync_bridge` and `sdk_delegate` must not be null and
  // must outlive `this`.
  GroupDataModel(const base::FilePath& data_sharing_dir,
                 CollaborationGroupSyncBridge* collaboration_group_sync_bridge,
                 DataSharingSDKDelegate* sdk_delegate);
  ~GroupDataModel() override;

  GroupDataModel(const GroupDataModel&) = delete;
  GroupDataModel& operator=(const GroupDataModel&) = delete;
  GroupDataModel(GroupDataModel&&) = delete;
  GroupDataModel& operator=(GroupDataModel&&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Indicates whether data is loaded from the disk, it can still be stale
  // though. GetGroup() / GetAllGroups() returns no data prior as long as it is
  // set to false.
  bool IsModelLoaded() const;

  // Returns nullopt if the group is not (yet) stored locally or doesn't exist.
  std::optional<GroupData> GetGroup(const GroupId& group_id) const;
  // Groups are ordered by id.
  std::set<GroupData> GetAllGroups() const;

  // CollaborationGroupSyncBridge::Observer implementation.
  void OnGroupsUpdated(const std::vector<GroupId>& added_group_ids,
                       const std::vector<GroupId>& updated_group_ids,
                       const std::vector<GroupId>& deleted_group_ids) override;
  // TODO(crbug.com/301390275): refer to collaborations in the name, since it is
  // currently confusing (we have model, store and bridge, that are all
  // indicates that their state is loaded).
  void OnDataLoaded() override;

  GroupDataStore& GetGroupDataStoreForTesting();

 private:
  void OnGroupDataStoreLoaded(GroupDataStore::DBInitStatus status);
  // `collaboration_group_sync_bridge_` and `group_data_store_` might be out of
  // sync on startup, this method handles all missed deletions and updates.
  void ProcessInitialData();

  // Asynchronously fetches data from the SDK.
  void FetchGroupsFromSDK(const std::vector<GroupId>& added_or_updated_groups);
  void OnGroupsFetchedFromSDK(
      const std::map<GroupId, VersionToken>& requested_groups_and_versions,
      const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
          read_groups_result);

  GroupDataStore group_data_store_;
  bool is_group_data_store_loaded_ = false;
  bool is_collaboration_group_bridge_loaded_ = false;

  raw_ptr<CollaborationGroupSyncBridge> collaboration_group_sync_bridge_;
  raw_ptr<DataSharingSDKDelegate> sdk_delegate_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<GroupDataModel> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_MODEL_H_
