// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_profile_interaction_manager.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/fingerprinting_protection_filter/browser/test_support.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
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
  bool is_on_3pc_blocked_enabled = false;
  content_settings::CookieControlsMode cookie_controls_mode =
      content_settings::CookieControlsMode::kBlockThirdParty;
  ActivationDecision initial_decision = ActivationDecision::ACTIVATED;
  ActivationLevel initial_level = ActivationLevel::kEnabled;
  ActivationLevel expected_level_output;
  ActivationDecision expected_decision;
};

class ProfileInteractionManagerTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<OnPageActivationComputedTestCase> {
 public:
  ProfileInteractionManagerTest() = default;

  GURL GetTestUrl() { return GURL("http://cool.things.com"); }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    mock_nav_handle_ = std::make_unique<content::MockNavigationHandle>(
        RenderViewHostTestHarness::web_contents());
  }

  void TearDown() override {
    mock_nav_handle_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  void SetFingerprintingProtectionFeatureEnabled(
      bool is_enabled,
      bool is_on_3pc_blocked_enabled) {
    if (is_enabled) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kEnableFingerprintingProtectionFilter,
            {{"enable_on_3pc_blocked",
              is_on_3pc_blocked_enabled ? "true" : "false"}}}},
          {});
    } else {
      EXPECT_FALSE(is_on_3pc_blocked_enabled);
      scoped_feature_list_.InitAndDisableFeature(
          features::kEnableFingerprintingProtectionFilter);
    }
  }

 protected:
  TestSupport test_support_;
  std::unique_ptr<content::MockNavigationHandle> mock_nav_handle_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

const OnPageActivationComputedTestCase kTestCases[] = {
    {
        .test_name = "FPFDisabled_NoException",
        .is_fp_feature_enabled = false,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
    },
    {
        .test_name = "FPFDisabled_Exception",
        .is_fp_feature_enabled = false,
        .site_has_tp_exception = true,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
    },
    {
        .test_name = "FPFEnabled_NoException_InitiallyDisabled",
        .initial_decision = ActivationDecision::ACTIVATION_DISABLED,
        .initial_level = ActivationLevel::kDisabled,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_DISABLED,
    },
    {
        .test_name = "FPFEnabled_NoException_UnknownConfig",
        .initial_decision = ActivationDecision::UNKNOWN,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::UNKNOWN,
    },
    {
        .test_name = "FPFEnabled_NoException_EnableOn3pcBlocked_3pcBlocked",
        .is_on_3pc_blocked_enabled = true,
        .expected_level_output = ActivationLevel::kEnabled,
        .expected_decision = ActivationDecision::ACTIVATED,
    },
    {
        .test_name = "FPFEnabled_NoException_EnableOn3pcBlocked_3pcAllowed",
        .is_on_3pc_blocked_enabled = true,
        .cookie_controls_mode = content_settings::CookieControlsMode::kOff,
        .expected_level_output = ActivationLevel::kDisabled,
        .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET,
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
  SetFingerprintingProtectionFeatureEnabled(
      test_case.is_fp_feature_enabled, test_case.is_on_3pc_blocked_enabled);
  // Navigate to the test url.
  mock_nav_handle_->set_url(GetTestUrl());
  test_support_.prefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(test_case.cookie_controls_mode));

  if (test_case.site_has_tp_exception) {
    test_support_.tracking_protection_settings()
        ->AddTrackingProtectionException(GetTestUrl(),
                                         /*is_user_bypass_exception=*/true);
  }
  // Prepare the manager under test and input with initial_decision param.
  auto test_manager = ProfileInteractionManager(
      test_support_.tracking_protection_settings(), test_support_.prefs());
  auto actual_decision = test_case.initial_decision;

  ActivationLevel actual_level = test_manager.OnPageActivationComputed(
      mock_nav_handle_.get(), test_case.initial_level, &actual_decision);

  EXPECT_EQ(actual_level, test_case.expected_level_output);
  EXPECT_EQ(actual_decision, test_case.expected_decision);
}

}  // namespace
}  // namespace fingerprinting_protection_filter
