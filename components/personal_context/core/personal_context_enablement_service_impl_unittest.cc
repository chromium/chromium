// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_enablement_service_impl.h"

#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
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
              (PersonalContextEligibilityState),
              (override));
};

class PersonalContextEnablementServiceImplTest : public testing::Test {
 public:
  PersonalContextEnablementServiceImplTest() {
    SetPrefs();
    CreateService("us");
    SignIn("test@gmail.com");
  }
  ~PersonalContextEnablementServiceImplTest() override = default;

 protected:
  void SignIn(const std::string& email,
              bool is_underaged = false,
              bool is_managed = false) {
    AccountInfo account_info = identity_test_env_.MakeAccountAvailable(email);
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
    identity_test_env_.SetPrimaryAccount(email, signin::ConsentLevel::kSignin);
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
        personal_context::prefs::
            kPersonalContextAmbientAutofillNoticeShouldBeShown,
        false);
    pref_service_.SetBoolean(
        personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
        true);
    // Enable all by default to satisfy requirements.
    ON_CALL(mock_account_settings_service_, GetBoolean(testing::_))
        .WillByDefault(Return(true));
  }

  PersonalContextEnablementServiceImpl& service() { return *service_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kPersonalContextLogNonEligibilityUma};
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  testing::NiceMock<account_settings::MockAccountSettingService>
      mock_account_settings_service_;
  base::HistogramTester histogram_tester_;
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
              PersonalContextEligibilityState::kDisabledNotEligible);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kPersonalContextForceEnablementState,
        {{"state", "1"}});
    EXPECT_EQ(service().GetEnablementState(),
              PersonalContextEligibilityState::kDisabledNeedsOptIn);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kPersonalContextForceEnablementState,
        {{"state", "2"}});
    EXPECT_EQ(service().GetEnablementState(),
              PersonalContextEligibilityState::kEligible);
  }
}

// Verifies that the service is enabled when all feature flags and other
// requirements (set up in the test fixture) are met.
TEST_F(PersonalContextEnablementServiceImplTest, EnabledWhenAllFeaturesAreOn) {
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEligibilityState::kEligible);
  histogram_tester().ExpectBucketCount(
      "Autofill.PersonalContext.NonEligibilityReason",
      PersonalContextNonEligibilityReason::kEligible, 1);
}

#if !BUILDFLAG(IS_CHROMEOS)  // Signing out does not work on ChromeOS.
// Verifies that the service is disabled when the user is not signed in.
TEST_F(PersonalContextEnablementServiceImplTest, DisabledWhenSignedOut) {
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEligibilityState::kDisabledNotEligible);

  histogram_tester().ExpectBucketCount(
      "Autofill.PersonalContext.NonEligibilityReason",
      PersonalContextNonEligibilityReason::kNotSignedIn, 2);
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

// Verifies that the service is disabled for underaged users.
TEST_F(PersonalContextEnablementServiceImplTest, DisabledWhenUnderaged) {
  SignIn("under@gmail.com", /*is_underaged=*/true);

  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEligibilityState::kDisabledNotEligible);

  histogram_tester().ExpectBucketCount(
      "Autofill.PersonalContext.NonEligibilityReason",
      PersonalContextNonEligibilityReason::kNotAgeEligible, 1);
}

// Verifies that the service is disabled for managed (enterprise) accounts.
TEST_F(PersonalContextEnablementServiceImplTest, DisabledWhenManaged) {
  SignIn("managed@example.com", /*is_underaged=*/false, /*is_managed=*/true);

  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEligibilityState::kDisabledNotEligible);

  histogram_tester().ExpectBucketCount(
      "Autofill.PersonalContext.NonEligibilityReason",
      PersonalContextNonEligibilityReason::kNotConsumerAccount, 1);
}

// Verifies that the service fails safe (disabled) if the required
// `AccountSettingService` is missing.
TEST_F(PersonalContextEnablementServiceImplTest,
       DisabledWhenAccountSettingsServiceNotAvailable) {
  service_ = std::make_unique<PersonalContextEnablementServiceImpl>(
      nullptr, identity_test_env_.identity_manager(), &pref_service_,
      GeoIpCountryCode("US"), "en-US");

  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEligibilityState::kDisabledNotEligible);
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
            PersonalContextEligibilityState::kDisabledNotEligible);

  histogram_tester().ExpectBucketCount(
      "Autofill.PersonalContext.NonEligibilityReason",
      PersonalContextNonEligibilityReason::kNotOptedInToContext, 1);
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
            PersonalContextEligibilityState::kDisabledNotEligible);

  histogram_tester().ExpectBucketCount(
      "Autofill.PersonalContext.NonEligibilityReason",
      PersonalContextNonEligibilityReason::kNotPhotosAndWorkspaceAvailable, 1);
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
              PersonalContextEligibilityState::kEligible);
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
              PersonalContextEligibilityState::kEligible);
  }
}

