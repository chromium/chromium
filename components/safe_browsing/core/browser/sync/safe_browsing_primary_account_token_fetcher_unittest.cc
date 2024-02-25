// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetch_tracker.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingPrimaryAccountTokenFetcherTest : public ::testing::Test {
 public:
  SafeBrowsingPrimaryAccountTokenFetcherTest() {}

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_environment_;
};

TEST_F(SafeBrowsingPrimaryAccountTokenFetcherTest, Success) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  std::string access_token;
  SafeBrowsingPrimaryAccountTokenFetcher fetcher(
      identity_test_environment_.identity_manager());
  fetcher.Start(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "token", base::Time::Now());
  EXPECT_EQ(access_token, "token");
}

TEST_F(SafeBrowsingPrimaryAccountTokenFetcherTest, Failure) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  std::string access_token;
  SafeBrowsingPrimaryAccountTokenFetcher fetcher(
      identity_test_environment_.identity_manager());
  fetcher.Start(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  ASSERT_TRUE(access_token.empty());
}

TEST_F(SafeBrowsingPrimaryAccountTokenFetcherTest,
       SuccessWithConsentedPrimaryAccount) {
  // TODO(https://crbug.com/40066949): Delete this test after UNO phase 3
  // migration is complete. See `ConsentLevel::kSync` documentation for more
  // details.
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSync);
  std::string access_token;
  SafeBrowsingPrimaryAccountTokenFetcher fetcher(
      identity_test_environment_.identity_manager());
  fetcher.Start(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "token", base::Time::Now());
  EXPECT_EQ(access_token, "token");
}

// Verifies that destruction of a SafeBrowsingPrimaryAccountTokenFetcher
// instance from within the client callback that the token was fetched doesn't
// cause a crash.
TEST_F(SafeBrowsingPrimaryAccountTokenFetcherTest,
       FetcherDestroyedFromWithinOnTokenFetchedCallback) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  std::string access_token;

  // Destroyed in the token fetch callback.
  auto* fetcher = new SafeBrowsingPrimaryAccountTokenFetcher(
      identity_test_environment_.identity_manager());

  fetcher->Start(base::BindOnce(
      [](std::string* target_token,
         SafeBrowsingPrimaryAccountTokenFetcher* fetcher,
         const std::string& token) {
        *target_token = token;
        delete fetcher;
      },
      &access_token, fetcher));

  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  ASSERT_TRUE(access_token.empty());
}

// Verifies that destruction of a SafeBrowsingPrimaryAccountTokenFetcher
// instance from within the client callback that the token was fetched doesn't
// cause a crash when invoked due to the token fetch timing out.
TEST_F(SafeBrowsingPrimaryAccountTokenFetcherTest,
       FetcherDestroyedFromWithinOnTokenFetchedCallbackInvokedOnTimeout) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  std::string access_token;
  bool callback_invoked = false;

  // Destroyed in the token fetch callback, which is invoked on timeout.
  auto* fetcher = new SafeBrowsingPrimaryAccountTokenFetcher(
      identity_test_environment_.identity_manager());

  fetcher->Start(base::BindOnce(
      [](bool* on_invoked_flag, std::string* target_token,
         SafeBrowsingPrimaryAccountTokenFetcher* fetcher,
         const std::string& token) {
        *on_invoked_flag = true;
        *target_token = token;
        delete fetcher;
      },
      &callback_invoked, &access_token, fetcher));

  // Trigger a timeout of the fetch, which will invoke the client callback
  // passed to the fetcher.
  task_environment_.FastForwardBy(
      base::Milliseconds(kTokenFetchTimeoutDelayFromMilliseconds));
  ASSERT_TRUE(callback_invoked);
  ASSERT_TRUE(access_token.empty());
}

// Verifies that completion of an access token fetch followed by the timeout
// period for the fetch being reached doesn't cause a crash. Regression test for
// crbug.com/1276273.
TEST_F(SafeBrowsingPrimaryAccountTokenFetcherTest, TimeoutAfterSuccess) {
  identity_test_environment_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  std::string access_token;
  SafeBrowsingPrimaryAccountTokenFetcher fetcher(
      identity_test_environment_.identity_manager());
  fetcher.Start(
      base::BindOnce([](std::string* target_token,
                        const std::string& token) { *target_token = token; },
                     &access_token));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "token", base::Time::Now());
  EXPECT_EQ(access_token, "token");

  // Trigger a timeout of the now-completed fetch: this should not cause any
  // adverse effects (e.g., a crash).
  task_environment_.FastForwardBy(
      base::Milliseconds(kTokenFetchTimeoutDelayFromMilliseconds));
}

}  // namespace safe_browsing
