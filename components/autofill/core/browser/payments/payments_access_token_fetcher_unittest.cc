// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_access_token_fetcher.h"

#include <memory>
#include <string>
#include <variant>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {
constexpr std::string kTestAccessToken = "token";
}

class PaymentsAccessTokenFetcherTest
    : public testing::Test,
      signin::IdentityManager::DiagnosticsObserver {
 public:
  void SetUp() override {
    identity_test_env_.identity_manager()->AddDiagnosticsObserver(this);
    identity_test_env_.MakePrimaryAccountAvailable(
        "example@gmail.com", signin::ConsentLevel::kSignin);
    token_fetcher_ = std::make_unique<PaymentsAccessTokenFetcher>(
        *identity_test_env_.identity_manager());
  }

  void TearDown() override {
    identity_test_env_.identity_manager()->RemoveDiagnosticsObserver(this);
  }

  // IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRemovedFromCache(const CoreAccountId& account_id,
                                     const signin::ScopeSet& scopes) override {
    token_removed_from_cache_ = true;
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<PaymentsAccessTokenFetcher> token_fetcher_;
  base::MockCallback<base::OnceCallback<void(
      const std::variant<GoogleServiceAuthError, std::string>&)>>
      callback_;
  bool token_removed_from_cache_ = false;
};

// Tests that it should early abort if there is a pending request.
TEST_F(PaymentsAccessTokenFetcherTest, AbortIfRequestPending) {
  EXPECT_CALL(callback_, Run(testing::_)).Times(0);

  token_fetcher_->GetAccessToken(/*invalidate_old=*/false, callback_.Get());
  token_fetcher_->GetAccessToken(/*invalidate_old=*/false, callback_.Get());

  EXPECT_CALL(
      callback_,
      Run(std::variant<GoogleServiceAuthError, std::string>(kTestAccessToken)))
      .Times(1);

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kTestAccessToken, base::Time::Now() + base::Days(10));
}

// Tests that if an access token is prefetched, it is returned immediately.
TEST_F(PaymentsAccessTokenFetcherTest, ReusedToken) {
  token_fetcher_->set_access_token_for_testing(kTestAccessToken);
  EXPECT_CALL(
      callback_,
      Run(std::variant<GoogleServiceAuthError, std::string>(kTestAccessToken)))
      .Times(1);

  token_fetcher_->GetAccessToken(/*invalidate_old=*/false, callback_.Get());
}

// Tests that the access token fetching succeeded.
TEST_F(PaymentsAccessTokenFetcherTest, FetchTokenSucceeded) {
  EXPECT_CALL(
      callback_,
      Run(std::variant<GoogleServiceAuthError, std::string>(kTestAccessToken)))
      .Times(1);

  token_fetcher_->GetAccessToken(/*invalidate_old=*/false, callback_.Get());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kTestAccessToken, base::Time::Now() + base::Days(10));
}

// Tests that the access token fetching succeeded when old token needs to be
// invalidated.
TEST_F(PaymentsAccessTokenFetcherTest, FetchTokenSucceededInvalidateOld) {
  token_fetcher_->set_access_token_for_testing("Some other token");
  EXPECT_CALL(
      callback_,
      Run(std::variant<GoogleServiceAuthError, std::string>(kTestAccessToken)))
      .Times(1);

  token_fetcher_->GetAccessToken(/*invalidate_old=*/true, callback_.Get());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kTestAccessToken, base::Time::Now() + base::Days(10));

  EXPECT_TRUE(token_removed_from_cache_);
}

// Tests that the access token fetching failed.
TEST_F(PaymentsAccessTokenFetcherTest, FetchTokenFailed) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT);
  EXPECT_CALL(callback_,
              Run(std::variant<GoogleServiceAuthError, std::string>(error)))
      .Times(1);

  token_fetcher_->GetAccessToken(/*invalidate_old=*/false, callback_.Get());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      error);
}

}  // namespace autofill::payments
