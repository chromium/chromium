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
  base::Optional<signin::AccessTokenInfo> maybe_account_info;
  SafeBrowsingPrimaryAccountTokenFetcher fetcher(
      identity_test_environment_.identity_manager());
  fetcher.Start(signin::ConsentLevel::kNotRequired,
                base::BindOnce(
                    [](base::Optional<signin::AccessTokenInfo>* target_info,
                       base::Optional<signin::AccessTokenInfo> info) {
                      *target_info = info;
                    },
                    &maybe_account_info));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "token", base::Time::Now());
  ASSERT_TRUE(maybe_account_info.has_value());
  EXPECT_EQ(maybe_account_info.value().token, "token");
}

TEST_F(SafeBrowsingPrimaryAccountTokenFetcherTest, Failure) {
  identity_test_environment_.MakeUnconsentedPrimaryAccountAvailable(
      "test@example.com");
  base::Optional<signin::AccessTokenInfo> maybe_account_info;
  SafeBrowsingPrimaryAccountTokenFetcher fetcher(
      identity_test_environment_.identity_manager());
  fetcher.Start(signin::ConsentLevel::kNotRequired,
                base::BindOnce(
                    [](base::Optional<signin::AccessTokenInfo>* target_info,
                       base::Optional<signin::AccessTokenInfo> info) {
                      *target_info = info;
                    },
                    &maybe_account_info));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
          GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  ASSERT_FALSE(maybe_account_info.has_value());
}

TEST_F(SafeBrowsingPrimaryAccountTokenFetcherTest, NoSyncingAccount) {
  identity_test_environment_.MakeUnconsentedPrimaryAccountAvailable(
      "test@example.com");
  base::Optional<signin::AccessTokenInfo> maybe_account_info;
  SafeBrowsingPrimaryAccountTokenFetcher fetcher(
      identity_test_environment_.identity_manager());
  fetcher.Start(signin::ConsentLevel::kSync,
                base::BindOnce(
                    [](base::Optional<signin::AccessTokenInfo>* target_info,
                       base::Optional<signin::AccessTokenInfo> info) {
                      *target_info = info;
                    },
                    &maybe_account_info));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "token", base::Time::Now());
  ASSERT_FALSE(maybe_account_info.has_value());
}

TEST_F(SafeBrowsingPrimaryAccountTokenFetcherTest, SyncSuccess) {
  identity_test_environment_.MakePrimaryAccountAvailable("test@example.com");
  base::Optional<signin::AccessTokenInfo> maybe_account_info;
  SafeBrowsingPrimaryAccountTokenFetcher fetcher(
      identity_test_environment_.identity_manager());
  fetcher.Start(signin::ConsentLevel::kSync,
                base::BindOnce(
                    [](base::Optional<signin::AccessTokenInfo>* target_info,
                       base::Optional<signin::AccessTokenInfo> info) {
                      *target_info = info;
                    },
                    &maybe_account_info));
  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          "token", base::Time::Now());
  ASSERT_TRUE(maybe_account_info.has_value());
  EXPECT_EQ(maybe_account_info.value().token, "token");
}

}  // namespace safe_browsing
