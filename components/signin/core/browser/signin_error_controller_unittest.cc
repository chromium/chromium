// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_error_controller.h"

#include <stddef.h>

#include <functional>
#include <memory>

#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPrimaryAccountEmail[] = "primary@example.com";
constexpr char kTestEmail[] = "me@test.com";
constexpr char kOtherTestEmail[] = "you@test.com";

class MockSigninErrorControllerObserver
    : public SigninErrorController::Observer {
 public:
  MOCK_METHOD0(OnErrorChanged, void());
};

}  // namespace

TEST(SigninErrorControllerTest, SingleAccount) {
  MockSigninErrorControllerObserver observer;
  EXPECT_CALL(observer, OnErrorChanged()).Times(0);

  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      identity_test_env.identity_manager());
  ScopedObserver<SigninErrorController, SigninErrorController::Observer>
      scoped_observer(&observer);
  scoped_observer.Add(&error_controller);
  ASSERT_FALSE(error_controller.HasError());
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  // IdentityTestEnvironment does not call OnEndBatchChanges() as part of
  // MakeAccountAvailable(), and thus the signin error controller is not
  // updated.
  EXPECT_CALL(observer, OnErrorChanged()).Times(0);

  CoreAccountId test_account_id =
      identity_test_env.MakeAccountAvailable(kTestEmail).account_id;
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  GoogleServiceAuthError error1 =
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(observer, OnErrorChanged()).Times(1);
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      test_account_id, error1);
  EXPECT_TRUE(error_controller.HasError());
  EXPECT_EQ(error1, error_controller.auth_error());
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  GoogleServiceAuthError error2 =
      GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  EXPECT_CALL(observer, OnErrorChanged()).Times(1);
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      test_account_id, error2);
  EXPECT_TRUE(error_controller.HasError());
  EXPECT_EQ(error2, error_controller.auth_error());
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnErrorChanged()).Times(1);
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      test_account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(error_controller.HasError());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            error_controller.auth_error());
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST(SigninErrorControllerTest, AccountTransitionAnyAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;

  CoreAccountId test_account_id =
      identity_test_env.MakeAccountAvailable(kTestEmail).account_id;
  CoreAccountId other_test_account_id =
      identity_test_env.MakeAccountAvailable(kOtherTestEmail).account_id;
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      identity_test_env.identity_manager());
  ASSERT_FALSE(error_controller.HasError());

  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      test_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      other_test_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_TRUE(error_controller.HasError());
  ASSERT_EQ(test_account_id, error_controller.error_account_id());

  // Now resolve the auth errors - the menu item should go away.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      test_account_id, GoogleServiceAuthError::AuthErrorNone());
  ASSERT_FALSE(error_controller.HasError());
}

// This test exercises behavior on signin/signout, which is not relevant on
// ChromeOS.
#if !defined(OS_CHROMEOS)
TEST(SigninErrorControllerTest, AccountTransitionPrimaryAccount) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;
  signin::PrimaryAccountMutator* primary_account_mutator =
      identity_test_env.identity_manager()->GetPrimaryAccountMutator();

  CoreAccountId test_account_id =
      identity_test_env.MakeAccountAvailable(kTestEmail).account_id;
  CoreAccountId other_test_account_id =
      identity_test_env.MakeAccountAvailable(kOtherTestEmail).account_id;
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::PRIMARY_ACCOUNT,
      identity_test_env.identity_manager());
  ASSERT_FALSE(error_controller.HasError());

  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      test_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      other_test_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_FALSE(error_controller.HasError());  // No primary account.

  // Set the primary account.
  identity_test_env.SetPrimaryAccount(kOtherTestEmail);

  ASSERT_FALSE(error_controller.HasError());  // Error is on secondary.

  // Change the primary account to the account with an error and check that the
  // error controller updates its error status accordingly.
  primary_account_mutator->ClearPrimaryAccount(
      signin::PrimaryAccountMutator::ClearAccountsAction::kKeepAll,
      signin_metrics::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  identity_test_env.SetPrimaryAccount(kTestEmail);
  ASSERT_TRUE(error_controller.HasError());
  ASSERT_EQ(test_account_id, error_controller.error_account_id());

  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      other_test_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_TRUE(error_controller.HasError());
  ASSERT_EQ(test_account_id, error_controller.error_account_id());

  // Change the primary account again and check that the error controller
  // updates its error status accordingly.
  primary_account_mutator->ClearPrimaryAccount(
      signin::PrimaryAccountMutator::ClearAccountsAction::kKeepAll,
      signin_metrics::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  identity_test_env.SetPrimaryAccount(kOtherTestEmail);
  ASSERT_TRUE(error_controller.HasError());
  ASSERT_EQ(other_test_account_id, error_controller.error_account_id());

  // Sign out and check that that the error controller updates its error status
  // accordingly.
  primary_account_mutator->ClearPrimaryAccount(
      signin::PrimaryAccountMutator::ClearAccountsAction::kKeepAll,
      signin_metrics::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  ASSERT_FALSE(error_controller.HasError());
}
#endif

// Verify that SigninErrorController handles errors properly.
TEST(SigninErrorControllerTest, AuthStatusEnumerateAllErrors) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;

  CoreAccountId test_account_id =
      identity_test_env.MakeAccountAvailable(kTestEmail).account_id;
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      identity_test_env.identity_manager());

  GoogleServiceAuthError::State table[] = {
      GoogleServiceAuthError::NONE,
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
      GoogleServiceAuthError::USER_NOT_SIGNED_UP,
      GoogleServiceAuthError::CONNECTION_FAILED,
      GoogleServiceAuthError::SERVICE_UNAVAILABLE,
      GoogleServiceAuthError::REQUEST_CANCELED,
      GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE,
      GoogleServiceAuthError::SERVICE_ERROR};
  static_assert(
      base::size(table) == GoogleServiceAuthError::NUM_STATES -
                               GoogleServiceAuthError::kDeprecatedStateCount,
      "table array does not match the number of auth error types");

  for (GoogleServiceAuthError::State state : table) {
    GoogleServiceAuthError error(state);

    if (error.IsTransientError())
      continue;  // Only persistent errors or non-errors are reported.

    identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
        test_account_id, error);

    EXPECT_EQ(error_controller.HasError(), error.IsPersistentError());

    if (error.IsPersistentError()) {
      EXPECT_EQ(state, error_controller.auth_error().state());
      EXPECT_EQ(test_account_id, error_controller.error_account_id());
    } else {
      EXPECT_EQ(GoogleServiceAuthError::NONE,
                error_controller.auth_error().state());
      EXPECT_EQ(CoreAccountId(), error_controller.error_account_id());
    }
  }
}

