// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_token_fetcher.h"

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/public/base/test_signin_client.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

const CoreAccountId kAccountId("account_id");
const char kAccessToken[] = "access_token";

// Status of the token fetch.
enum class FetchStatus { kSuccess, kFailure, kPending };

}  // namespace

class OAuthMultiloginTokenFetcherTest : public testing::Test {
 public:
  OAuthMultiloginTokenFetcherTest()
      : test_signin_client_(&pref_service_), token_service_(&pref_service_) {}

  ~OAuthMultiloginTokenFetcherTest() override = default;

  std::unique_ptr<OAuthMultiloginTokenFetcher> CreateFetcher(
      const std::vector<CoreAccountId> account_ids) {
    return std::make_unique<OAuthMultiloginTokenFetcher>(
        &test_signin_client_, &token_service_, account_ids,
        base::BindOnce(&OAuthMultiloginTokenFetcherTest::OnSuccess,
                       base::Unretained(this)),
        base::BindOnce(&OAuthMultiloginTokenFetcherTest::OnFailure,
                       base::Unretained(this)));
  }

  // Returns the status of the token fetch.
  FetchStatus GetFetchStatus() const {
    if (success_callback_called_)
      return FetchStatus::kSuccess;
    return failure_callback_called_ ? FetchStatus::kFailure
                                    : FetchStatus::kPending;
  }

 protected:
  // Success callback for OAuthMultiloginTokenFetcher.
  void OnSuccess(
      const std::vector<OAuthMultiloginTokenFetcher::AccountIdTokenPair>&
          account_id_token_pairs) {
    DCHECK(!success_callback_called_);
    DCHECK(account_id_token_pairs_.empty());
    success_callback_called_ = true;
    account_id_token_pairs_ = account_id_token_pairs;
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
  std::vector<OAuthMultiloginTokenFetcher::AccountIdTokenPair>
      account_id_token_pairs_;

  TestingPrefServiceSimple pref_service_;
  TestSigninClient test_signin_client_;
  FakeProfileOAuth2TokenService token_service_;
};

TEST_F(OAuthMultiloginTokenFetcherTest, OneAccountSuccess) {
  token_service_.UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({kAccountId});
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service_.IssueAllTokensForAccount(kAccountId, success_response);
  EXPECT_EQ(FetchStatus::kSuccess, GetFetchStatus());
  // Check result.
  EXPECT_EQ(1u, account_id_token_pairs_.size());
  EXPECT_EQ(kAccountId, account_id_token_pairs_[0].account_id);
  EXPECT_EQ(kAccessToken, account_id_token_pairs_[0].token);
}

TEST_F(OAuthMultiloginTokenFetcherTest, OneAccountPersistentError) {
  token_service_.UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({kAccountId});
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  token_service_.IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_EQ(FetchStatus::kFailure, GetFetchStatus());
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, error_.state());
}

TEST_F(OAuthMultiloginTokenFetcherTest, OneAccountTransientError) {
  token_service_.UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({kAccountId});
  // Connection failure will be retried.
  token_service_.IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  // Success on retry.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  token_service_.IssueAllTokensForAccount(kAccountId, success_response);
  EXPECT_EQ(FetchStatus::kSuccess, GetFetchStatus());
  // Check result.
  EXPECT_EQ(1u, account_id_token_pairs_.size());
  EXPECT_EQ(kAccountId, account_id_token_pairs_[0].account_id);
  EXPECT_EQ(kAccessToken, account_id_token_pairs_[0].token);
}

