// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_profile_interaction_manager.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fingerprinting_protection_filter {
namespace {

using ::subresource_filter::ActivationDecision;
using ::subresource_filter::mojom::ActivationLevel;
using ::testing::TestWithParam;

struct OnPageActivationComputedTestCase {
  std::string test_name;
  bool is_fp_feature_enabled = true;
  bool is_fp_user_pref_enabled = true;
  bool site_has_tp_exception = false;
  ActivationDecision initial_decision = ActivationDecision::ACTIVATED;
  ActivationLevel initial_level = ActivationLevel::kEnabled;
  ActivationLevel expected_level_output;
  ActivationDecision expected_decision;
};

class ProfileInteractionManagerTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<OnPageActivationComputedTestCase> {
 public:
  ProfileInteractionManagerTest() {
    HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
    privacy_sandbox::tracking_protection::RegisterProfilePrefs(
        prefs()->registry());
  }

  GURL GetTestUrl() { return GURL("http://cool.things.com"); }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        prefs(), /*is_off_the_record=*/false, /*store_last_modified=*/false,
        /*restore_session=*/false, /*should_record_metrics=*/false);
    tracking_protection_settings_ =
        std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
            prefs(), host_content_settings_map_.get(),
            /*onboarding_service=*/nullptr, /*is_incognito=*/false);

    auto* contents = RenderViewHostTestHarness::web_contents();
    mock_nav_handle_ =
        std::make_unique<content::MockNavigationHandle>(contents);
  }

  void TearDown() override {
    host_content_settings_map_->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();
    RenderViewHostTestHarness::TearDown();
  }

  void SetFingerprintingProtectionSettingEnabled(bool is_enabled) {
    if (is_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          privacy_sandbox::kFingerprintingProtectionSetting);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          privacy_sandbox::kFingerprintingProtectionSetting);
    }
  }

  privacy_sandbox::TrackingProtectionSettings* tracking_protection() {
    return tracking_protection_settings_.get();
  }

  content::MockNavigationHandle* navigation_handle() {
    return mock_nav_handle_.get();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
  std::unique_ptr<content::MockNavigationHandle> mock_nav_handle_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

const OnPageActivationComputedTestCase kTestCases[] = {
    {
        .test_name = "FPFEnabled_UserOptIn_NoException",
        .expected_level_output = ActivationLevel::kEnabled,
        .expected_decision = ActivationDecision::ACTIVATED,
    },
    {
        .test_name = "FPFEnabled_UserOptIn_Exception",
        .site_has_tp_exception = true,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::URL_ALLOWLISTED,
    },
    {
        .test_name = "FPFEnabled_UserOptOut_NoException",
        .is_fp_user_pref_enabled = false,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
    },
    {
        .test_name = "FPFEnabled_UserOptOut_Exception",
        .is_fp_user_pref_enabled = false,
        .site_has_tp_exception = true,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
    },
    {
        .test_name = "FPFDisabled_UserOptOut_NoException",
        .is_fp_feature_enabled = false,
        .is_fp_user_pref_enabled = false,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
    },
    {
        .test_name = "FPFDisabled_UserOptIn_NoException",
        .is_fp_feature_enabled = false,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
    },
    {
        .test_name = "FPFDisabled_UserOptIn_Exception",
        .is_fp_feature_enabled = false,
        .site_has_tp_exception = true,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
    },
    {
        .test_name = "FPFDisabled_UserOptOut_Exception",
        .is_fp_feature_enabled = false,
        .is_fp_user_pref_enabled = false,
        .site_has_tp_exception = true,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
    },
    {
        .test_name = "FPFEnabled_UserOptIn_NoException_InitiallyDisabled",
        .initial_decision = ActivationDecision::ACTIVATION_DISABLED,
        .initial_level = ActivationLevel::kDisabled,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_DISABLED,
    },
    {
        .test_name = "FPFEnabled_UserOptIn_NoException_UnknownConfig",
        .initial_decision = ActivationDecision::UNKNOWN,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::UNKNOWN,
    }};

INSTANTIATE_TEST_SUITE_P(
    ProfileInteractionManagerTestSuiteInstantiation,
    ProfileInteractionManagerTest,
    testing::ValuesIn<OnPageActivationComputedTestCase>(kTestCases),
    [](const testing::TestParamInfo<ProfileInteractionManagerTest::ParamType>&
           info) { return info.param.test_name; });

TEST_P(ProfileInteractionManagerTest,
       OnPageActivationComputesLevelAndDecision) {
  const OnPageActivationComputedTestCase& test_case = GetParam();

  // Initialize whether the feature is enabled.
  SetFingerprintingProtectionSettingEnabled(test_case.is_fp_feature_enabled);
  // Navigate to the test url.
  navigation_handle()->set_url(GetTestUrl());
  // Initialize the tracking_protection_settings_ for test.
  prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled,
                      test_case.is_fp_user_pref_enabled);
  if (test_case.site_has_tp_exception) {
    tracking_protection()->AddTrackingProtectionException(
        GetTestUrl(), /*is_user_bypass_exception=*/true);
  }
  // Prepare the manager under test and input with initial_decision param.
  auto test_manager = ProfileInteractionManager(tracking_protection());
  auto actual_decision = test_case.initial_decision;

  ActivationLevel actual_level = test_manager.OnPageActivationComputed(
      navigation_handle(), test_case.initial_level, &actual_decision);

  EXPECT_EQ(actual_level, test_case.expected_level_output);
  EXPECT_EQ(actual_decision, test_case.expected_decision);
}

}  // namespace
}  // namespace fingerprinting_protection_filter
