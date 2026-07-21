// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/promos/signin_promo_tab_helper.h"

#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SigninPromoTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  SigninPromoTabHelperTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~SigninPromoTabHelperTest() override = default;

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  // Helper to quickly make an account available as primary with a specific
  // access point.
  AccountInfo MakePrimaryAccountWithAccessPoint(
      const std::string& email,
      signin_metrics::AccessPoint access_point) {
    return signin::MakeAccountAvailable(
        identity_manager(), signin::AccountAvailabilityOptionsBuilder()
                                .AsPrimary(signin::ConsentLevel::kSignin)
                                .WithAccessPoint(access_point)
                                .Build(email));
  }

  // Helper to simulate sign-in pending (re-auth) state for a given account and
  // access point.
  AccountInfo SetupReauthPendingState(
      const std::string& email,
      signin_metrics::AccessPoint access_point) {
    AccountInfo info = MakePrimaryAccountWithAccessPoint(email, access_point);
    signin::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_manager(), info.account_id,
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));
    EXPECT_TRUE(signin_util::IsSigninPending(identity_manager()));
    return info;
  }
};

TEST_F(SigninPromoTabHelperTest, CallbackFiresOnSuccessfulSignIn) {
  base::test::TestFuture<void> future;

  SigninPromoTabHelper::GetForWebContents(*web_contents())
      ->InitializeCallbackAfterSignIn(
          future.GetCallback(),
          signin_metrics::AccessPoint::kSendTabToSelfPromo);

  MakePrimaryAccountWithAccessPoint(
      "test@gmail.com", signin_metrics::AccessPoint::kSendTabToSelfPromo);

  EXPECT_TRUE(future.Wait());
}

TEST_F(SigninPromoTabHelperTest, CallbackDoesNotFireIfTimeoutExceeded) {
  base::test::TestFuture<void> future;

  SigninPromoTabHelper::GetForWebContents(*web_contents())
      ->InitializeCallbackAfterSignIn(
          future.GetCallback(),
          signin_metrics::AccessPoint::kSendTabToSelfPromo);

  // Fast-forward by 51 minutes (exceeds the 50-minute timeout).
  task_environment()->FastForwardBy(base::Minutes(51));

  MakePrimaryAccountWithAccessPoint(
      "test@gmail.com", signin_metrics::AccessPoint::kSendTabToSelfPromo);

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
}

TEST_F(SigninPromoTabHelperTest, CallbackDoesNotFireOnDifferentAccessPoint) {
  base::test::TestFuture<void> future;

  auto* helper = SigninPromoTabHelper::GetForWebContents(*web_contents());
  helper->InitializeCallbackAfterSignIn(
      future.GetCallback(), signin_metrics::AccessPoint::kSendTabToSelfPromo);

  MakePrimaryAccountWithAccessPoint("test@gmail.com",
                                    signin_metrics::AccessPoint::kSettings);

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(helper->IsInitializedForTesting());

  // Subsequent correct sign-in should still not fire it because it was reset.
  signin::ClearPrimaryAccount(identity_manager());
  MakePrimaryAccountWithAccessPoint(
      "another_test@gmail.com",
      signin_metrics::AccessPoint::kSendTabToSelfPromo);
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
}

TEST_F(SigninPromoTabHelperTest, CallbackFiresOnSuccessfulReauth) {
  AccountInfo info = SetupReauthPendingState(
      "test@email.com", signin_metrics::AccessPoint::kSettings);

  base::test::TestFuture<void> future;

  SigninPromoTabHelper::GetForWebContents(*web_contents())
      ->InitializeCallbackAfterSignIn(future.GetCallback(),
                                      signin_metrics::AccessPoint::kSettings);

  EXPECT_FALSE(future.IsReady());

  // Successfully reauthenticate (clearing the token error status).
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), info.account_id,
      GoogleServiceAuthError::AuthErrorNone());

  EXPECT_TRUE(future.Wait());
}

TEST_F(SigninPromoTabHelperTest, CallbackResetsOnIdentityManagerShutdown) {
  base::test::TestFuture<void> future;
  auto* helper = SigninPromoTabHelper::GetForWebContents(*web_contents());
  helper->InitializeCallbackAfterSignIn(
      future.GetCallback(), signin_metrics::AccessPoint::kSendTabToSelfPromo);

  ASSERT_TRUE(helper->IsInitializedForTesting());

  // Simulate IdentityManager shutdown event.
  helper->OnIdentityManagerShutdown(identity_manager());

  EXPECT_FALSE(helper->IsInitializedForTesting());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
}

TEST_F(SigninPromoTabHelperTest,
       CallbackDoesNotFireOnReauthDifferentAccessPoint) {
  AccountInfo info = SetupReauthPendingState(
      "test@email.com", signin_metrics::AccessPoint::kSettings);

  base::test::TestFuture<void> future;
  auto* helper = SigninPromoTabHelper::GetForWebContents(*web_contents());
  helper->InitializeCallbackAfterSignIn(future.GetCallback(),
                                        signin_metrics::AccessPoint::kSettings);

  // Simulate reauth but completed with a DIFFERENT access point (e.g.
  // PasswordBubble).
  AccountInfo extended_info = identity_manager()->FindExtendedAccountInfo(info);
  extended_info.access_point = signin_metrics::AccessPoint::kPasswordBubble;
  signin::UpdateAccountInfoForAccount(identity_manager(), extended_info);

  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), info.account_id,
      GoogleServiceAuthError::AuthErrorNone());

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(future.IsReady());
  EXPECT_FALSE(helper->IsInitializedForTesting());
}

TEST_F(SigninPromoTabHelperTest, CallbackResetsOnPrimaryAccountSignout) {
  SetupReauthPendingState("account_a@email.com",
                          signin_metrics::AccessPoint::kSettings);

  base::test::TestFuture<void> future;
  auto* helper = SigninPromoTabHelper::GetForWebContents(*web_contents());
  helper->InitializeCallbackAfterSignIn(future.GetCallback(),
                                        signin_metrics::AccessPoint::kSettings);

  // Change the primary account to Account B.
  signin::ClearPrimaryAccount(identity_manager());
  signin::MakePrimaryAccountAvailable(identity_manager(), "account_b@gmail.com",
                                      signin::ConsentLevel::kSignin);

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(helper->IsInitializedForTesting());
  EXPECT_FALSE(future.IsReady());
}
}  // namespace
