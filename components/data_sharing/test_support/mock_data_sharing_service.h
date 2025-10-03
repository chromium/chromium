// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_DATA_SHARING_SERVICE_H_
#define COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_DATA_SHARING_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/data_sharing/internal/preview_server_proxy.h"
#include "components/data_sharing/public/data_sharing_sdk_delegate.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace data_sharing {

// The mock implementation of the DataSharingService.
class MockDataSharingService : public DataSharingService {
 public:
  MockDataSharingService();
  ~MockDataSharingService() override;

  // Disallow copy/assign.
  MockDataSharingService(const MockDataSharingService&) = delete;
  MockDataSharingService& operator=(const MockDataSharingService&) = delete;

  // DataSharingService Impl.
  MOCK_METHOD0(IsEmptyService, bool());
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD0(GetDataSharingNetworkLoader, DataSharingNetworkLoader*());
  MOCK_METHOD0(GetCollaborationGroupControllerDelegate,
               base::WeakPtr<syncer::DataTypeControllerDelegate>());
  MOCK_METHOD0(IsGroupDataModelLoaded, bool());
  MOCK_METHOD1(ReadGroup, std::optional<GroupData>(const GroupId&));
  MOCK_METHOD0(ReadAllGroups, std::set<GroupData>());
  MOCK_METHOD2(GetPossiblyRemovedGroupMember,
               std::optional<GroupMemberPartialData>(const GroupId&,
                                                     const GaiaId&));
  MOCK_METHOD1(GetPossiblyRemovedGroup,
               std::optional<GroupData>(const GroupId&));
  MOCK_METHOD2(
      ReadGroupDeprecated,
      void(const GroupId&,
           base::OnceCallback<void(const GroupDataOrFailureOutcome&)>));
  MOCK_METHOD2(
      ReadNewGroup,
      void(const GroupToken&,
           base::OnceCallback<void(const GroupDataOrFailureOutcome&)>));
  MOCK_METHOD2(
      CreateGroup,
      void(const std::string&,
           base::OnceCallback<void(const GroupDataOrFailureOutcome&)>));
  MOCK_METHOD2(DeleteGroup,
               void(const GroupId&,
                    base::OnceCallback<void(PeopleGroupActionOutcome)>));
  MOCK_METHOD3(InviteMember,
               void(const GroupId&,
                    const std::string&,
                    base::OnceCallback<void(PeopleGroupActionOutcome)>));
  MOCK_METHOD3(AddMember,
               void(const GroupId&,
                    const std::string&,
                    base::OnceCallback<void(PeopleGroupActionOutcome)>));
  MOCK_METHOD3(RemoveMember,
               void(const GroupId&,
                    const std::string&,
                    base::OnceCallback<void(PeopleGroupActionOutcome)>));
  MOCK_METHOD2(LeaveGroup,
               void(const GroupId&,
                    base::OnceCallback<void(PeopleGroupActionOutcome)>));
  MOCK_METHOD1(IsLeavingOrDeletingGroup, bool(const GroupId&));
  MOCK_METHOD0(GetGroupEventsSinceStartup, std::vector<GroupEvent>());
  MOCK_METHOD2(HandleShareURLNavigationIntercepted,
               void(const GURL&, std::unique_ptr<ShareURLInterceptionContext>));
  MOCK_METHOD1(GetDataSharingUrl, std::unique_ptr<GURL>(const GroupData&));
  MOCK_METHOD2(
      EnsureGroupVisibility,
      void(const GroupId&,
           base::OnceCallback<void(const GroupDataOrFailureOutcome&)>));
  MOCK_METHOD2(
      GetSharedEntitiesPreview,
      void(const GroupToken&,
           base::OnceCallback<void(const SharedDataPreviewOrFailureOutcome&)>));
  MOCK_METHOD(void,
              GetAvatarImageForURL,
              (const GURL&,
               int,
               base::OnceCallback<void(const gfx::Image&)>,
               image_fetcher::ImageFetcher*));
  MOCK_METHOD1(SetSDKDelegate, void(std::unique_ptr<DataSharingSDKDelegate>));
  MOCK_METHOD0(GetSDKDelegate, DataSharingSDKDelegate*());
  MOCK_METHOD1(SetUIDelegate, void(std::unique_ptr<DataSharingUIDelegate>));
  MOCK_METHOD0(GetUiDelegate, DataSharingUIDelegate*());
  MOCK_METHOD0(GetLogger, Logger*());
  MOCK_METHOD1(AddGroupDataForTesting, void(GroupData));
  MOCK_METHOD1(SetPreviewServerProxyForTesting,
               void(std::unique_ptr<PreviewServerProxy>));
  MOCK_METHOD0(GetPreviewServerProxyForTesting, PreviewServerProxy*());
  MOCK_METHOD1(OnCollaborationGroupRemoved, void(const GroupId&));
  MOCK_METHOD1(IsContextIdShared, bool(const ContextId&));
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_DATA_SHARING_SERVICE_H_
