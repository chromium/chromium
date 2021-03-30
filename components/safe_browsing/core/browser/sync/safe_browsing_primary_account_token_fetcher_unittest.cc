// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/common/test_task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingPrimaryAccountTokenFetcherTest : public ::testing::Test {
 public:
  SafeBrowsingPrimaryAccountTokenFetcherTest()
      : task_environment_(CreateTestTaskEnvironment()) {}

 protected:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
};

TEST_F(SafeBrowsingPrimaryAccountTokenFetcherTest, Success) {
  identity_test_environment_.MakeUnconsentedPrimaryAccountAvailable(
      "test@example.com");
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
  identity_test_environment_.MakeUnconsentedPrimaryAccountAvailable(
      "test@example.com");
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
  identity_test_environment_.MakePrimaryAccountAvailable("test@example.com");
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

}  // namespace safe_browsing
