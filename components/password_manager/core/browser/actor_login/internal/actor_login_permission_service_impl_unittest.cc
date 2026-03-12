// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_permission_service_impl.h"

#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor_login {

class ActorLoginPermissionServiceImplTest : public testing::Test {
 protected:
  ActorLoginPermissionServiceImpl service_;
};

TEST_F(ActorLoginPermissionServiceImplTest, ListAllPermissionsReturnsEmpty) {
  base::test::TestFuture<std::vector<FederatedPermission>> future;
  service_.ListAllPermissions(future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

}  // namespace actor_login
