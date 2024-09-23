// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/oauth_token_fetcher.h"

#include <optional>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/babelorca/token_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/token_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::babelorca {
namespace {

constexpr char kOAuthToken[] = "test-token";
constexpr base::Time kExpirationTime =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(50000));
constexpr char kIdToken[] = "id-test-token";
constexpr base::TimeDelta kRetryInitialBackoff = base::Milliseconds(500);

class OAuthTokenFetcherTest : public testing::Test {
 protected:
  // testing::Test:
  void SetUp() override {
    account_info_ = identity_test_env_.MakeAccountAvailable("test@school.edu");
    identity_test_env_.SetPrimaryAccount(account_info_.email,
                                         signin::ConsentLevel::kSignin);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  AccountInfo account_info_;
};

TEST_F(OAuthTokenFetcherTest, SuccessfulOAuthTokenFetch) {
  base::test::TestFuture<std::optional<TokenDataWrapper>> fetch_future;
  OAuthTokenFetcher oauth_token_fetcher(identity_test_env_.identity_manager());

  oauth_token_fetcher.FetchToken(fetch_future.GetCallback());
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, kExpirationTime, kIdToken,
          {GaiaConstants::kTachyonOAuthScope});

  auto token_data = fetch_future.Get();
  ASSERT_TRUE(token_data.has_value());
  EXPECT_THAT(token_data->token, testing::StrEq(kOAuthToken));
  EXPECT_EQ(token_data->expiration_time, kExpirationTime);
}

TEST_F(OAuthTokenFetcherTest, FailedOAuthTokenFetch) {
  base::test::TestFuture<std::optional<TokenDataWrapper>> fetch_future;
  OAuthTokenFetcher oauth_token_fetcher(identity_test_env_.identity_manager());

  oauth_token_fetcher.FetchToken(fetch_future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      account_info_.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));

  auto token_data = fetch_future.Get();
  EXPECT_FALSE(token_data.has_value());
}

TEST_F(OAuthTokenFetcherTest, OverlappingTokenFetchShouldFail) {
  base::test::TestFuture<std::optional<TokenDataWrapper>> first_fetch_future;
  base::test::TestFuture<std::optional<TokenDataWrapper>> overlap_fetch_future;
  OAuthTokenFetcher oauth_token_fetcher(identity_test_env_.identity_manager());

  oauth_token_fetcher.FetchToken(first_fetch_future.GetCallback());
  oauth_token_fetcher.FetchToken(overlap_fetch_future.GetCallback());
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, kExpirationTime, kIdToken,
          {GaiaConstants::kTachyonOAuthScope});

  EXPECT_TRUE(first_fetch_future.Get().has_value());
  EXPECT_FALSE(overlap_fetch_future.Get().has_value());
}

TEST_F(OAuthTokenFetcherTest, SuccessiveTokenFetchShouldProceed) {
  base::test::TestFuture<std::optional<TokenDataWrapper>> first_fetch_future;
  base::test::TestFuture<std::optional<TokenDataWrapper>> second_fetch_future;
  OAuthTokenFetcher oauth_token_fetcher(identity_test_env_.identity_manager());

  oauth_token_fetcher.FetchToken(first_fetch_future.GetCallback());
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, kExpirationTime, kIdToken,
          {GaiaConstants::kTachyonOAuthScope});
  oauth_token_fetcher.FetchToken(second_fetch_future.GetCallback());
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, kExpirationTime, kIdToken,
          {GaiaConstants::kTachyonOAuthScope});

  EXPECT_TRUE(first_fetch_future.Get().has_value());
  EXPECT_TRUE(second_fetch_future.Get().has_value());
}

TEST_F(OAuthTokenFetcherTest, RetryOnRetriableError) {
  base::test::TestFuture<std::optional<TokenDataWrapper>> fetch_future;
  OAuthTokenFetcher oauth_token_fetcher(identity_test_env_.identity_manager());

  oauth_token_fetcher.FetchToken(fetch_future.GetCallback());
  // Failure.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      account_info_.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  // First retry success.
  task_environment_.FastForwardBy(kRetryInitialBackoff);
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, kExpirationTime, kIdToken,
          {GaiaConstants::kTachyonOAuthScope});

  EXPECT_TRUE(fetch_future.Get().has_value());
}

TEST_F(OAuthTokenFetcherTest, RespondOnLastSuccessfulRetry) {
  base::test::TestFuture<std::optional<TokenDataWrapper>> fetch_future;
  OAuthTokenFetcher oauth_token_fetcher(identity_test_env_.identity_manager());

  oauth_token_fetcher.FetchToken(fetch_future.GetCallback());
  // Failure.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      account_info_.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  task_environment_.FastForwardBy(kRetryInitialBackoff);
  // First retry failure.
  task_environment_.FastForwardBy(kRetryInitialBackoff);
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      account_info_.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  // Second retry success.
  task_environment_.FastForwardBy(kRetryInitialBackoff * 2);
  identity_test_env_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
          kOAuthToken, kExpirationTime, kIdToken,
          {GaiaConstants::kTachyonOAuthScope});

  EXPECT_TRUE(fetch_future.Get().has_value());
}

TEST_F(OAuthTokenFetcherTest, RespondAfterMaxRetries) {
  base::test::TestFuture<std::optional<TokenDataWrapper>> fetch_future;
  OAuthTokenFetcher oauth_token_fetcher(identity_test_env_.identity_manager());

  oauth_token_fetcher.FetchToken(fetch_future.GetCallback());
  // Failure.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      account_info_.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  // First retry failure.
  task_environment_.FastForwardBy(kRetryInitialBackoff);
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      account_info_.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  // Second retry failure.
  task_environment_.FastForwardBy(kRetryInitialBackoff * 2);
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      account_info_.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));

  EXPECT_FALSE(fetch_future.Get().has_value());
}

}  // namespace
}  // namespace ash::babelorca