// Verifies that the internal state cache and observers are updated when
// account settings change.
TEST_F(PersonalContextEnablementServiceImplTest,
       CacheUpdatedOnAccountSettingChanged) {
  // Initial state is kEligible.
  ASSERT_EQ(service().GetEnablementState(),
            PersonalContextEligibilityState::kEligible);

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
            PersonalContextEligibilityState::kDisabledNotEligible);

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

  // The cache should be updated back to kEligible.
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEligibilityState::kEligible);
}

TEST_F(PersonalContextEnablementServiceImplTest,
       DisabledWhenAccountSettingsServiceNotAvailableAndOptInEnabled) {
  base::test::ScopedFeatureList feature_list{
      features::kPersonalContextFirstRunOptIn};

  service_ = std::make_unique<PersonalContextEnablementServiceImpl>(
      nullptr, identity_test_env_.identity_manager(), &pref_service_,
      GeoIpCountryCode("US"), "en-US");

  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEligibilityState::kDisabledNotEligible);
}

TEST_F(PersonalContextEnablementServiceImplTest,
       NeedsOptInWhenAccountOptedOutOfContextAndOptInEnabled) {
  base::test::ScopedFeatureList feature_list{
      features::kPersonalContextFirstRunOptIn};

  PersonalContextEnablementServiceImplTestApi(&service())
      .ComputeEnablementState();

  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContext.name)))
      .WillOnce(Return(false));

  service().OnAccountSettingDataUpdated(
      account_settings::kAccountSettingContext.name);
  EXPECT_EQ(service().GetEnablementState(),
            PersonalContextEligibilityState::kDisabledNeedsOptIn);
}

TEST_F(PersonalContextEnablementServiceImplTest,
       NeedsOptInWhenNoContextSourcesEnabledAndOptInEnabled) {
  base::test::ScopedFeatureList feature_list{
      features::kPersonalContextFirstRunOptIn};

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
            PersonalContextEligibilityState::kDisabledNeedsOptIn);
}

class PersonalContextEnablementServiceImplGeolocationTest
    : public PersonalContextEnablementServiceImplTest,
      public testing::WithParamInterface<
          std::tuple<std::string, PersonalContextEligibilityState>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    PersonalContextEnablementServiceImplGeolocationTest,
    testing::Values(
        std::make_tuple(/*country_code=*/"au",
                        PersonalContextEligibilityState::kDisabledNotEligible),
        std::make_tuple(/*country_code=*/"fr",
                        PersonalContextEligibilityState::kDisabledNotEligible),
        std::make_tuple(/*country_code=*/"us",
                        PersonalContextEligibilityState::kEligible)));

// Verifies that the service is only enabled in supported geographical regions
// (e.g. "US" only).
TEST_P(PersonalContextEnablementServiceImplGeolocationTest,
       CheckCountryEnablement) {
  CreateService(std::get<0>(GetParam()));
  EXPECT_EQ(service().GetEnablementState(), std::get<1>(GetParam()));

  if (std::get<1>(GetParam()) == PersonalContextEligibilityState::kEligible) {
    histogram_tester().ExpectBucketCount(
        "Autofill.PersonalContext.NonEligibilityReason",
        PersonalContextNonEligibilityReason::kEligible, 2);
  } else {
    histogram_tester().ExpectBucketCount(
        "Autofill.PersonalContext.NonEligibilityReason",
        PersonalContextNonEligibilityReason::kNotGeoIpUS, 1);
  }
}

class PersonalContextEnablementServiceImplLocaleTest
    : public PersonalContextEnablementServiceImplTest,
      public testing::WithParamInterface<
          std::tuple<std::string, PersonalContextEligibilityState>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    PersonalContextEnablementServiceImplLocaleTest,
    testing::Values(
        std::make_tuple(/*locale=*/"fr-FR",
                        PersonalContextEligibilityState::kDisabledNotEligible),
        std::make_tuple(/*locale=*/"de-DE",
                        PersonalContextEligibilityState::kDisabledNotEligible),
        std::make_tuple(/*locale=*/"en-US",
                        PersonalContextEligibilityState::kEligible),
        std::make_tuple(
            /*locale=*/"en-GB",
            PersonalContextEligibilityState::kDisabledNotEligible)));

// Verifies that the service is only enabled for the en-US locale.
TEST_P(PersonalContextEnablementServiceImplLocaleTest, CheckLocaleEnablement) {
  CreateService("us", std::get<0>(GetParam()));
  EXPECT_EQ(service().GetEnablementState(), std::get<1>(GetParam()));

  if (std::get<1>(GetParam()) == PersonalContextEligibilityState::kEligible) {
    histogram_tester().ExpectBucketCount(
        "Autofill.PersonalContext.NonEligibilityReason",
        PersonalContextNonEligibilityReason::kEligible, 2);
  } else {
    histogram_tester().ExpectBucketCount(
        "Autofill.PersonalContext.NonEligibilityReason",
        PersonalContextNonEligibilityReason::kNotLocaleEnUS, 1);
  }
}

}  // namespace
}  // namespace personal_context
