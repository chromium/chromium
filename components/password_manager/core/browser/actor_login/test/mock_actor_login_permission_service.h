// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_MOCK_ACTOR_LOGIN_PERMISSION_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_MOCK_ACTOR_LOGIN_PERMISSION_SERVICE_H_

#include "base/functional/callback.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace actor_login {

class MockActorLoginPermissionService : public ActorLoginPermissionService {
 public:
  MockActorLoginPermissionService();
  ~MockActorLoginPermissionService() override;

  MOCK_METHOD(void,
              ListPermissions,
              (const std::vector<FederatedOrigins>&, ListPermissionsResult),
              (override));
  MOCK_METHOD(void, ListAllPermissions, (ListPermissionsResult), (override));
  MOCK_METHOD(void,
              DeletePermission,
              (const url::Origin&, const std::string&, DeletePermissionResult),
              (override));
  MOCK_METHOD(void,
              GrantPermission,
              (const FederatedPermission&, GrantPermissionResult),
              (override));
};

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_TEST_MOCK_ACTOR_LOGIN_PERMISSION_SERVICE_H_
