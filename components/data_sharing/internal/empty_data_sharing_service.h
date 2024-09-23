// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_EMPTY_DATA_SHARING_SERVICE_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_EMPTY_DATA_SHARING_SERVICE_H_

#include "components/data_sharing/public/data_sharing_service.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace data_sharing {

// An empty implementation of DataSharingService that can be used when the
// data sharing feature is disabled.
class EmptyDataSharingService : public DataSharingService {
 public:
  EmptyDataSharingService();
  ~EmptyDataSharingService() override;

  // Disallow copy/assign.
  EmptyDataSharingService(const EmptyDataSharingService&) = delete;
  EmptyDataSharingService& operator=(const EmptyDataSharingService&) = delete;

  // DataSharingService implementation.
  bool IsEmptyService() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
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
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_EMPTY_DATA_SHARING_SERVICE_H_
