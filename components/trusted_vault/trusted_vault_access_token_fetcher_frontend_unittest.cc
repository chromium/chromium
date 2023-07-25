// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"

#include <string>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using testing::Eq;
using testing::Ne;

MATCHER_P(HasExpectedToken, token, "") {
  const TrustedVaultAccessTokenFetcher::AccessTokenInfoOrError&
      token_info_or_error = arg;
  return token_info_or_error.has_value() && token_info_or_error->token == token;
}

MATCHER_P(HasUnexpectedError, error, "") {
  const TrustedVaultAccessTokenFetcher::AccessTokenInfoOrError&
      token_info_or_error = arg;
  return !token_info_or_error.has_value() &&
         token_info_or_error.error() == error;
}

class TrustedVaultAccessTokenFetcherFrontendTest : public testing::Test {
 public:
  TrustedVaultAccessTokenFetcherFrontendTest()
      : frontend_(identity_env_.identity_manager()) {}
  ~TrustedVaultAccessTokenFetcherFrontendTest() override = default;

  TrustedVaultAccessTokenFetcherFrontend* frontend() { return &frontend_; }

  signin::IdentityTestEnvironment* identity_env() { return &identity_env_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  // |identity_env_| must outlive |frontend_|.
  signin::IdentityTestEnvironment identity_env_;
  TrustedVaultAccessTokenFetcherFrontend frontend_;
};

TEST_F(TrustedVaultAccessTokenFetcherFrontendTest,
       ShouldFetchAccessTokenForPrimaryAccount) {
  const CoreAccountId kAccountId =
      identity_env()
          ->MakePrimaryAccountAvailable("test@gmail.com",
                                        signin::ConsentLevel::kSignin)
          .account_id;
  const std::string kAccessToken = "access_token";

  base::MockCallback<TrustedVaultAccessTokenFetcher::TokenCallback>
      token_callback;
  frontend()->FetchAccessToken(kAccountId, token_callback.Get());

  // Callback should be called upon fetching access token from the server.
  EXPECT_CALL(token_callback, Run(HasExpectedToken(kAccessToken)));
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccountId, kAccessToken, base::Time::Now() + base::Hours(1));
}

TEST_F(TrustedVaultAccessTokenFetcherFrontendTest,
       ShouldFetchAccessTokenForUnconsentedPrimaryAccount) {
  const CoreAccountId kAccountId =
      identity_env()
          ->MakePrimaryAccountAvailable("test@gmail.com",
                                        signin::ConsentLevel::kSignin)
          .account_id;
  const std::string kAccessToken = "access_token";

  base::MockCallback<TrustedVaultAccessTokenFetcher::TokenCallback>
      token_callback;
  frontend()->FetchAccessToken(kAccountId, token_callback.Get());

  // Callback should be called upon fetching access token from the server.
  EXPECT_CALL(token_callback, Run(HasExpectedToken(kAccessToken)));
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccountId, kAccessToken, base::Time::Now() + base::Hours(1));
}

TEST_F(TrustedVaultAccessTokenFetcherFrontendTest,
       ShouldRejectFetchAttemptForNonPrimaryAccount) {
  identity_env()->MakePrimaryAccountAvailable("test1@gmail.com",
                                              signin::ConsentLevel::kSignin);
  const CoreAccountId kSecondaryAccountId =
      identity_env()->MakeAccountAvailable("test2@gmail.com").account_id;

  // Fetch should be rejected immediately.
  base::MockCallback<TrustedVaultAccessTokenFetcher::TokenCallback>
      token_callback;
  EXPECT_CALL(
      token_callback,
      Run(HasUnexpectedError(
          TrustedVaultAccessTokenFetcher::FetchingError::kNotPrimaryAccount)));
  frontend()->FetchAccessToken(kSecondaryAccountId, token_callback.Get());
}

TEST_F(TrustedVaultAccessTokenFetcherFrontendTest,
       ShouldReplyOnUnsuccessfulFetchAttemptWithTransientAuthError) {
  const CoreAccountId kAccountId =
      identity_env()
          ->MakePrimaryAccountAvailable("test@gmail.com",
                                        signin::ConsentLevel::kSignin)
          .account_id;
  const std::string kAccessToken = "access_token";

  base::MockCallback<TrustedVaultAccessTokenFetcher::TokenCallback>
      token_callback;
  frontend()->FetchAccessToken(kAccountId, token_callback.Get());

  // Callback should be called upon unsuccessful token fetching attempt.
  EXPECT_CALL(
      token_callback,
      Run(HasUnexpectedError(
          TrustedVaultAccessTokenFetcher::FetchingError::kTransientAuthError)));
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));
}

TEST_F(TrustedVaultAccessTokenFetcherFrontendTest,
       ShouldReplyOnUnsuccessfulFetchAttemptWithPersistentAuthError) {
  const CoreAccountId kAccountId =
      identity_env()
          ->MakePrimaryAccountAvailable("test@gmail.com",
                                        signin::ConsentLevel::kSignin)
          .account_id;
  const std::string kAccessToken = "access_token";

  base::MockCallback<TrustedVaultAccessTokenFetcher::TokenCallback>
      token_callback;
  frontend()->FetchAccessToken(kAccountId, token_callback.Get());

  // Callback should be called upon unsuccessful token fetching attempt.
  EXPECT_CALL(token_callback,
              Run(HasUnexpectedError(TrustedVaultAccessTokenFetcher::
                                         FetchingError::kPersistentAuthError)));
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromUnexpectedServiceResponse("error"));
}

TEST_F(TrustedVaultAccessTokenFetcherFrontendTest, ShouldAllowMultipleFetches) {
  const CoreAccountId kAccountId =
      identity_env()
          ->MakePrimaryAccountAvailable("test@gmail.com",
                                        signin::ConsentLevel::kSignin)
          .account_id;
  const std::string kAccessToken = "access_token";

  base::MockCallback<TrustedVaultAccessTokenFetcher::TokenCallback>
      token_callback1;
  frontend()->FetchAccessToken(kAccountId, token_callback1.Get());

  // Start second fetch before the first one completes.
  base::MockCallback<TrustedVaultAccessTokenFetcher::TokenCallback>
      token_callback2;
  frontend()->FetchAccessToken(kAccountId, token_callback2.Get());

  // Both token callbacks should be called upon fetching access token from the
  // server.
  EXPECT_CALL(token_callback1, Run(HasExpectedToken(kAccessToken)));
  EXPECT_CALL(token_callback2, Run(HasExpectedToken(kAccessToken)));
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccountId, kAccessToken, base::Time::Now() + base::Hours(1));
}

}  // namespace

}  // namespace trusted_vault
