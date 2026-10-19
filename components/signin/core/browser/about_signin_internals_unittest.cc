// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/about_signin_internals.h"

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::base::test::DictionaryHasValues;
using ::testing::ElementsAre;

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
        account_info.GetAccountId(), kCanOverrideAccountInfoCapabilityName,
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
        account_info.GetAccountId(), kCanFetchFamilyMemberInfoCapabilityName,
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
        account_info.GetAccountId(), kCanFetchFamilyMemberInfoCapabilityName,
        channel));

    // When capability is enabled, returns true.
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_override_account_info(true);
    identity_test_env_.UpdateAccountInfoForAccount(account_info);

    EXPECT_TRUE(about_signin_internals_->CanOverrideAccountCapability(
        account_info.GetAccountId(), kCanFetchFamilyMemberInfoCapabilityName,
        channel));

    // When capability is disabled, returns false.
    mutator.set_can_override_account_info(false);
    identity_test_env_.UpdateAccountInfoForAccount(account_info);

    EXPECT_FALSE(about_signin_internals_->CanOverrideAccountCapability(
        account_info.GetAccountId(), kCanFetchFamilyMemberInfoCapabilityName,
        channel));
  }
}

TEST_F(AboutSigninInternalsTest,
       GetSigninStatus_NullSigninErrorControllerAndAccountReconcilor) {
  identity_test_env_.MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);

  AboutSigninInternals internals(identity_test_env_.identity_manager(),
                                 /*signin_error_controller=*/nullptr,
                                 signin::AccountConsistencyMethod::kDisabled,
                                 &signin_client_,
                                 /*account_reconcilor=*/nullptr);

  base::DictValue status = internals.GetSigninStatus();
  const base::ListValue* signin_info = status.FindList("signin_info");
  ASSERT_TRUE(signin_info);
  ASSERT_FALSE(signin_info->empty());
  const base::DictValue* basic_section = (*signin_info)[0].GetIfDict();
  ASSERT_TRUE(basic_section);
  const base::ListValue* basic_data = basic_section->FindList("data");
  ASSERT_TRUE(basic_data);

  for (const base::Value& entry : *basic_data) {
    const std::string* label = entry.GetDict().FindString("label");
    ASSERT_TRUE(label);
    EXPECT_NE(*label, "Auth Error");
    EXPECT_NE(*label, "Auth Error Account Id");
    EXPECT_NE(*label, "Auth Error Username");
    EXPECT_NE(*label, "Account Reconcilor blocked");
    EXPECT_NE(*label, "Account Reconcilor State");
  }

  internals.Shutdown();
}

TEST_F(AboutSigninInternalsTest, ResetSigninPrefs) {
  constexpr const char* kSigninPrefs[] = {
      "google.services.signin.AUTHENTICATION_RESULT_RECEIVED.value",
      "google.services.signin.AUTHENTICATION_RESULT_RECEIVED.time",
      "google.services.signin.REFRESH_TOKEN_RECEIVED.value",
      "google.services.signin.REFRESH_TOKEN_RECEIVED.time",
      "google.services.signin.LAST_REFRESH_TOKEN_ACCOUNT_ID.value",
      "google.services.signin.LAST_REFRESH_TOKEN_ACCOUNT_ID.time",
      "google.services.signin.LAST_SIGNIN_ACCESS_POINT.value",
      "google.services.signin.LAST_SIGNIN_ACCESS_POINT.time",
      "google.services.signin.LAST_SIGNOUT_SOURCE.value",
      "google.services.signin.LAST_SIGNOUT_SOURCE.time",
  };

  for (const char* pref : kSigninPrefs) {
    pref_service_.SetString(pref, "non_empty");
  }

  AboutSigninInternals::ResetSigninPrefs(&pref_service_);

  for (const char* pref : kSigninPrefs) {
    EXPECT_TRUE(pref_service_.HasPrefPath(pref));
    EXPECT_EQ(pref_service_.GetString(pref), "");
  }
}

TEST_F(AboutSigninInternalsTest, AccountReconcilorEvents) {
  static_cast<AccountReconcilor::Observer*>(about_signin_internals())
      ->OnReconcileFinished(AccountReconcilor::Trigger::kTokenChange,
                            AccountReconcilor::Operation::kMultilogin,
                            GoogleServiceAuthError::AuthErrorNone());
  static_cast<AccountReconcilor::Observer*>(about_signin_internals())
      ->OnReconcileFinished(
          AccountReconcilor::Trigger::kCookieChange,
          AccountReconcilor::Operation::kLogout,
          GoogleServiceAuthError::FromServiceError("Logout failed"));

  base::DictValue status = about_signin_internals()->GetSigninStatus();
  const base::ListValue* events = status.FindList("accountReconcilorEvents");
  ASSERT_TRUE(events);
  EXPECT_THAT(
      *events,
      ElementsAre(
          DictionaryHasValues(base::DictValue()
                                  .Set("trigger", "TokenChange")
                                  .Set("operation", "Multilogin")
                                  .Set("result", "OK")),
          DictionaryHasValues(base::DictValue()
                                  .Set("trigger", "CookieChange")
                                  .Set("operation", "Logout")
                                  .Set("result",
                                       "Failure: Service responded with error: "
                                       "'Logout failed'"))));
}

TEST_F(AboutSigninInternalsTest, GetSigninStatus_IsPii) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.GetAccountId(),
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  base::DictValue status = about_signin_internals()->GetSigninStatus();
  const base::ListValue* signin_info = status.FindList("signin_info");
  ASSERT_TRUE(signin_info);

  std::set<std::string> pii_labels;
  std::set<std::string> non_pii_labels;
  for (const base::Value& section : *signin_info) {
    const base::ListValue* data = section.GetDict().FindList("data");
    ASSERT_TRUE(data);
    for (const base::Value& entry : *data) {
      const base::DictValue& entry_dict = entry.GetDict();
      const std::string* label = entry_dict.FindString("label");
      ASSERT_TRUE(label);
      std::optional<bool> is_pii = entry_dict.FindBool("isPii");
      ASSERT_TRUE(is_pii.has_value());
      if (*is_pii) {
        pii_labels.insert(*label);
      } else {
        non_pii_labels.insert(*label);
      }
    }
  }

  std::set<std::string> expected_pii_labels = {
      "Account Id",
      "Gaia Id",
      "Username",
      "Auth Error Account Id",
      "Auth Error Username",
#if !BUILDFLAG(IS_CHROMEOS)
      "RefreshToken Account Id",
#endif
  };
  EXPECT_EQ(pii_labels, expected_pii_labels);
  EXPECT_FALSE(non_pii_labels.empty());
}

}  // namespace
