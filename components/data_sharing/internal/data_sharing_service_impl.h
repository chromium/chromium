// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SERVICE_IMPL_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SERVICE_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/version_info/channel.h"
#include "components/data_sharing/internal/collaboration_group_sync_bridge.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace data_sharing {
class DataSharingNetworkLoader;

// The internal implementation of the DataSharingService.
class DataSharingServiceImpl : public DataSharingService {
 public:
  DataSharingServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      syncer::OnceModelTypeStoreFactory model_type_store_factory,
      version_info::Channel channel);
  ~DataSharingServiceImpl() override;

  // Disallow copy/assign.
  DataSharingServiceImpl(const DataSharingServiceImpl&) = delete;
  DataSharingServiceImpl& operator=(const DataSharingServiceImpl&) = delete;

  // DataSharingService implementation.
  bool IsEmptyService() override;
  DataSharingNetworkLoader* GetDataSharingNetworkLoader() override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetCollaborationGroupControllerDelegate() override;
  void ReadAllGroups(
      base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)> callback)
      override;
  void ReadGroup(const std::string& group_id,
                 base::OnceCallback<void(const GroupDataOrFailureOutcome&)>
                     callback) override;
  void CreateGroup(const std::string& group_name,
                   base::OnceCallback<void(const GroupDataOrFailureOutcome&)>
                       callback) override;
  void DeleteGroup(
      const std::string& group_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void InviteMember(
      const std::string& group_id,
      const std::string& invitee_email,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void RemoveMember(
      const std::string& group_id,
      const std::string& member_email,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;

 private:
  std::unique_ptr<DataSharingNetworkLoader> data_sharing_network_loader_;
  std::unique_ptr<CollaborationGroupSyncBridge>
      collaboration_group_sync_bridge_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SERVICE_IMPL_H_
