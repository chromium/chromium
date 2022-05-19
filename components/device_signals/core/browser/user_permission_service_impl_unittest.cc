// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/user_permission_service_impl.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using policy::EnterpriseManagementAuthority;
using policy::ScopedManagementServiceOverrideForTesting;

namespace device_signals {

namespace {

constexpr char kUserGaiaId[] = "some-gaia-id";
constexpr char kUserEmail[] = "user@abc.com";
constexpr char kHostedDomain[] = "example.com";

class TestManagementService : public policy::ManagementService {
 public:
  TestManagementService() : ManagementService({}) {}
  explicit TestManagementService(
      std::vector<std::unique_ptr<policy::ManagementStatusProvider>> providers)
      : ManagementService(std::move(providers)) {}
  void SetManagementStatusProviderForTesting(
      std::vector<std::unique_ptr<policy::ManagementStatusProvider>>
          providers) {
    SetManagementStatusProvider(std::move(providers));
  }
};

}  // namespace

class UserPermissionServiceImplTest : public testing::Test {
 protected:
  UserPermissionServiceImplTest()
      : scoped_override_(&management_service_,
                         EnterpriseManagementAuthority::CLOUD_DOMAIN) {
    permission_service_ = std::make_unique<UserPermissionServiceImpl>(
        identity_test_env_.identity_manager(), &management_service_);
  }

  base::test::TaskEnvironment task_environment_;

  signin::IdentityTestEnvironment identity_test_env_;
  TestManagementService management_service_;
  ScopedManagementServiceOverrideForTesting scoped_override_;

  std::unique_ptr<UserPermissionServiceImpl> permission_service_;
};

// Tests CanCollectSignals with a missing user ID.
TEST_F(UserPermissionServiceImplTest, CanCollectSignals_EmptyUserId) {
  base::test::TestFuture<UserPermission> future;
  UserContext user_context;
  permission_service_->CanCollectSignals(user_context, future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kUnknownUser);
}

// Tests CanCollectSignals with a user ID that does not represent a valid user.
TEST_F(UserPermissionServiceImplTest, CanCollectSignals_UserId_NoUser) {
  base::test::TestFuture<UserPermission> future;
  UserContext user_context;
  user_context.user_id = kUserGaiaId;
  permission_service_->CanCollectSignals(user_context, future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kUnknownUser);
}

// Tests CanCollectSignals with a user ID that does not represent a managed
// user.
TEST_F(UserPermissionServiceImplTest, CanCollectSignals_User_NotManaged) {
  // Create known account.
  AccountInfo account = identity_test_env_.MakeAccountAvailableWithCookies(
      kUserEmail, kUserGaiaId);

  // Make sure there is no hosted domain.
  account.hosted_domain = "";
  identity_test_env_.UpdateAccountInfoForAccount(account);

  base::test::TestFuture<UserPermission> future;
  UserContext user_context;
  user_context.user_id = account.gaia;
  permission_service_->CanCollectSignals(user_context, future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kConsumerUser);
}

// Tests CanCollectSignals with a managed user ID but the browser is not
// managed.
TEST_F(UserPermissionServiceImplTest, CanCollectSignals_BrowserNotManaged) {
  // Set management to something other than CLOUD_DOMAIN.
  ScopedManagementServiceOverrideForTesting another_scope(
      &management_service_, EnterpriseManagementAuthority::CLOUD);

  // Create known account.
  AccountInfo account = identity_test_env_.MakeAccountAvailableWithCookies(
      kUserEmail, kUserGaiaId);

  // Make sure there is a hosted domain.
  account.hosted_domain = kHostedDomain;
  identity_test_env_.UpdateAccountInfoForAccount(account);

  base::test::TestFuture<UserPermission> future;
  UserContext user_context;
  user_context.user_id = account.gaia;
  permission_service_->CanCollectSignals(user_context, future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kMissingConsent);
}

// Tests CanCollectSignals with a managed user ID and the browser is managed.
// This is missing the affiliation check at the moment.
TEST_F(UserPermissionServiceImplTest, CanCollectSignals_BrowserManaged) {
  // Create known account.
  AccountInfo account = identity_test_env_.MakeAccountAvailableWithCookies(
      kUserEmail, kUserGaiaId);

  // Make sure there is a hosted domain.
  account.hosted_domain = kHostedDomain;
  identity_test_env_.UpdateAccountInfoForAccount(account);

  base::test::TestFuture<UserPermission> future;
  UserContext user_context;
  user_context.user_id = account.gaia;
  permission_service_->CanCollectSignals(user_context, future.GetCallback());
  EXPECT_EQ(future.Get(), UserPermission::kUnaffiliated);
}

}  // namespace device_signals
