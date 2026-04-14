// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service_impl.h"

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/accessibility_annotator/core/accessibility_annotator_debug_features.h"
#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service_impl_test_api.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/prefs.h"
#include "components/account_settings/account_settings.h"
#include "components/account_settings/account_settings_features.h"
#include "components/account_settings/mock_account_setting_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {
namespace {

using testing::Return;

MATCHER_P(AccountSettingWithName, name, "") {
  return std::string(arg.name) == name;
}

class MockAccessibilityAnnotatorEnablementServiceObserver
    : public AccessibilityAnnotatorEnablementService::Observer {
 public:
  MOCK_METHOD(void,
              OnEnablementStateChanged,
              (RemoteAnnotatorEnablementState),
              (override));
};

class AccessibilityAnnotatorEnablementServiceImplTest : public testing::Test {
 public:
  AccessibilityAnnotatorEnablementServiceImplTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAccessibilityAnnotator,
                              features::kAccessibilityAnnotatorFirstRun},
        /*disabled_features=*/{});

    SetPrefs();
    CreateService("us");
    SignIn("test@gmail.com");
  }
  ~AccessibilityAnnotatorEnablementServiceImplTest() override = default;

 protected:
  void SignIn(const std::string& email,
              bool is_underaged = false,
              bool is_managed = false) {
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    AccountInfo::Builder builder(account_info);
    if (is_managed) {
      builder.SetHostedDomain("example.com");
    }
    AccountCapabilities capabilities = account_info.GetAccountCapabilities();
    AccountCapabilitiesTestMutator mutator(&capabilities);
    mutator.set_can_use_model_execution_features(!is_underaged);
    builder.UpdateAccountCapabilitiesWith(capabilities);
    identity_test_env_.UpdateAccountInfoForAccount(builder.Build());
  }

  void CreateService(const std::string& country_code) {
    service_ = std::make_unique<AccessibilityAnnotatorEnablementServiceImpl>(
        &mock_account_settings_service_, identity_test_env_.identity_manager(),
        subscription_eligibility_service_.get(), &pref_service_,
        GeoIpCountryCode(base::ToUpperASCII(country_code)));
  }

  void SetPrefs() {
    accessibility_annotator::prefs::RegisterProfilePrefs(
        pref_service_.registry());
    pref_service_.SetBoolean(
        accessibility_annotator::prefs::kShouldShowRemoteAnnotatorFirstRunInfo,
        false);
    subscription_eligibility::prefs::RegisterProfilePrefs(
        pref_service_.registry());
    pref_service_.SetInteger(
        subscription_eligibility::prefs::kAiSubscriptionTier, 1);
    subscription_eligibility_service_ = std::make_unique<
        subscription_eligibility::SubscriptionEligibilityService>(
        &pref_service_);

    // Enable all by default to satisfy requirements.
    ON_CALL(mock_account_settings_service_, GetBoolean(testing::_))
        .WillByDefault(Return(true));
  }

  AccessibilityAnnotatorEnablementServiceImpl& service() { return *service_; }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<subscription_eligibility::SubscriptionEligibilityService>
      subscription_eligibility_service_;
  testing::NiceMock<account_settings::MockAccountSettingService>
      mock_account_settings_service_;
  std::unique_ptr<AccessibilityAnnotatorEnablementServiceImpl> service_;
};

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest, ForcedEnablementState) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kAccessibilityAnnotatorForceEnablementState,
        {{"remote_annotator_enablement_state", "0"}});
    EXPECT_EQ(service().GetEnablementState(),
              RemoteAnnotatorEnablementState::kDisabledNotEligible);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kAccessibilityAnnotatorForceEnablementState,
        {{"remote_annotator_enablement_state", "1"}});
    EXPECT_EQ(service().GetEnablementState(),
              RemoteAnnotatorEnablementState::kDisabledPendingInfo);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kAccessibilityAnnotatorForceEnablementState,
        {{"remote_annotator_enablement_state", "2"}});
    EXPECT_EQ(service().GetEnablementState(),
              RemoteAnnotatorEnablementState::kDisabledPendingSetup);
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::debug::kAccessibilityAnnotatorForceEnablementState,
        {{"remote_annotator_enablement_state", "3"}});
    EXPECT_EQ(service().GetEnablementState(),
              RemoteAnnotatorEnablementState::kEnabled);
  }
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       DisabledWhenFeaturesAreOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAccessibilityAnnotator,
                             features::kAccessibilityAnnotatorFirstRun});

  test_api(&service()).RecomputeEnablementState();
  EXPECT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       DisabledWhenMainFeatureIsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAccessibilityAnnotatorFirstRun},
      /*disabled_features=*/{features::kAccessibilityAnnotator});

  test_api(&service()).RecomputeEnablementState();
  EXPECT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       EnabledWhenAllFeaturesAreOn) {
  EXPECT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kEnabled);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       DisabledPendingInfoWhenInfoNotAcknowledged) {
  pref_service_.SetBoolean(
      accessibility_annotator::prefs::kShouldShowRemoteAnnotatorFirstRunInfo,
      true);
  EXPECT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledPendingInfo);
}

