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
  MOCK_METHOD(void,
              StartJoinFlow,
              (std::unique_ptr<CollaborationControllerDelegate> delegate,
               const GURL& url),
              (override));
  MOCK_METHOD(void,
              StartShareFlow,
              (std::unique_ptr<CollaborationControllerDelegate> delegate,
               tab_groups::EitherGroupID either_id),
              (override));
  MOCK_METHOD(ServiceStatus, GetServiceStatus, (), (override));
  MOCK_METHOD(data_sharing::MemberRole,
              GetCurrentUserRoleForGroup,
              (const data_sharing::GroupId& group_id),
              (override));
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COLLABORATION_SERVICE_H_
