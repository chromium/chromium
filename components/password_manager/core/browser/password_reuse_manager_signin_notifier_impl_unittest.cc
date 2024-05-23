// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_manager_signin_notifier_impl.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/mock_password_reuse_manager.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace password_manager {
namespace {

class PasswordReuseManagerSigninNotifierImplTest : public testing::Test {
 public:
  PasswordReuseManagerSigninNotifierImplTest() = default;
  ~PasswordReuseManagerSigninNotifierImplTest() override = default;

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  MockPasswordReuseManager reuse_manager_;
};

// Checks that if a notifier is subscribed on sign-in events, then
// PasswordReuseManager receives sign-in notifications.
TEST_F(PasswordReuseManagerSigninNotifierImplTest, Subscribed) {
  PasswordReuseManagerSigninNotifierImpl notifier(identity_manager());
  notifier.SubscribeToSigninEvents(&reuse_manager_);
  identity_test_env()->MakePrimaryAccountAvailable("test@example.com",
                                                   signin::ConsentLevel::kSync);
  EXPECT_CALL(reuse_manager_, ClearAllGaiaPasswordHash());
  identity_test_env()->ClearPrimaryAccount();
  notifier.UnsubscribeFromSigninEvents();
}

// Checks that if a notifier is unsubscribed on sign-in events, then
// PasswordReuseManager receives no sign-in notifications.
TEST_F(PasswordReuseManagerSigninNotifierImplTest, Unsubscribed) {
  PasswordReuseManagerSigninNotifierImpl notifier(identity_manager());
  notifier.SubscribeToSigninEvents(&reuse_manager_);
  notifier.UnsubscribeFromSigninEvents();
  EXPECT_CALL(reuse_manager_, ClearAllGaiaPasswordHash()).Times(0);
  identity_test_env()->MakePrimaryAccountAvailable("test@example.com",
                                                   signin::ConsentLevel::kSync);
  identity_test_env()->ClearPrimaryAccount();
}

#if !BUILDFLAG(IS_IOS)
// This test is excluded from iOS since iOS does not support multiple Google
// accounts. Checks that ClearGaiaPasswordHash() is called when a secondary
// account is removed.
TEST_F(PasswordReuseManagerSigninNotifierImplTest, SignOutContentArea) {
  PasswordReuseManagerSigninNotifierImpl notifier(identity_manager());
  notifier.SubscribeToSigninEvents(&reuse_manager_);

  identity_test_env()->MakePrimaryAccountAvailable("username",
                                                   signin::ConsentLevel::kSync);
  EXPECT_CALL(reuse_manager_, ClearGaiaPasswordHash("username2"));
  auto* identity_manager = identity_test_env()->identity_manager();
  identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
      /*gaia_id=*/"secondary_account_id",
      /*email=*/"username2",
      /*refresh_token=*/"refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  // This call is necessary to ensure that the account removal is fully
  // processed in this testing context.
  identity_test_env()->EnableRemovalOfExtendedAccountInfo();
  identity_manager->GetAccountsMutator()->RemoveAccount(
      CoreAccountId::FromGaiaId("secondary_account_id"),
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  testing::Mock::VerifyAndClearExpectations(&reuse_manager_);

  EXPECT_CALL(reuse_manager_, ClearGaiaPasswordHash("username"));
  EXPECT_CALL(reuse_manager_, ClearAllGaiaPasswordHash());
  identity_test_env()->ClearPrimaryAccount();
  notifier.UnsubscribeFromSigninEvents();
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace password_manager
