// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_token_fetcher.h"

#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_response.h"
#include "components/signin/public/base/test_signin_client.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

using testing::UnorderedPointwise;

using AccountParams = OAuthMultiloginTokenFetcher::AccountParams;

namespace {

const char kAccessToken[] = "access_token";

// Status of the token fetch.
enum class FetchStatus { kSuccess, kFailure, kPending };

// Matches `std::pair<CoreAccountId, OAuthMultiloginTokenResponse>` against an
// `std::pair<CoreAccountId, std::string>` for use inside testing::Pointwise().
MATCHER(HasTheSameAccountIdTokenPair, "") {
  const auto& [response_pair, token_pair] = arg;
  return testing::ExplainMatchResult(testing::Eq(token_pair.first),
                                     response_pair.first, result_listener) &&
         testing::ExplainMatchResult(
             testing::AllOf(
                 testing::Property("oauth_token()",
                                   &OAuthMultiloginTokenResponse::oauth_token,
                                   token_pair.second),
                 testing::Property(
                     "token_binding_assertion()",
                     &OAuthMultiloginTokenResponse::token_binding_assertion,
                     testing::IsEmpty())),
             response_pair.second, result_listener);
}

}  // namespace

class OAuthMultiloginTokenFetcherTest : public testing::Test {
 public:
  const CoreAccountId kAccountId{
      CoreAccountId::FromGaiaId(GaiaId("account_id"))};

  OAuthMultiloginTokenFetcherTest()
      : test_signin_client_(&pref_service_), token_service_(&pref_service_) {}

  ~OAuthMultiloginTokenFetcherTest() override = default;

  std::unique_ptr<OAuthMultiloginTokenFetcher> CreateFetcher(
      const std::vector<AccountParams>& account_params,
      const std::string& ephemeral_public_key = std::string()) {
    return std::make_unique<OAuthMultiloginTokenFetcher>(
        &test_signin_client_, &token_service_, account_params,
        ephemeral_public_key,
        base::BindOnce(&OAuthMultiloginTokenFetcherTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&OAuthMultiloginTokenFetcherTest::OnFailure,
                       base::Unretained(this)));
  }

  // Returns the status of the token fetch.
  FetchStatus GetFetchStatus() const {
    if (success_callback_called_) {
      return FetchStatus::kSuccess;
    }
    return failure_callback_called_ ? FetchStatus::kFailure
                                    : FetchStatus::kPending;
  }

  FakeProfileOAuth2TokenService& token_service() { return token_service_; }

  const base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse>& tokens()
      const {
    return tokens_;
  }

  const GoogleServiceAuthError& error() const { return error_; }

 private:
  // Success callback for OAuthMultiloginTokenFetcher.
  void OnSuccess(
      base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse> tokens) {
    DCHECK(!success_callback_called_);
    DCHECK(tokens_.empty());
    success_callback_called_ = true;
    tokens_ = std::move(tokens);
  }

  // Failure callback for OAuthMultiloginTokenFetcher.
  void OnFailure(const GoogleServiceAuthError& error) {
    DCHECK(!failure_callback_called_);
    failure_callback_called_ = true;
    error_ = error;
  }

  base::test::TaskEnvironment task_environment_;

  bool success_callback_called_ = false;
  bool failure_callback_called_ = false;
  GoogleServiceAuthError error_;
  base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse> tokens_;

  TestingPrefServiceSimple pref_service_;
  TestSigninClient test_signin_client_;
  FakeProfileOAuth2TokenService token_service_;
};

TEST_F(OAuthMultiloginTokenFetcherTest, OneAccountSuccess) {
  token_service().UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({{.account_id = kAccountId}});
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service().IssueAllTokensForAccount(kAccountId, success_response);
  EXPECT_EQ(FetchStatus::kSuccess, GetFetchStatus());
  // Check result.
  EXPECT_THAT(tokens(),
              UnorderedPointwise(HasTheSameAccountIdTokenPair(),
                                 {std::make_pair(kAccountId, kAccessToken)}));
}

TEST_F(OAuthMultiloginTokenFetcherTest, OneAccountPersistentError) {
  token_service().UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({{.account_id = kAccountId}});
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  token_service().IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_EQ(FetchStatus::kFailure, GetFetchStatus());
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, error().state());
}

TEST_F(OAuthMultiloginTokenFetcherTest, OneAccountTransientError) {
  token_service().UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({{.account_id = kAccountId}});
  // Connection failure will be retried.
  token_service().IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  // Success on retry.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service().IssueAllTokensForAccount(kAccountId, success_response);
  EXPECT_EQ(FetchStatus::kSuccess, GetFetchStatus());
  // Check result.
  EXPECT_THAT(tokens(),
              UnorderedPointwise(HasTheSameAccountIdTokenPair(),
                                 {std::make_pair(kAccountId, kAccessToken)}));
}

TEST_F(OAuthMultiloginTokenFetcherTest, OneAccountTransientErrorMaxRetries) {
  token_service().UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({{.account_id = kAccountId}});
  // Repeated connection failures.
  token_service().IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  token_service().IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  // Stop retrying, and fail.
  EXPECT_EQ(FetchStatus::kFailure, GetFetchStatus());
  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error().state());
}

