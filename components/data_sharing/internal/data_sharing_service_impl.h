// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SERVICE_IMPL_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SERVICE_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/version_info/channel.h"
#include "components/data_sharing/internal/collaboration_group_sync_bridge.h"
#include "components/data_sharing/internal/preview_server_proxy.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/data_sharing_ui_delegate.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace data_sharing_pb {

class AddAccessTokenResult;
class CreateGroupResult;
class LookupGaiaIdByEmailResult;
class ReadGroupsResult;
class LookupGaiaIdByEmailResult;

}  // namespace data_sharing_pb

namespace data_sharing {
class DataSharingNetworkLoader;
class PreviewServerProxy;

// The internal implementation of the DataSharingService.
class DataSharingServiceImpl : public DataSharingService,
                               public CollaborationGroupSyncBridge::Observer {
 public:
  // `identity_manager` must not be null and must outlive this object.
  // `sdk_delegate` is nullable, indicating that SDK is not available.
  DataSharingServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      syncer::OnceDataTypeStoreFactory data_type_store_factory,
      version_info::Channel channel,
      std::unique_ptr<DataSharingSDKDelegate> sdk_delegate,
      std::unique_ptr<DataSharingUIDelegate> ui_delegate);
  ~DataSharingServiceImpl() override;

  // Disallow copy/assign.
  DataSharingServiceImpl(const DataSharingServiceImpl&) = delete;
  DataSharingServiceImpl& operator=(const DataSharingServiceImpl&) = delete;

  // DataSharingService implementation.
  bool IsEmptyService() override;
  void AddObserver(DataSharingService::Observer* observer) override;
  void RemoveObserver(DataSharingService::Observer* observer) override;
  DataSharingNetworkLoader* GetDataSharingNetworkLoader() override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetCollaborationGroupControllerDelegate() override;
  void ReadAllGroups(
      base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)> callback)
      override;
  void ReadGroup(const GroupId& group_id,
                 base::OnceCallback<void(const GroupDataOrFailureOutcome&)>
                     callback) override;
  void CreateGroup(const std::string& group_name,
                   base::OnceCallback<void(const GroupDataOrFailureOutcome&)>
                       callback) override;
  void DeleteGroup(
      const GroupId& group_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void InviteMember(
      const GroupId& group_id,
      const std::string& invitee_email,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void AddMember(
      const GroupId& group_id,
      const std::string& access_token,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void RemoveMember(
      const GroupId& group_id,
      const std::string& member_email,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  bool ShouldInterceptNavigationForShareURL(const GURL& url) override;
  void HandleShareURLNavigationIntercepted(const GURL& url) override;
  std::unique_ptr<GURL> GetDataSharingURL(const GroupData& group_data) override;
  ParseURLResult ParseDataSharingURL(const GURL& url) override;
  void Shutdown() override;
  void EnsureGroupVisibility(
      const GroupId& group_id,
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback)
      override;
  void GetSharedEntitiesPreview(
      const GroupToken& group_token,
      base::OnceCallback<void(const SharedDataPreviewOrFailureOutcome&)>
          callback) override;
  DataSharingUIDelegate* GetUIDelegate() override;
  ServiceStatus GetServiceStatus() override;

  // CollaborationGroupSyncBridge::Observer implementation.
  void OnGroupsUpdated(const std::vector<GroupId>& added_group_ids,
                       const std::vector<GroupId>& updated_group_ids,
                       const std::vector<GroupId>& deleted_group_ids) override;
  void OnDataLoaded() override;

  CollaborationGroupSyncBridge* GetCollaborationGroupSyncBridgeForTesting();

 private:
  void OnReadSingleGroupCompleted(
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback,
      const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
          result);
  void OnReadAllGroupsCompleted(
      base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)> callback,
      const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
          result);
  void OnCreateGroupCompleted(
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback,
      const base::expected<data_sharing_pb::CreateGroupResult, absl::Status>&
          result);
  void OnGaiaIdLookupForAddMemberCompleted(
      const GroupId& group_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback,
      const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                           absl::Status>& result);
  void OnGaiaIdLookupForRemoveMemberCompleted(
      const GroupId& group_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback,
      const base::expected<data_sharing_pb::LookupGaiaIdByEmailResult,
                           absl::Status>& result);
  void OnReadGroupsToNotifyObserversCompleted(
      const std::set<GroupId>& added_group_ids,
      const std::set<GroupId>& updated_group_ids,
      const base::expected<data_sharing_pb::ReadGroupsResult, absl::Status>&
          read_groups_result);

  // Converts absl::Status to PeopleGroupActionOutcome and passes it to
  // `callback`, used by DeleteGroup(), InviteMember(), and RemoveMember()
  // flows.
  void OnSimpleGroupActionCompleted(
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback,
      const absl::Status& result);
  void OnAccessTokenAdded(
      base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback,
      const base::expected<data_sharing_pb::AddAccessTokenResult, absl::Status>&
          result);

  ServiceStatus current_status_;
  // It must be destroyed after the `sdk_delegate_` member because
  // `sdk_delegate` needs the `data_sharing_network_loader_`.
  std::unique_ptr<DataSharingNetworkLoader> data_sharing_network_loader_;
  std::unique_ptr<CollaborationGroupSyncBridge>
      collaboration_group_sync_bridge_;
  // Nullable.
  std::unique_ptr<DataSharingSDKDelegate> sdk_delegate_;
  std::unique_ptr<DataSharingUIDelegate> ui_delegate_;

  base::ObserverList<DataSharingService::Observer> observers_;
  std::unique_ptr<PreviewServerProxy> preview_server_proxy_;

  base::WeakPtrFactory<DataSharingServiceImpl> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SERVICE_IMPL_H_
