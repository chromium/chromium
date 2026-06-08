// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_enablement_service_impl.h"

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/account_settings/account_settings.h"
#include "components/account_settings/account_settings_features.h"
#include "components/account_settings/mock_account_setting_service.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_enablement_service_impl_test_api.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {
namespace {

using testing::Return;

MATCHER_P(AccountSettingWithName, name, "") {
  return std::string(arg.name) == name;
}

class MockPersonalContextEnablementServiceObserver
    : public PersonalContextEnablementService::Observer {
 public:
  MOCK_METHOD(void,
              OnEnablementStateChanged,
              (PersonalContextEnablementState),
              (override));
};

class PersonalContextEnablementServiceImplTest : public testing::Test {
 public:
  PersonalContextEnablementServiceImplTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kPersonalContext,
                              features::kPersonalContextFirstRun},
        /*disabled_features=*/{});

    SetPrefs();
    CreateService("us");
    SignIn("test@gmail.com");
  }
  ~PersonalContextEnablementServiceImplTest() override = default;

 protected:
  void SignIn(const std::string& email,
              bool is_underaged = false,
              bool is_managed = false) {
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    AccountInfo::Builder builder(account_info);
    if (is_managed) {
      builder.SetHostedDomain("example.com");
    } else {
      builder.SetHostedDomain("");
    }
    AccountCapabilities capabilities = account_info.GetAccountCapabilities();
    AccountCapabilitiesTestMutator mutator(&capabilities);
    mutator.set_can_use_model_execution_features(!is_underaged);
    builder.UpdateAccountCapabilitiesWith(capabilities);
    identity_test_env_.UpdateAccountInfoForAccount(builder.Build());
  }

  void CreateService(const std::string& country_code,
                     const std::string& locale = "en-US") {
    service_ = std::make_unique<PersonalContextEnablementServiceImpl>(
        &mock_account_settings_service_, identity_test_env_.identity_manager(),
        &pref_service_, GeoIpCountryCode(base::ToUpperASCII(country_code)),
        locale);
  }

  void SetPrefs() {
    personal_context::prefs::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.SetBoolean(
        personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown,
        false);
    pref_service_.SetBoolean(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        true);
    // Enable all by default to satisfy requirements.
    ON_CALL(mock_account_settings_service_, GetBoolean(testing::_))
        .WillByDefault(Return(true));
  }

  PersonalContextEnablementServiceImpl& service() { return *service_; }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  testing::NiceMock<account_settings::MockAccountSettingService>
      mock_account_settings_service_;
  std::unique_ptr<PersonalContextEnablementServiceImpl> service_;
};

// Verifies that the debug override feature correctly forces the enablement
// state regardless of other requirements.
TEST_F(PersonalContextEnablementServiceImplTest, ForcedEnablementState) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kPersonalContextForceEnablementState,
        {{"state", "0"}});
    EXPECT_EQ(service().GetEnablementState(),
              PersonalContextEnablementState::kDisabledNotEligible);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kPersonalContextForceEnablementState,
        {{"state", "1"}});
    EXPECT_EQ(service().GetEnablementState(),
              PersonalContextEnablementState::kDisabledNeedsOptIn);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kPersonalContextForceEnablementState,
        {{"state", "2"}});
    EXPECT_EQ(service().GetEnablementState(),
              PersonalContextEnablementState::
                  kDisabledViaPersonalIntelligenceInAutofillToggle);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kPersonalContextForceEnablementState,
        {{"state", "3"}});
    EXPECT_EQ(service().GetEnablementState(),
              PersonalContextEnablementState::kEnabledShouldShowNotice);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kPersonalContextForceEnablementState,
        {{"state", "4"}});
    EXPECT_EQ(service().GetEnablementState(),
              PersonalContextEnablementState::kEnabled);
  }
}

