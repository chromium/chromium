// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_DATA_SHARING_SERVICE_H_
#define COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_DATA_SHARING_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/data_sharing/public/data_sharing_service.h"
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
  MOCK_METHOD1(
      ReadAllGroups,
      void(base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)>));
  MOCK_METHOD2(
      ReadGroup,
      void(const GroupId&,
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
  MOCK_METHOD1(ShouldInterceptNavigationForShareURL, bool(const GURL&));
  MOCK_METHOD1(HandleShareURLNavigationIntercepted, void(const GURL&));
  MOCK_METHOD1(GetDataSharingURL, std::unique_ptr<GURL>(const GroupData&));
  MOCK_METHOD1(ParseDataSharingURL, ParseURLResult(const GURL&));
  MOCK_METHOD2(
      EnsureGroupVisibility,
      void(const GroupId&,
           base::OnceCallback<void(const GroupDataOrFailureOutcome&)>));
  MOCK_METHOD2(
      GetSharedEntitiesPreview,
      void(const GroupToken&,
           base::OnceCallback<void(const SharedDataPreviewOrFailureOutcome&)>));
  MOCK_METHOD0(GetUIDelegate, DataSharingUIDelegate*());
  MOCK_METHOD0(GetServiceStatus, ServiceStatus());
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_DATA_SHARING_SERVICE_H_
