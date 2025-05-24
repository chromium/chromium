// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COLLABORATION_SERVICE_H_
#define COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COLLABORATION_SERVICE_H_

#include "components/collaboration/public/collaboration_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace collaboration {

class MockCollaborationService : public CollaborationService {
 public:
  MockCollaborationService();
  ~MockCollaborationService() override;

  MOCK_METHOD(bool, IsEmptyService, (), (override));
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD(void,
              StartJoinFlow,
              (std::unique_ptr<CollaborationControllerDelegate> delegate,
               const GURL& url),
              (override));
  MOCK_METHOD(void,
              StartShareOrManageFlow,
              (std::unique_ptr<CollaborationControllerDelegate> delegate,
               const tab_groups::EitherGroupID& either_id,
               CollaborationServiceShareOrManageEntryPoint entry),
              (override));
  MOCK_METHOD(void,
              StartLeaveOrDeleteFlow,
              (std::unique_ptr<CollaborationControllerDelegate> delegate,
               const tab_groups::EitherGroupID& either_id,
               CollaborationServiceLeaveOrDeleteEntryPoint entry),
              (override));
  MOCK_METHOD(void, CancelAllFlows, (base::OnceCallback<void()>), (override));
  MOCK_METHOD(ServiceStatus, GetServiceStatus, (), (override));
  MOCK_METHOD(void,
              OnSyncServiceInitialized,
              (syncer::SyncService*),
              (override));
  MOCK_METHOD(data_sharing::MemberRole,
              GetCurrentUserRoleForGroup,
              (const data_sharing::GroupId& group_id),
              (override));
  MOCK_METHOD(std::optional<data_sharing::GroupData>,
              GetGroupData,
              (const data_sharing::GroupId& group_id),
              (override));
  MOCK_METHOD2(DeleteGroup,
               void(const data_sharing::GroupId&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD2(LeaveGroup,
               void(const data_sharing::GroupId&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD1(ShouldInterceptNavigationForShareURL, bool(const GURL& url));
  MOCK_METHOD(
      void,
      HandleShareURLNavigationIntercepted,
      (const GURL& url,
       std::unique_ptr<data_sharing::ShareURLInterceptionContext> context,
       CollaborationServiceJoinEntryPoint entry),
      (override));
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COLLABORATION_SERVICE_H_