// Verifies that the service is disabled when all related feature flags are off.
TEST_F(PersonalContextEnablementServiceImplTest, DisabledWhenFeaturesAreOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kPersonalContext,
                             features::kPersonalContextFirstRun});

  EXPECT_EQ(PersonalContextEnablementServiceImplTestApi(&service())
                .ComputeEnablementState(),
            PersonalContextEnablementState::kDisabledNotEligible);
}

// Verifies that the main feature flag (kPersonalContext) is a hard requirement.
TEST_F(PersonalContextEnablementServiceImplTest, DisabledWhenMainFeatureIsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kPersonalContextFirstRun},
      /*disabled_features=*/{features::kPersonalContext});

  EXPECT_EQ(PersonalContextEnablementServiceImplTestApi(&service())
                .ComputeEnablementState(),
            PersonalContextEnablementState::kDisabledNotEligible);
}

// Verifies that the service is enabled when all feature flags and other
// requirements (set up in the test fixture) are met.
TEST_F(PersonalContextEnablementServiceImplTest, EnabledWhenAllFeaturesAreOn) {
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kEnabled);
}

// Verifies that the state is `kEnabled` when the notice no longer needs to be
// shown and the toggle is "on".
TEST_F(PersonalContextEnablementServiceImplTest,
       EnabledShouldNotShowNoticeAndToggleIsOn) {
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown,
      false);
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      true);
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kEnabled);
}

// Verifies that the state is `kEnabledShouldShowNotice` when the notice still
// needs to be shown and the toggle is "on".
TEST_F(PersonalContextEnablementServiceImplTest,
       EnabledShouldShowNoticeAndToggleIsOn) {
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown,
      true);
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      true);
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kEnabledShouldShowNotice);
}

// Verifies that the state is `kDisabledViaPersonalIntelligenceInAutofillToggle`
// when the toggle is off.
TEST_F(PersonalContextEnablementServiceImplTest, DisabledViaToggle) {
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      false);

  // The toggle takes precedence no matter if the notice should still be shown.
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown,
      false);
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::
                kDisabledViaPersonalIntelligenceInAutofillToggle);

  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown,
      true);
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::
                kDisabledViaPersonalIntelligenceInAutofillToggle);
}

#if !BUILDFLAG(IS_CHROMEOS)  // Signing out does not work on ChromeOS.
// Verifies that the service is disabled when the user is not signed in.
TEST_F(PersonalContextEnablementServiceImplTest, DisabledWhenSignedOut) {
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kDisabledNotEligible);
}

// Verifies that the onboarding preference is reset when the user signs out,
// ensuring they see the notice again if they sign back in.
TEST_F(PersonalContextEnablementServiceImplTest, ClearsPrefOnSignout) {
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown,
      false);
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      false);
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_TRUE(pref_service_.GetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown));
  EXPECT_TRUE(pref_service_.GetBoolean(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Verifies that the service is disabled for underaged users.
TEST_F(PersonalContextEnablementServiceImplTest, DisabledWhenUnderaged) {
  SignIn("under@gmail.com", /*is_underaged=*/true);

  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kDisabledNotEligible);
}

// Verifies that the service is disabled for managed (enterprise) accounts.
TEST_F(PersonalContextEnablementServiceImplTest, DisabledWhenManaged) {
  SignIn("managed@example.com", /*is_underaged=*/false, /*is_managed=*/true);

  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kDisabledNotEligible);
}



// Verifies that the service fails safe (disabled) if the required
// `AccountSettingService` is missing.
TEST_F(PersonalContextEnablementServiceImplTest,
       DisabledWhenAccountSettingsServiceNotAvailable) {
  service_ = std::make_unique<PersonalContextEnablementServiceImpl>(
      nullptr, identity_test_env_.identity_manager(), &pref_service_,
      GeoIpCountryCode("US"), "en-US");

  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kDisabledNotEligible);
}

// Verifies that the service is disabled if the user has explicitly opted
// out of personal context in their global account settings.
TEST_F(PersonalContextEnablementServiceImplTest,
       DisabledWhenAccountOptedOutOfContext) {
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContext.name)))
      .WillOnce(Return(false));

  service().OnAccountSettingDataUpdated(
      account_settings::kAccountSettingContext.name);
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kDisabledNotEligible);
}

