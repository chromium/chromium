// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/promotion/promotion_eligibility_checker.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kExpectedAccessToken[] = "access_token";

}  // namespace

namespace enterprise_promotion {

class PromotionEligibilityCheckerTest : public testing::Test {
 public:
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  void SetUp() override {
    account_id_ = identity_test_env()
                      ->MakePrimaryAccountAvailable(
                          "test@example.com", signin::ConsentLevel::kSignin)
                      .account_id;

    checker_ = std::make_unique<PromotionEligibilityChecker>(
        identity_test_env()->identity_manager());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  CoreAccountId account_id_;
  std::unique_ptr<PromotionEligibilityChecker> checker_;
};

TEST_F(PromotionEligibilityCheckerTest, FetchAccessTokenSuccess) {
  std::string actual_oauth_token;

  checker_->FetchAccessToken(account_id_);

  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kExpectedAccessToken, base::Time::Max());

  actual_oauth_token = checker_->GetFetchedTokenForTesting();

  EXPECT_EQ(actual_oauth_token, kExpectedAccessToken);
}

}  // namespace enterprise_promotion
