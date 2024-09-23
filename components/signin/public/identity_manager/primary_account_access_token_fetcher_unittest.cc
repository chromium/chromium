// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MockCallback;
using testing::StrictMock;

namespace signin {

namespace {

void OnAccessTokenFetchComplete(
    base::OnceClosure done_closure,
    const GoogleServiceAuthError& expected_error,
    const AccessTokenInfo& expected_access_token_info,
    GoogleServiceAuthError error,
    AccessTokenInfo access_token_info) {
  EXPECT_EQ(expected_error, error);
  if (expected_error == GoogleServiceAuthError::AuthErrorNone())
    EXPECT_EQ(expected_access_token_info, access_token_info);

  std::move(done_closure).Run();
}

}  // namespace

class PrimaryAccountAccessTokenFetcherTest
    : public testing::TestWithParam<ConsentLevel> {
 public:
  using TestTokenCallback =
      StrictMock<MockCallback<AccessTokenFetcher::TokenCallback>>;

  PrimaryAccountAccessTokenFetcherTest()
      : identity_test_env_(std::make_unique<IdentityTestEnvironment>()),
        access_token_info_("access token",
                           base::Time::Now() + base::Hours(1),
                           "id_token") {}

  ~PrimaryAccountAccessTokenFetcherTest() override = default;

  std::unique_ptr<PrimaryAccountAccessTokenFetcher> CreateFetcher(
      AccessTokenFetcher::TokenCallback callback,
      PrimaryAccountAccessTokenFetcher::Mode mode,
      ConsentLevel consent) {
    // API scope that does not require consent.
    std::set<std::string> scopes = {
        GaiaConstants::kChromeSafeBrowsingOAuth2Scope};
    return std::make_unique<PrimaryAccountAccessTokenFetcher>(
        "test_consumer", identity_test_env_->identity_manager(), scopes,
        std::move(callback), mode, consent);
  }

  // Creates a fetcher with sync consent based on the test param.
  std::unique_ptr<PrimaryAccountAccessTokenFetcher> CreateFetcher(
      AccessTokenFetcher::TokenCallback callback,
      PrimaryAccountAccessTokenFetcher::Mode mode) {
    ConsentLevel consent = GetParam();
    return CreateFetcher(std::move(callback), mode, consent);
  }

  std::unique_ptr<PrimaryAccountAccessTokenFetcher> CreateDelayedStartFetcher(
      PrimaryAccountAccessTokenFetcher::Mode mode) {
    // API scope that does not require consent.
    std::set<std::string> scopes = {
        GaiaConstants::kChromeSafeBrowsingOAuth2Scope};
    ConsentLevel consent = GetParam();
    return std::make_unique<PrimaryAccountAccessTokenFetcher>(
        "test_consumer", identity_test_env_->identity_manager(), scopes, mode,
        consent);
  }

  IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_.get();
  }

  void ShutdownIdentityManager() { identity_test_env_.reset(); }

  // Signs the user in to the primary account, returning the account ID.
  CoreAccountId SignIn() {
    return identity_test_env_
        ->MakePrimaryAccountAvailable("me@gmail.com",
                                      signin::ConsentLevel::kSync)
        .account_id;
  }

