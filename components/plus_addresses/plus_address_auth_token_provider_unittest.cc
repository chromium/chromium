// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_auth_token_provider.h"

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/plus_addresses/features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {

class PlusAddressAuthTokenProviderTest : public ::testing::Test {
 public:
  PlusAddressAuthTokenProviderTest() {
    // Init the feature param to add `test_scope` to GetUnconsentedOAuth2Scopes
    features_.InitAndEnableFeatureWithParameters(
        kFeature, {{kEnterprisePlusAddressOAuthScope.name, test_scope_}});

    // Time-travel back to 1970 so that we can test with base::Time::FromDoubleT
    clock_.SetNow(base::Time::FromDoubleT(1));
  }

 protected:
  // A blocking helper that signs the user in and gets an OAuth token with our
  // test scope.
  // Note: this blocks indefinitely if there are no listeners for token
  // creation. This means it must be called after GetAuthToken.
  void WaitForSignInAndToken() {
    identity_test_env_.MakePrimaryAccountAvailable(
        test_email_address_, signin::ConsentLevel::kSignin);
    identity_test_env_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
            test_token_, test_token_expiration_time_, "id", test_scopes_);
  }

  // A blocking helper that gets an OAuth token for our test scope that expires
  // at `expiration_time`.
  void WaitForToken(base::Time expiration_time) {
    identity_test_env_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
            test_token_, expiration_time, "id", test_scopes_);
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  void AdvanceTimeTo(base::Time now) {
    ASSERT_GE(now, clock_.Now());
    clock_.SetNow(now);
  }

  base::Clock* test_clock() { return &clock_; }

  std::string test_token_ = "access_token";
  std::string test_scope_ = "https://googleapis.com/test.scope";
  signin::ScopeSet test_scopes_ = {test_scope_};
  base::Time test_token_expiration_time_ = base::Time::FromDoubleT(1000);

 private:
  // Required by `signin::IdentityTestEnvironment`.
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

  base::test::ScopedFeatureList features_;
  base::SimpleTestClock clock_;
  std::string test_email_address_ = "foo@gmail.com";
};

TEST_F(PlusAddressAuthTokenProviderTest, TokenRequestedBeforeSignin) {
  PlusAddressAuthTokenProvider provider(identity_manager(), test_scopes_);

  bool ran_callback = false;
  provider.GetAuthToken(base::BindLambdaForTesting([&](std::string token) {
    EXPECT_EQ(token, test_token_);
    ran_callback = true;
  }));

  // The callback is run only after signin.
  EXPECT_FALSE(ran_callback);
  WaitForSignInAndToken();
  EXPECT_TRUE(ran_callback);
}

TEST_F(PlusAddressAuthTokenProviderTest, TokenRequestedUserNeverSignsIn) {
  PlusAddressAuthTokenProvider provider(identity_manager(), test_scopes_);

  base::MockOnceCallback<void(std::string)> callback;
  EXPECT_CALL(callback, Run(testing::_)).Times(0);
  provider.GetAuthToken(callback.Get());
}

TEST_F(PlusAddressAuthTokenProviderTest, TokenRequestedAfterExpiration) {
  PlusAddressAuthTokenProvider provider(identity_manager(), test_scopes_);
  // Make an initial OAuth token request.
  base::MockOnceCallback<void(std::string)> first_callback;
  provider.GetAuthToken(first_callback.Get());
  EXPECT_CALL(first_callback, Run(test_token_)).Times(1);

  // Sign in, get a token, and fast-forward to after it is expired.
  WaitForSignInAndToken();
  base::Time now = test_token_expiration_time_ + base::Seconds(1);
  AdvanceTimeTo(now);

  // Issue another request for an OAuth token.
  base::MockOnceCallback<void(std::string)> second_callback;
  provider.GetAuthToken(second_callback.Get());

  // Callback is only run once the new OAuth token request has completed.
  EXPECT_CALL(second_callback, Run(test_token_)).Times(1);
  WaitForToken(/*expiration_time=*/now + base::Hours(1));
}

}  // namespace plus_addresses