// Verifies that the service is disabled if no specific context sources
// (e.g. Photos, Workspace) are enabled, even if the global opt-in is on.
TEST_F(PersonalContextEnablementServiceImplTest,
       DisabledWhenNoContextSourcesEnabled) {
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContext.name)))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContextWorkspace.name)))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContextPhotos.name)))
      .WillOnce(Return(false));

  service().OnAccountSettingDataUpdated(
      account_settings::kAccountSettingContext.name);
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kDisabledNotEligible);
}

// Verifies that the service is enabled if at least one context source
// is enabled.
TEST_F(PersonalContextEnablementServiceImplTest,
       EnabledWhenAtLeastOneContextSourceEnabled) {
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContext.name)))
      .WillRepeatedly(Return(true));
  {
    // Only Workspace enabled.
    EXPECT_CALL(mock_account_settings_service_,
                GetBoolean(AccountSettingWithName(
                    account_settings::kAccountSettingContextWorkspace.name)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_account_settings_service_,
                GetBoolean(AccountSettingWithName(
                    account_settings::kAccountSettingContextPhotos.name)))
        .Times(0);

    service().OnAccountSettingDataUpdated(
        account_settings::kAccountSettingContext.name);
    EXPECT_EQ(service().GetEnablementState(),
              PersonalContextEnablementState::kEnabled);
  }
  {
    // Only Photos enabled.
    EXPECT_CALL(mock_account_settings_service_,
                GetBoolean(AccountSettingWithName(
                    account_settings::kAccountSettingContextWorkspace.name)))
        .WillOnce(Return(false));
    EXPECT_CALL(mock_account_settings_service_,
                GetBoolean(AccountSettingWithName(
                    account_settings::kAccountSettingContextPhotos.name)))
        .WillOnce(Return(true));

    service().OnAccountSettingDataUpdated(
        account_settings::kAccountSettingContext.name);
    EXPECT_EQ(service().GetEnablementState(),
              PersonalContextEnablementState::kEnabled);
  }
}

// Verifies that registered observers are correctly notified when the
// enablement state changes due to a preference update.
TEST_F(PersonalContextEnablementServiceImplTest,
       ObserversNotifiedOnEnablementStateChanged) {
  MockPersonalContextEnablementServiceObserver observer;
  service().AddObserver(&observer);

  // Initial state is kEnabled.
  ASSERT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kEnabled);

  // Trigger a change to `kEnabledShouldShowNotice` by setting a pref.
  EXPECT_CALL(observer,
              OnEnablementStateChanged(
                  PersonalContextEnablementState::kEnabledShouldShowNotice));
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown,
      true);

  // Trigger a change back to kEnabled.
  EXPECT_CALL(observer, OnEnablementStateChanged(
                            PersonalContextEnablementState::kEnabled));
  pref_service_.SetBoolean(
      personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown,
      false);

  service().RemoveObserver(&observer);
}

// Verifies that the internal state cache and observers are updated when
// account settings change.
TEST_F(PersonalContextEnablementServiceImplTest,
       CacheUpdatedOnAccountSettingChanged) {
  // Initial state is kEnabled.
  ASSERT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kEnabled);

  // Opt out of context in account settings.
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContext.name)))
      .WillRepeatedly(Return(false));

  // Notify the service that an account setting has changed.
  service().OnAccountSettingDataUpdated(
      account_settings::kAccountSettingContext.name);

  // The cache should be updated to kDisabledNotEligible.
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kDisabledNotEligible);

  // Opt back in.
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContext.name)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContextWorkspace.name)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContextPhotos.name)))
      .WillRepeatedly(Return(false));

  // Notify again.
  service().OnAccountSettingDataUpdated(
      account_settings::kAccountSettingContext.name);

  // The cache should be updated back to kEnabled.
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kEnabled);
}

