// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/primary_account_policy_manager_impl.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/primary_account_policy_manager_impl.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrimaryAccountPolicyManagerImplTest : public testing::Test {
 public:
  PrimaryAccountPolicyManagerImplTest()
      : test_signin_client_(&user_prefs_),
        token_service_(
            &user_prefs_,
            std::make_unique<FakeProfileOAuth2TokenServiceDelegate>()),
        primary_account_manager_(&test_signin_client_,
                                 &token_service_,
                                 &account_tracker_,
                                 signin::AccountConsistencyMethod::kDisabled,
                                 nullptr /*policy_manager*/),
        policy_manager_(&test_signin_client_) {
    PrimaryAccountManager::RegisterProfilePrefs(user_prefs_.registry());
    PrimaryAccountManager::RegisterPrefs(local_state_.registry());

    policy_manager_.InitializePolicy(&local_state_, &primary_account_manager_);
  }

  ~PrimaryAccountPolicyManagerImplTest() override {
    test_signin_client_.Shutdown();
  }

  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  TestingPrefServiceSimple local_state_;
  TestSigninClient test_signin_client_;
  ProfileOAuth2TokenService token_service_;
  AccountTrackerService account_tracker_;
  PrimaryAccountManager primary_account_manager_;
  PrimaryAccountPolicyManagerImpl policy_manager_;
};

TEST_F(PrimaryAccountPolicyManagerImplTest, Prohibited) {
  local_state_.SetString(prefs::kGoogleServicesUsernamePattern,
                         ".*@google.com");
  EXPECT_TRUE(policy_manager_.IsAllowedUsername("test@google.com"));
  EXPECT_TRUE(policy_manager_.IsAllowedUsername("happy@google.com"));
  EXPECT_FALSE(policy_manager_.IsAllowedUsername("test@invalid.com"));
  EXPECT_FALSE(policy_manager_.IsAllowedUsername("test@notgoogle.com"));
  EXPECT_FALSE(policy_manager_.IsAllowedUsername(std::string()));
}

TEST_F(PrimaryAccountPolicyManagerImplTest, TestAlternateWildcard) {
  // Test to make sure we accept "*@google.com" as a pattern (treat it as if
  // the admin entered ".*@google.com").
  local_state_.SetString(prefs::kGoogleServicesUsernamePattern, "*@google.com");
  EXPECT_TRUE(policy_manager_.IsAllowedUsername("test@google.com"));
  EXPECT_TRUE(policy_manager_.IsAllowedUsername("happy@google.com"));
  EXPECT_FALSE(policy_manager_.IsAllowedUsername("test@invalid.com"));
  EXPECT_FALSE(policy_manager_.IsAllowedUsername("test@notgoogle.com"));
  EXPECT_FALSE(policy_manager_.IsAllowedUsername(std::string()));
}
