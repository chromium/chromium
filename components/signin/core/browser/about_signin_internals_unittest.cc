// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/about_signin_internals.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestEmail[] = "test@example.com";

class AboutSigninInternalsTest : public ::testing::Test {
 public:
  AboutSigninInternalsTest()
      : signin_client_(&pref_service_),
        signin_error_controller_(
            SigninErrorController::AccountMode::ANY_ACCOUNT,
            identity_test_env_.identity_manager()),
        account_reconcilor_(
            identity_test_env_.identity_manager(),
            &signin_client_,
            std::make_unique<signin::AccountReconcilorDelegate>()) {
    AboutSigninInternals::RegisterPrefs(pref_service_.registry());
    about_signin_internals_ = std::make_unique<AboutSigninInternals>(
        identity_test_env_.identity_manager(), &signin_error_controller_,
        signin::AccountConsistencyMethod::kDisabled, &signin_client_,
        &account_reconcilor_);
  }

  ~AboutSigninInternalsTest() override {
    about_signin_internals_->Shutdown();
    about_signin_internals_.reset();
    account_reconcilor_.Shutdown();
    signin_error_controller_.Shutdown();
  }

  AboutSigninInternals* about_signin_internals() {
    return about_signin_internals_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestSigninClient signin_client_;
  signin::IdentityTestEnvironment identity_test_env_;
  SigninErrorController signin_error_controller_;
  AccountReconcilor account_reconcilor_;
  std::unique_ptr<AboutSigninInternals> about_signin_internals_;
};

TEST_F(AboutSigninInternalsTest,
       CanOverrideAccountCapability_DisallowOverridingCanOverrideAccountInfo) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(kTestEmail);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_override_account_info(true);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  for (version_info::Channel channel : {
           version_info::Channel::UNKNOWN,
           version_info::Channel::CANARY,
           version_info::Channel::DEV,
           version_info::Channel::BETA,
           version_info::Channel::STABLE,
       }) {
    EXPECT_FALSE(about_signin_internals_->CanOverrideAccountCapability(
        account_info.account_id, kCanOverrideAccountInfoCapabilityName,
        channel));
  }
}

TEST_F(AboutSigninInternalsTest,
       CanOverrideAccountCapability_PreReleaseChannels) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(kTestEmail);

  for (version_info::Channel channel : {
           version_info::Channel::UNKNOWN,
           version_info::Channel::CANARY,
           version_info::Channel::DEV,
       }) {
    EXPECT_TRUE(about_signin_internals_->CanOverrideAccountCapability(
        account_info.account_id, kCanFetchFamilyMemberInfoCapabilityName,
        channel));
  }
}

TEST_F(AboutSigninInternalsTest,
       CanOverrideAccountCapability_StableAndBetaChannels) {
  AccountInfo account_info =
      identity_test_env_.MakeAccountAvailable(kTestEmail);

  for (version_info::Channel channel : {
           version_info::Channel::BETA,
           version_info::Channel::STABLE,
       }) {
    // False by default (capability not enabled on account).
    EXPECT_FALSE(about_signin_internals_->CanOverrideAccountCapability(
        account_info.account_id, kCanFetchFamilyMemberInfoCapabilityName,
        channel));

    // When capability is enabled, returns true.
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_override_account_info(true);
    identity_test_env_.UpdateAccountInfoForAccount(account_info);

    EXPECT_TRUE(about_signin_internals_->CanOverrideAccountCapability(
        account_info.account_id, kCanFetchFamilyMemberInfoCapabilityName,
        channel));

    // When capability is disabled, returns false.
    mutator.set_can_override_account_info(false);
    identity_test_env_.UpdateAccountInfoForAccount(account_info);

    EXPECT_FALSE(about_signin_internals_->CanOverrideAccountCapability(
        account_info.account_id, kCanFetchFamilyMemberInfoCapabilityName,
        channel));
  }
}

}  // namespace