TEST_F(PersonalContextEnablementServiceImplTest,
       DisabledWhenAccountSettingsServiceNotAvailableAndOptInEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kPersonalContext,
                            features::kPersonalContextFirstRun,
                            features::kPersonalContextFirstRunOptIn},
      /*disabled_features=*/{});

  service_ = std::make_unique<PersonalContextEnablementServiceImpl>(
      nullptr, identity_test_env_.identity_manager(), &pref_service_,
      GeoIpCountryCode("US"), "en-US");

  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kDisabledNotEligible);
}

TEST_F(PersonalContextEnablementServiceImplTest,
       NeedsOptInWhenAccountOptedOutOfContextAndOptInEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kPersonalContext,
                            features::kPersonalContextFirstRun,
                            features::kPersonalContextFirstRunOptIn},
      /*disabled_features=*/{});

  PersonalContextEnablementServiceImplTestApi(&service())
      .ComputeEnablementState();

  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContext.name)))
      .WillOnce(Return(false));

  service().OnAccountSettingDataUpdated(
      account_settings::kAccountSettingContext.name);
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kDisabledNeedsOptIn);
}

TEST_F(PersonalContextEnablementServiceImplTest,
       NeedsOptInWhenNoContextSourcesEnabledAndOptInEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kPersonalContext,
                            features::kPersonalContextFirstRun,
                            features::kPersonalContextFirstRunOptIn},
      /*disabled_features=*/{});

  PersonalContextEnablementServiceImplTestApi(&service())
      .ComputeEnablementState();

  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContext.name)))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContextWorkspace.name)))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContextPhotos.name)))
      .WillOnce(Return(false));

  service().OnAccountSettingDataUpdated(
      account_settings::kAccountSettingContext.name);
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEnablementState::kDisabledNeedsOptIn);
}

class PersonalContextEnablementServiceImplGeolocationTest
    : public PersonalContextEnablementServiceImplTest,
      public testing::WithParamInterface<
          std::tuple<std::string, PersonalContextEnablementState>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    PersonalContextEnablementServiceImplGeolocationTest,
    testing::Values(
        std::make_tuple(/*country_code=*/"au",
                        PersonalContextEnablementState::kDisabledNotEligible),
        std::make_tuple(/*country_code=*/"fr",
                        PersonalContextEnablementState::kDisabledNotEligible),
        std::make_tuple(/*country_code=*/"us",
                        PersonalContextEnablementState::kEnabled)));

// Verifies that the service is only enabled in supported geographical regions
// (e.g. "US" only).
TEST_P(PersonalContextEnablementServiceImplGeolocationTest,
       CheckCountryEnablement) {
  CreateService(std::get<0>(GetParam()));
  EXPECT_EQ(service().GetEnablementState(), std::get<1>(GetParam()));
}

class PersonalContextEnablementServiceImplLocaleTest
    : public PersonalContextEnablementServiceImplTest,
      public testing::WithParamInterface<
          std::tuple<std::string, PersonalContextEnablementState>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    PersonalContextEnablementServiceImplLocaleTest,
    testing::Values(
        std::make_tuple(/*locale=*/"fr-FR",
                        PersonalContextEnablementState::kDisabledNotEligible),
        std::make_tuple(/*locale=*/"de-DE",
                        PersonalContextEnablementState::kDisabledNotEligible),
        std::make_tuple(/*locale=*/"en-US",
                        PersonalContextEnablementState::kEnabled),
        std::make_tuple(/*locale=*/"en-GB",
                        PersonalContextEnablementState::kDisabledNotEligible)));

// Verifies that the service is only enabled for the en-US locale.
TEST_P(PersonalContextEnablementServiceImplLocaleTest, CheckLocaleEnablement) {
  CreateService("us", std::get<0>(GetParam()));
  EXPECT_EQ(service().GetEnablementState(), std::get<1>(GetParam()));
}

}  // namespace
}  // namespace personal_context
