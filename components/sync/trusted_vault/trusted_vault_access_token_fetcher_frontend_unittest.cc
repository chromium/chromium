// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"

#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::Eq;
using testing::Ne;

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
                                        signin::ConsentLevel::kSync)
          .account_id;
  const std::string kAccessToken = "access_token";

  absl::optional<signin::AccessTokenInfo> fetched_access_token;
  frontend()->FetchAccessToken(
      kAccountId,
      base::BindLambdaForTesting(
          [&](absl::optional<signin::AccessTokenInfo> access_token_info) {
            fetched_access_token = access_token_info;
          }));
  // Access token shouldn't be fetched immediately.
  EXPECT_THAT(fetched_access_token, Eq(absl::nullopt));

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccountId, kAccessToken, base::Time::Now() + base::Hours(1));

  // Now access token should be fetched.
  ASSERT_THAT(fetched_access_token, Ne(absl::nullopt));
  EXPECT_THAT(fetched_access_token->token, Eq(kAccessToken));
}

TEST_F(TrustedVaultAccessTokenFetcherFrontendTest,
       ShouldFetchAccessTokenForUnconsentedPrimaryAccount) {
  const CoreAccountId kAccountId =
      identity_env()
          ->MakePrimaryAccountAvailable("test@gmail.com",
                                        signin::ConsentLevel::kSignin)
          .account_id;
  const std::string kAccessToken = "access_token";

  absl::optional<signin::AccessTokenInfo> fetched_access_token;
  frontend()->FetchAccessToken(
      kAccountId,
      base::BindLambdaForTesting(
          [&](absl::optional<signin::AccessTokenInfo> access_token_info) {
            fetched_access_token = access_token_info;
          }));
  // Access token shouldn't be fetched immediately.
  EXPECT_THAT(fetched_access_token, Eq(absl::nullopt));

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccountId, kAccessToken, base::Time::Now() + base::Hours(1));

  // Now access token should be fetched.
  ASSERT_THAT(fetched_access_token, Ne(absl::nullopt));
  EXPECT_THAT(fetched_access_token->token, Eq(kAccessToken));
}

TEST_F(TrustedVaultAccessTokenFetcherFrontendTest,
       ShouldRejectFetchAttemptForNonPrimaryAccount) {
  identity_env()->MakePrimaryAccountAvailable("test1@gmail.com",
                                              signin::ConsentLevel::kSync);
  const CoreAccountId kSecondaryAccountId =
      identity_env()->MakeAccountAvailable("test2@gmail.com").account_id;

  absl::optional<signin::AccessTokenInfo> fetched_access_token;
  bool callback_called = false;
  frontend()->FetchAccessToken(
      kSecondaryAccountId,
      base::BindLambdaForTesting(
          [&](absl::optional<signin::AccessTokenInfo> access_token_info) {
            fetched_access_token = access_token_info;
            callback_called = true;
          }));

  // Fetch should be rejected immediately.
  EXPECT_TRUE(callback_called);
  EXPECT_THAT(fetched_access_token, Eq(absl::nullopt));
}

TEST_F(TrustedVaultAccessTokenFetcherFrontendTest,
       ShouldReplyOnUnsuccessfulFetchAttempt) {
  const CoreAccountId kAccountId =
      identity_env()
          ->MakePrimaryAccountAvailable("test@gmail.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  const std::string kAccessToken = "access_token";

  absl::optional<signin::AccessTokenInfo> fetched_access_token;
  bool callback_called = false;
  frontend()->FetchAccessToken(
      kAccountId,
      base::BindLambdaForTesting(
          [&](absl::optional<signin::AccessTokenInfo> access_token_info) {
            fetched_access_token = access_token_info;
            callback_called = true;
          }));
  // Access token shouldn't be fetched immediately.
  EXPECT_FALSE(callback_called);

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromUnexpectedServiceResponse("error"));

  EXPECT_TRUE(callback_called);
  EXPECT_THAT(fetched_access_token, Eq(absl::nullopt));
}

TEST_F(TrustedVaultAccessTokenFetcherFrontendTest, ShouldAllowMultipleFetches) {
  const CoreAccountId kAccountId =
      identity_env()
          ->MakePrimaryAccountAvailable("test@gmail.com",
                                        signin::ConsentLevel::kSync)
          .account_id;
  const std::string kAccessToken = "access_token";

  absl::optional<signin::AccessTokenInfo> fetched_access_token1;
  frontend()->FetchAccessToken(
      kAccountId,
      base::BindLambdaForTesting(
          [&](absl::optional<signin::AccessTokenInfo> access_token_info) {
            fetched_access_token1 = access_token_info;
          }));
  // Start second fetch before the first one completes.
  absl::optional<signin::AccessTokenInfo> fetched_access_token2;
  frontend()->FetchAccessToken(
      kAccountId,
      base::BindLambdaForTesting(
          [&](absl::optional<signin::AccessTokenInfo> access_token_info) {
            fetched_access_token2 = access_token_info;
          }));
  // Access token shouldn't be fetched immediately.
  EXPECT_THAT(fetched_access_token1, Eq(absl::nullopt));
  EXPECT_THAT(fetched_access_token2, Eq(absl::nullopt));

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kAccountId, kAccessToken, base::Time::Now() + base::Hours(1));

  // Both fetch callbacks should be called.
  ASSERT_THAT(fetched_access_token1, Ne(absl::nullopt));
  EXPECT_THAT(fetched_access_token1->token, Eq(kAccessToken));

  ASSERT_THAT(fetched_access_token2, Ne(absl::nullopt));
  EXPECT_THAT(fetched_access_token2->token, Eq(kAccessToken));
}

}  // namespace

}  // namespace syncer