  // Returns an AccessTokenInfo with valid information that can be used for
  // completing access token requests.
  const AccessTokenInfo& access_token_info() const {
    return access_token_info_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<IdentityTestEnvironment> identity_test_env_;
  AccessTokenInfo access_token_info_;
};

TEST_P(PrimaryAccountAccessTokenFetcherTest, OneShotShouldReturnAccessToken) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(), PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       DelayedOneShotShouldReturnAccessToken) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateDelayedStartFetcher(
      PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  fetcher->Start(callback.Get());
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       WaitAndRetryShouldReturnAccessToken) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       DelayedWaitAndRetryShouldReturnAccessToken) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateDelayedStartFetcher(
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  fetcher->Start(callback.Get());
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_P(PrimaryAccountAccessTokenFetcherTest, ShouldNotReplyIfDestroyed) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(), PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  EXPECT_CALL(callback, Run).Times(0);

  // Destroy the fetcher before the access token request is fulfilled.
  fetcher.reset();

  // Fulfilling the request now should have no effect.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_P(PrimaryAccountAccessTokenFetcherTest, ShouldNotReplyIfNotStarted) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this would result in a
  // request for an access token if the fetcher were started.
  auto fetcher = CreateDelayedStartFetcher(
      PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  // No token request generated because the fetcher has not been started.
  EXPECT_FALSE(identity_test_env()->IsAccessTokenRequestPending());
}

TEST_P(PrimaryAccountAccessTokenFetcherTest, OneShotCallsBackWhenSignedOut) {
  base::RunLoop run_loop;

  // Signed out -> we should get called back.
  auto fetcher = CreateFetcher(
      base::BindOnce(&OnAccessTokenFetchComplete, run_loop.QuitClosure(),
                     GoogleServiceAuthError(
                         GoogleServiceAuthError::State::USER_NOT_SIGNED_UP),
                     AccessTokenInfo()),
      PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();
}

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       OneShotCallsBackWhenNoRefreshToken) {
  base::RunLoop run_loop;

  identity_test_env()->SetPrimaryAccount("me@gmail.com",
                                         signin::ConsentLevel::kSync);

  // Signed in, but there is no refresh token -> we should get called back.
  auto fetcher = CreateFetcher(
      base::BindOnce(&OnAccessTokenFetchComplete, run_loop.QuitClosure(),
                     GoogleServiceAuthError(
                         GoogleServiceAuthError::State::USER_NOT_SIGNED_UP),
                     AccessTokenInfo()),
      PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();
}

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       WaitAndRetryNoCallbackWhenSignedOut) {
  TestTokenCallback callback;

  // Signed out -> the fetcher should wait for a sign-in which never happens
  // in this test, so we shouldn't get called back.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  EXPECT_CALL(callback, Run).Times(0);
}

TEST_P(PrimaryAccountAccessTokenFetcherTest, ShouldWaitForSignIn) {
  TestTokenCallback callback;

  // Not signed in, so this should wait for a sign-in to complete.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  CoreAccountId account_id = SignIn();

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // The request should not have to have been retried.
  EXPECT_FALSE(fetcher->access_token_request_retried());
}

TEST_P(PrimaryAccountAccessTokenFetcherTest, ShouldWaitForRefreshToken) {
  TestTokenCallback callback;

  CoreAccountId account_id =
      identity_test_env()
          ->SetPrimaryAccount("me@gmail.com", signin::ConsentLevel::kSync)
          .account_id;

  // Signed in, but there is no refresh token -> we should not get called back
  // (yet).
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // Getting a refresh token should result in a request for an access token.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);

  // The request should not have to have been retried.
  EXPECT_FALSE(fetcher->access_token_request_retried());
}

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       ShouldIgnoreRefreshTokensForOtherAccounts) {
  TestTokenCallback callback;

  // Signed-in to account_id, but there's only a refresh token for a different
  // account.
  CoreAccountId account_id =
      identity_test_env()
          ->SetPrimaryAccount("me@gmail.com", signin::ConsentLevel::kSync)
          .account_id;
  identity_test_env()->MakeAccountAvailable(account_id.ToString() + "2");

  // The fetcher should wait for the correct refresh token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // A refresh token for yet another account shouldn't matter either.
  identity_test_env()->MakeAccountAvailable(account_id.ToString() + "3");
}

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       OneShotCanceledAccessTokenRequest) {
  CoreAccountId account_id = SignIn();

  base::RunLoop run_loop;

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      base::BindOnce(
          &OnAccessTokenFetchComplete, run_loop.QuitClosure(),
          GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          AccessTokenInfo()),
      PrimaryAccountAccessTokenFetcher::Mode::kImmediate);

  // A canceled access token request should result in a callback.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       WaitAndRetryCanceledAccessTokenRequest) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // A canceled access token request should get retried once.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       ShouldRetryCanceledAccessTokenRequestOnlyOnce) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // A canceled access token request should get retried once.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // On the second failure, we should get called back with an empty access
  // token.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          AccessTokenInfo()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