#if !BUILDFLAG(IS_CHROMEOS)  // Signing out does not work on ChromeOS.
TEST_F(AccessibilityAnnotatorEnablementServiceImplTest, DisabledWhenSignedOut) {
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest, ClearsPrefOnSignout) {
  pref_service_.SetBoolean(prefs::kShouldShowRemoteAnnotatorFirstRunInfo,
                           false);
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_TRUE(
      pref_service_.GetBoolean(prefs::kShouldShowRemoteAnnotatorFirstRunInfo));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest, DisabledWhenUnderaged) {
  SignIn("under@gmail.com", /*is_underaged=*/true);

  EXPECT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest, DisabledWhenManaged) {
  SignIn("managed@example.com", /*is_underaged=*/false, /*is_managed=*/true);

  EXPECT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       DisabledWhenTierNotEligible) {
  pref_service_.SetInteger(subscription_eligibility::prefs::kAiSubscriptionTier,
                           3);

  EXPECT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       DisabledWhenAccountSettingsServiceNotAvailable) {
  service_ = std::make_unique<AccessibilityAnnotatorEnablementServiceImpl>(
      nullptr, identity_test_env_.identity_manager(),
      subscription_eligibility_service_.get(), &pref_service_,
      GeoIpCountryCode("US"));

  EXPECT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       DisabledWhenAccountOptedOutOfContext) {
  EXPECT_CALL(mock_account_settings_service_,
              GetBoolean(AccountSettingWithName(
                  account_settings::kAccountSettingContext.name)))
      .WillOnce(Return(false));

  service().OnAccountSettingDataUpdated(
      account_settings::kAccountSettingContext.name);
  EXPECT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
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
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
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
              RemoteAnnotatorEnablementState::kEnabled);
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
              RemoteAnnotatorEnablementState::kEnabled);
  }
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       ObserversNotifiedOnEnablementStateChanged) {
  MockAccessibilityAnnotatorEnablementServiceObserver observer;
  service().AddObserver(&observer);

  // Initial state is kEnabled.
  ASSERT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kEnabled);

  // Trigger a change to kDisabledPendingInfo by setting a pref.
  EXPECT_CALL(observer,
              OnEnablementStateChanged(
                  RemoteAnnotatorEnablementState::kDisabledPendingInfo));
  pref_service_.SetBoolean(
      accessibility_annotator::prefs::kShouldShowRemoteAnnotatorFirstRunInfo,
      true);

  // Trigger a change back to kEnabled.
  EXPECT_CALL(observer, OnEnablementStateChanged(
                            RemoteAnnotatorEnablementState::kEnabled));
  pref_service_.SetBoolean(
      accessibility_annotator::prefs::kShouldShowRemoteAnnotatorFirstRunInfo,
      false);

  service().RemoveObserver(&observer);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       CacheUpdatedOnAccountSettingChanged) {
  // Initial state is kEnabled.
  ASSERT_EQ(service().GetEnablementState(),
            RemoteAnnotatorEnablementState::kEnabled);

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
            RemoteAnnotatorEnablementState::kDisabledNotEligible);

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
            RemoteAnnotatorEnablementState::kEnabled);
}

class AccessibilityAnnotatorEnablementServiceImplGeolocationTest
    : public AccessibilityAnnotatorEnablementServiceImplTest,
      public testing::WithParamInterface<
          std::tuple<std::string, RemoteAnnotatorEnablementState>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    AccessibilityAnnotatorEnablementServiceImplGeolocationTest,
    testing::Values(
        std::make_tuple(/*country_code=*/"au",
                        RemoteAnnotatorEnablementState::kDisabledNotEligible),
        std::make_tuple(/*country_code=*/"fr",
                        RemoteAnnotatorEnablementState::kDisabledNotEligible),
        std::make_tuple(/*country_code=*/"us",
                        RemoteAnnotatorEnablementState::kEnabled)));

TEST_P(AccessibilityAnnotatorEnablementServiceImplGeolocationTest,
       CheckCountryEnablement) {
  CreateService(std::get<0>(GetParam()));
  EXPECT_EQ(service().GetEnablementState(), std::get<1>(GetParam()));
}

}  // namespace
}  // namespace accessibility_annotator