TEST_F(OAuthMultiloginTokenFetcherTest, OneAccountTransientErrorMaxRetries) {
  token_service_.UpdateCredentials(kAccountId, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({kAccountId});
  // Repeated connection failures.
  token_service_.IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  token_service_.IssueErrorForAllPendingRequestsForAccount(
      kAccountId,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  // Stop retrying, and fail.
  EXPECT_EQ(FetchStatus::kFailure, GetFetchStatus());
  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error_.state());
}

// The flow succeeds even if requests are received out of order.
TEST_F(OAuthMultiloginTokenFetcherTest, MultipleAccountsSuccess) {
  const CoreAccountId account_1("account_1");
  const CoreAccountId account_2("account_2");
  const CoreAccountId account_3("account_3");
  token_service_.UpdateCredentials(account_1, "refresh_token");
  token_service_.UpdateCredentials(account_2, "refresh_token");
  token_service_.UpdateCredentials(account_3, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({account_1, account_2, account_3});
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = "token_3";
  token_service_.IssueAllTokensForAccount(account_3, success_response);
  success_response.access_token = "token_1";
  token_service_.IssueAllTokensForAccount(account_1, success_response);
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  success_response.access_token = "token_2";
  token_service_.IssueAllTokensForAccount(account_2, success_response);
  EXPECT_EQ(FetchStatus::kSuccess, GetFetchStatus());
  // Check result.
  EXPECT_EQ(3u, account_id_token_pairs_.size());
  EXPECT_EQ(account_1, account_id_token_pairs_[0].account_id);
  EXPECT_EQ(account_2, account_id_token_pairs_[1].account_id);
  EXPECT_EQ(account_3, account_id_token_pairs_[2].account_id);
  EXPECT_EQ("token_1", account_id_token_pairs_[0].token);
  EXPECT_EQ("token_2", account_id_token_pairs_[1].token);
  EXPECT_EQ("token_3", account_id_token_pairs_[2].token);
}

TEST_F(OAuthMultiloginTokenFetcherTest, MultipleAccountsTransientError) {
  const CoreAccountId account_1("account_1");
  const CoreAccountId account_2("account_2");
  const CoreAccountId account_3("account_3");
  token_service_.UpdateCredentials(account_1, "refresh_token");
  token_service_.UpdateCredentials(account_2, "refresh_token");
  token_service_.UpdateCredentials(account_3, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({account_1, account_2, account_3});
  // Connection failures will be retried.
  token_service_.IssueErrorForAllPendingRequestsForAccount(
      account_1,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  token_service_.IssueErrorForAllPendingRequestsForAccount(
      account_2,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  token_service_.IssueErrorForAllPendingRequestsForAccount(
      account_3,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  // Success on retry.
  OAuth2AccessTokenConsumer::TokenResponse success_response;
  success_response.access_token = kAccessToken;
  success_response.access_token = "token_1";
  token_service_.IssueAllTokensForAccount(account_1, success_response);
  success_response.access_token = "token_2";
  token_service_.IssueAllTokensForAccount(account_2, success_response);
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  success_response.access_token = "token_3";
  token_service_.IssueAllTokensForAccount(account_3, success_response);
  EXPECT_EQ(FetchStatus::kSuccess, GetFetchStatus());
  // Check result.
  EXPECT_EQ(3u, account_id_token_pairs_.size());
  EXPECT_EQ(account_1, account_id_token_pairs_[0].account_id);
  EXPECT_EQ(account_2, account_id_token_pairs_[1].account_id);
  EXPECT_EQ(account_3, account_id_token_pairs_[2].account_id);
  EXPECT_EQ("token_1", account_id_token_pairs_[0].token);
  EXPECT_EQ("token_2", account_id_token_pairs_[1].token);
  EXPECT_EQ("token_3", account_id_token_pairs_[2].token);
}

TEST_F(OAuthMultiloginTokenFetcherTest, MultipleAccountsPersistentError) {
  const CoreAccountId account_1("account_1");
  const CoreAccountId account_2("account_2");
  const CoreAccountId account_3("account_3");
  token_service_.UpdateCredentials(account_1, "refresh_token");
  token_service_.UpdateCredentials(account_2, "refresh_token");
  token_service_.UpdateCredentials(account_3, "refresh_token");
  std::unique_ptr<OAuthMultiloginTokenFetcher> fetcher =
      CreateFetcher({account_1, account_2, account_3});
  EXPECT_EQ(FetchStatus::kPending, GetFetchStatus());
  token_service_.IssueErrorForAllPendingRequestsForAccount(
      account_2,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  // Fail as soon as one of the accounts is in error.
  EXPECT_EQ(FetchStatus::kFailure, GetFetchStatus());
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, error_.state());
}

}  // namespace signin