// Shutting down the identity manager while a fetch is in progress should not
// crash. Regression test for https://crbug.com/1187288
TEST_P(PrimaryAccountAccessTokenFetcherTest, IdentityManagerShutdown) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          AccessTokenInfo()));
  ShutdownIdentityManager();
}

// Shutting down the identity manager while waiting for the account should not
// crash. Regression test for https://crbug.com/1187288
TEST_P(PrimaryAccountAccessTokenFetcherTest, IdentityManagerShutdownNoAccount) {
  TestTokenCallback callback;

  // The account is not present, the fetcher starts waiting for the account.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          AccessTokenInfo()));
  ShutdownIdentityManager();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       ShouldNotRetryCanceledAccessTokenRequestIfSignedOut) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // Simulate the user signing out while the access token request is pending.
  // In this case, the pending request gets canceled, and the fetcher should
  // *not* retry.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP),
          AccessTokenInfo()));

  identity_test_env()->ClearPrimaryAccount();
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       ShouldNotRetryCanceledAccessTokenRequestIfRefreshTokenRevoked) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // Simulate the refresh token getting removed. In this case, pending
  // access token requests get canceled, and the fetcher should *not* retry.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP),
          AccessTokenInfo()));
  identity_test_env()->RemoveRefreshTokenForPrimaryAccount();
}

TEST_P(PrimaryAccountAccessTokenFetcherTest,
       ShouldNotRetryFailedAccessTokenRequest) {
  TestTokenCallback callback;

  CoreAccountId account_id = SignIn();

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(
      callback.Get(),
      PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable);

  // An access token failure other than "canceled" should not be retried; we
  // should immediately get called back with an empty access token.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE),
          AccessTokenInfo()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
}

// The above tests all use a consented primary account, so they should all work
// whether or not sync consent is required.
INSTANTIATE_TEST_SUITE_P(All,
                         PrimaryAccountAccessTokenFetcherTest,
                         testing::Values(ConsentLevel::kSignin,
                                         ConsentLevel::kSync));

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Chrome OS can directly set the unconsented primary account during login,
// so it has additional tests.
TEST_F(PrimaryAccountAccessTokenFetcherTest,
       UnconsentedPrimaryAccountWithSyncConsentNotRequired) {
  TestTokenCallback callback;

  // Simulate login.
  identity_test_env()->MakePrimaryAccountAvailable(
      "me@gmail.com", signin::ConsentLevel::kSignin);

  // Perform an immediate fetch with consent not required.
  auto fetcher = CreateFetcher(
      callback.Get(), PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      ConsentLevel::kSignin);

  // We should get called back with the token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       UnconsentedPrimaryAccountWithSyncConsentRequired) {
  TestTokenCallback callback;

  // Simulate login.
  identity_test_env()->MakePrimaryAccountAvailable(
      "me@gmail.com", signin::ConsentLevel::kSignin);

  // Try an immediate fetch with consent required.
  auto fetcher = CreateFetcher(
      callback.Get(), PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      ConsentLevel::kSync);

  // No token request generated because the account isn't consented.
  EXPECT_FALSE(identity_test_env()->IsAccessTokenRequestPending());
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldWaitForUnconsentedAccountLogin) {
  TestTokenCallback callback;

  // Not logged in, so the fetcher waits for an account to become available.
  auto fetcher =
      CreateFetcher(callback.Get(),
                    PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
                    ConsentLevel::kSignin);
  EXPECT_FALSE(identity_test_env()->IsAccessTokenRequestPending());

  // Simulate login.
  identity_test_env()->MakePrimaryAccountAvailable(
      "me@gmail.com", signin::ConsentLevel::kSignin);

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback, Run(GoogleServiceAuthError::AuthErrorNone(),
                            access_token_info()));
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      access_token_info().token, access_token_info().expiration_time,
      access_token_info().id_token);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace signin