// Verify that existing error is not replaced by new error.
TEST(SigninErrorControllerTest, AuthStatusChange) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;

  CoreAccountId test_account_id =
      identity_test_env.MakeAccountAvailable(kTestEmail).account_id;
  CoreAccountId other_test_account_id =
      identity_test_env.MakeAccountAvailable(kOtherTestEmail).account_id;
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      identity_test_env.identity_manager());
  ASSERT_FALSE(error_controller.HasError());

  // Set an error for other_test_account_id.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      test_account_id, GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      other_test_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            error_controller.auth_error().state());
  ASSERT_EQ(other_test_account_id, error_controller.error_account_id());

  // Change the error for other_test_account_id.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      other_test_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));
  ASSERT_EQ(GoogleServiceAuthError::SERVICE_ERROR,
            error_controller.auth_error().state());
  ASSERT_EQ(other_test_account_id, error_controller.error_account_id());

  // Set the error for test_account_id -- nothing should change.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      test_account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE));
  ASSERT_EQ(GoogleServiceAuthError::SERVICE_ERROR,
            error_controller.auth_error().state());
  ASSERT_EQ(other_test_account_id, error_controller.error_account_id());

  // Clear the error for other_test_account_id, so the test_account_id's error
  // is used.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      other_test_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_EQ(GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE,
            error_controller.auth_error().state());
  ASSERT_EQ(test_account_id, error_controller.error_account_id());

  // Clear the remaining error.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      test_account_id, GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_FALSE(error_controller.HasError());
}

TEST(SigninErrorControllerTest,
     PrimaryAccountErrorsArePreferredToSecondaryAccountErrors) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;

  AccountInfo primary_account_info =
      identity_test_env.MakePrimaryAccountAvailable(kPrimaryAccountEmail);
  CoreAccountId secondary_account_id =
      identity_test_env.MakeAccountAvailable(kTestEmail).account_id;
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      identity_test_env.identity_manager());
  ASSERT_FALSE(error_controller.HasError());

  // Set an error for the Secondary Account.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            error_controller.auth_error().state());
  ASSERT_EQ(secondary_account_id, error_controller.error_account_id());

  // Set an error for the Primary Account. This should override the previous
  // error.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            error_controller.auth_error().state());
  ASSERT_EQ(primary_account_info.account_id,
            error_controller.error_account_id());

  // Clear the Primary Account error. This should cause the Secondary Account
  // error to be returned again.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            error_controller.auth_error().state());
  ASSERT_EQ(secondary_account_id, error_controller.error_account_id());

  // Clear the Secondary Account error too. All errors should be gone now.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_FALSE(error_controller.HasError());
}

TEST(SigninErrorControllerTest, PrimaryAccountErrorsAreSticky) {
  base::test::TaskEnvironment task_environment;
  signin::IdentityTestEnvironment identity_test_env;

  AccountInfo primary_account_info =
      identity_test_env.MakePrimaryAccountAvailable(kPrimaryAccountEmail);
  CoreAccountId secondary_account_id =
      identity_test_env.MakeAccountAvailable(kTestEmail).account_id;
  SigninErrorController error_controller(
      SigninErrorController::AccountMode::ANY_ACCOUNT,
      identity_test_env.identity_manager());
  ASSERT_FALSE(error_controller.HasError());

  // Set an error for the Primary Account.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            error_controller.auth_error().state());
  ASSERT_EQ(primary_account_info.account_id,
            error_controller.error_account_id());

  // Set an error for the Secondary Account. The Primary Account error should
  // stick.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            error_controller.auth_error().state());
  ASSERT_EQ(primary_account_info.account_id,
            error_controller.error_account_id());

  // Clear the Primary Account error. This should cause the Secondary Account
  // error to be returned again.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            error_controller.auth_error().state());
  ASSERT_EQ(secondary_account_id, error_controller.error_account_id());

  // Clear the Secondary Account error too. All errors should be gone now.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  ASSERT_FALSE(error_controller.HasError());
}