// The flow succeeds even if requests are received out of order.
TEST_F(OAuthMultiloginTokenFetcherTest, MultipleAccountsSuccess) {
  const CoreAccountId account_1 =
      CoreAccountId::FromGaiaId(GaiaId("account_1"));
  const CoreAccountId account_2 =
      CoreAccountId::FromGaiaId(GaiaId("account_2"));
  const CoreAccountId account_3 =
      CoreAccountId::FromGaiaId(GaiaId("account_3"));
  token_service().UpdateCredentials(account_1, "refresh_token");
  token_service().UpdateCredentials(account_2, "refresh_token");
  token_service().UpdateCredentials(account_3, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({{.account_id = account_1},
                     {.account_id = account_2},
                     {.account_id = account_3}});
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = "token_3";
  token_service().IssueAllTokensForAccount(account_3, success_response);
  success_response.access_token = "token_1";
  token_service().IssueAllTokensForAccount(account_1, success_response);
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  success_response.access_token = "token_2";
  token_service().IssueAllTokensForAccount(account_2, success_response);
  EXPECT_EQ(FetchStatus::kSuccess, GetFetchStatus());
  // Check result.
  EXPECT_THAT(tokens(),
              UnorderedPointwise(HasTheSameAccountIdTokenPair(),
                                 {std::make_pair(account_1, "token_1"),
                                  std::make_pair(account_2, "token_2"),
                                  std::make_pair(account_3, "token_3")}));
}

TEST_F(OAuthMultiloginTokenFetcherTest, MultipleAccountsTransientError) {
  const CoreAccountId account_1 =
      CoreAccountId::FromGaiaId(GaiaId("account_1"));
  const CoreAccountId account_2 =
      CoreAccountId::FromGaiaId(GaiaId("account_2"));
  const CoreAccountId account_3 =
      CoreAccountId::FromGaiaId(GaiaId("account_3"));
  token_service().UpdateCredentials(account_1, "refresh_token");
  token_service().UpdateCredentials(account_2, "refresh_token");
  token_service().UpdateCredentials(account_3, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({{.account_id = account_1},
                     {.account_id = account_2},
                     {.account_id = account_3}});
  // Connection failures will be retried.
  token_service().IssueErrorForAllPendingRequestsForAccount(
      account_1,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  token_service().IssueErrorForAllPendingRequestsForAccount(
      account_2,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  token_service().IssueErrorForAllPendingRequestsForAccount(
      account_3,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  // Success on retry.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  success_response.access_token = "token_1";
  token_service().IssueAllTokensForAccount(account_1, success_response);
  success_response.access_token = "token_2";
  token_service().IssueAllTokensForAccount(account_2, success_response);
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  success_response.access_token = "token_3";
  token_service().IssueAllTokensForAccount(account_3, success_response);
  EXPECT_EQ(FetchStatus::kSuccess, GetFetchStatus());
  // Check result.
  EXPECT_THAT(tokens(),
              UnorderedPointwise(HasTheSameAccountIdTokenPair(),
                                 {std::make_pair(account_1, "token_1"),
                                  std::make_pair(account_2, "token_2"),
                                  std::make_pair(account_3, "token_3")}));
}

TEST_F(OAuthMultiloginTokenFetcherTest, MultipleAccountsPersistentError) {
  const CoreAccountId account_1 =
      CoreAccountId::FromGaiaId(GaiaId("account_1"));
  const CoreAccountId account_2 =
      CoreAccountId::FromGaiaId(GaiaId("account_2"));
  const CoreAccountId account_3 =
      CoreAccountId::FromGaiaId(GaiaId("account_3"));
  token_service().UpdateCredentials(account_1, "refresh_token");
  token_service().UpdateCredentials(account_2, "refresh_token");
  token_service().UpdateCredentials(account_3, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({{.account_id = account_1},
                     {.account_id = account_2},
                     {.account_id = account_3}});
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  token_service().IssueErrorForAllPendingRequestsForAccount(
      account_2,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  // Fail as soon as one of the accounts is in error.
  EXPECT_EQ(FetchStatus::kFailure, GetFetchStatus());
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, error().state());
}

TEST_F(OAuthMultiloginTokenFetcherTest,
       OneAccountWithTokenBindingChallengeSuccess) {
  // `OAuthMultiloginHelperTest` provides a better coverage for the challenge
  // code path as it tests multilogin with refresh tokens. In this test, we just
  // check that a challenge parameter doesn't cause a crash.
  token_service().UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher = CreateFetcher(
      {{.account_id = kAccountId, .token_binding_challenge = "test_challenge"}},
      "ephemeral_pubkey");
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  OAuth2AccessTokenConsumer::TokenResponse success_response =
      OAuth2AccessTokenConsumer::TokenResponse::Builder()
          .WithAccessToken(kAccessToken)
          .build();
  token_service().IssueAllTokensForAccount(kAccountId, success_response);
  EXPECT_EQ(FetchStatus::kSuccess, GetFetchStatus());
  // Check result.
  EXPECT_THAT(tokens(),
              UnorderedPointwise(HasTheSameAccountIdTokenPair(),
                                 {std::make_pair(kAccountId, kAccessToken)}));
}

}  // namespace signin
