// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include <gmock/gmock.h>

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/browser/test_support.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_breakage_exception.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom-shared.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

namespace {
using ::subresource_filter::ActivationDecision;
using ::subresource_filter::mojom::ActivationLevel;
using ::testing::_;

class MockFingerprintingProtectionPageActivationThrottle
    : public FingerprintingProtectionPageActivationThrottle {
 public:
  MOCK_METHOD(void, NotifyResult, (GetActivationResult), (override));
  using FingerprintingProtectionPageActivationThrottle::
      FingerprintingProtectionPageActivationThrottle;
  using FingerprintingProtectionPageActivationThrottle::WillProcessResponse;
};

class MockActivationThrottleMockingNotifyPageActivationComputed
    : public FingerprintingProtectionPageActivationThrottle {
 public:
  MOCK_METHOD(void,
              NotifyPageActivationComputed,
              (subresource_filter::mojom::ActivationState,
               subresource_filter::ActivationDecision),
              (override));
  using FingerprintingProtectionPageActivationThrottle::
      FingerprintingProtectionPageActivationThrottle;
  using FingerprintingProtectionPageActivationThrottle::WillProcessResponse;
};

}  // namespace

class FPFPageActivationThrottleTest
    : public content::RenderViewHostTestHarness {
 public:
  FPFPageActivationThrottleTest() = default;

  FPFPageActivationThrottleTest(const FPFPageActivationThrottleTest&) = delete;
  FPFPageActivationThrottleTest& operator=(
      const FPFPageActivationThrottleTest&) = delete;

  ~FPFPageActivationThrottleTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    mock_nav_handle_ = std::make_unique<content::MockNavigationHandle>(
        RenderViewHostTestHarness::web_contents());
    mock_nav_registry_ =
        std::make_unique<content::MockNavigationThrottleRegistry>(
            mock_nav_handle_.get(),
            content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestSupport test_support_;
  std::unique_ptr<content::MockNavigationHandle> mock_nav_handle_;
  std::unique_ptr<content::MockNavigationThrottleRegistry> mock_nav_registry_;
};

MATCHER_P(HasEnableLogging,
          enable_logging,
          "Matches an object `obj` such that `obj.enable_logging == "
          "enable_logging`") {
  return arg.enable_logging == enable_logging;
}

TEST_F(FPFPageActivationThrottleTest,
       LoggingParamEnabledNonIncognito_PassesEnableLoggingInActivationState) {
  // Enable non-incognito feature with `enable_console_logging` param.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      {features::kEnableFingerprintingProtectionFilter},
      {{"enable_console_logging", "true"}});

  // Use a mock throttle to mock NotifyPageActivationComputed
  auto mock_throttle =
      MockActivationThrottleMockingNotifyPageActivationComputed(
          *mock_nav_registry_, test_support_.content_settings(),
          test_support_.prefs());

  // Expect that NotifyPageActivationComputed is called with an ActivationState
  // with enable_logging == true.
  EXPECT_CALL(mock_throttle,
              NotifyPageActivationComputed(HasEnableLogging(true), _))
      .Times(1);

  // Make call to `WillProcessResponse`, which leads to
  // `NotifyPageActivationComputed`.
  mock_throttle.WillProcessResponse();
}

TEST_F(FPFPageActivationThrottleTest,
       LoggingParamEnabledIncognito_PassesEnableLoggingInActivationState) {
  // Enable incognito feature with `enable_console_logging` param.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      {features::kEnableFingerprintingProtectionFilterInIncognito},
      {{"enable_console_logging", "true"}});

  // Use a mock throttle to mock NotifyPageActivationComputed
  auto mock_throttle =
      MockActivationThrottleMockingNotifyPageActivationComputed(
          *mock_nav_registry_, test_support_.content_settings(),
          test_support_.prefs(),
          /*is_incognito=*/true);

  // Expect that NotifyPageActivationComputed is called with an ActivationState
  // with enable_logging == true.
  EXPECT_CALL(mock_throttle,
              NotifyPageActivationComputed(HasEnableLogging(true), _))
      .Times(1);

  // Make call to `WillProcessResponse`, which leads to
  // `NotifyPageActivationComputed`.
  mock_throttle.WillProcessResponse();
}

TEST_F(
    FPFPageActivationThrottleTest,
    LoggingParamDisabledNonIncognito_DoesntPassEnableLoggingInActivationState) {
  // Enable non-incognito feature without `enable_console_logging` param.
  scoped_feature_list_.InitAndEnableFeature(
      {features::kEnableFingerprintingProtectionFilter});

  // Use a mock throttle to mock NotifyPageActivationComputed
  auto mock_throttle =
      MockActivationThrottleMockingNotifyPageActivationComputed(
          *mock_nav_registry_, test_support_.content_settings(),
          test_support_.prefs());

  // Expect that NotifyPageActivationComputed is called with an ActivationState
  // with enable_logging == false.
  EXPECT_CALL(mock_throttle,
              NotifyPageActivationComputed(HasEnableLogging(false), _))
      .Times(1);

  // Make call to `WillProcessResponse`, which leads to
  // `NotifyPageActivationComputed`.
  mock_throttle.WillProcessResponse();
}

struct FPFGetActivationWithTrackingProtectionSettingTestCase {
  std::string test_name;

  // Configuration
  bool is_incognito;
  bool tps_fp_setting_enabled;

  // Expectations
  ActivationLevel expected_level;
  ActivationDecision expected_decision;
};

class FPFPageActivationThrottleWithTrackingProtectionSettingTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<
          FPFGetActivationWithTrackingProtectionSettingTestCase> {
 public:
  FPFPageActivationThrottleWithTrackingProtectionSettingTest() = default;

  FPFPageActivationThrottleWithTrackingProtectionSettingTest(
      const FPFPageActivationThrottleWithTrackingProtectionSettingTest&) =
      delete;
  FPFPageActivationThrottleWithTrackingProtectionSettingTest& operator=(
      const FPFPageActivationThrottleWithTrackingProtectionSettingTest&) =
      delete;

  ~FPFPageActivationThrottleWithTrackingProtectionSettingTest() override =
      default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    mock_nav_handle_ = std::make_unique<content::MockNavigationHandle>(
        RenderViewHostTestHarness::web_contents());
    mock_nav_registry_ =
        std::make_unique<content::MockNavigationThrottleRegistry>(
            mock_nav_handle_.get(),
            content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
    scoped_feature_list_.InitWithFeatures(
        // The FingerprintingProtectionUx flag is used together with
        // `EnableFingerprintingProtectionFilterInIncognito` to enable the FPF
        // in incognito.
        {privacy_sandbox::kFingerprintingProtectionUx,
         features::kEnableFingerprintingProtectionFilterInIncognito},
        /*disabled_features=*/{
            features::kEnableFingerprintingProtectionFilter});
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestSupport test_support_;
  std::unique_ptr<content::MockNavigationHandle> mock_nav_handle_;
  std::unique_ptr<content::MockNavigationThrottleRegistry> mock_nav_registry_;
};

const FPFGetActivationWithTrackingProtectionSettingTestCase
    kGetActivationWithTrackingProtectionSettingTestCases[] = {
        {.test_name = "TPSettingEnabled_Incognito_Enabled",
         .is_incognito = true,
         .tps_fp_setting_enabled = true,
         .expected_level = ActivationLevel::kEnabled,
         .expected_decision = ActivationDecision::ACTIVATED},
        {.test_name = "TPSettingEnabled_NonIncognito_Disabled",
         .is_incognito = false,
         .tps_fp_setting_enabled = true,
         .expected_level = ActivationLevel::kDisabled,
         .expected_decision = ActivationDecision::UNKNOWN},
        {.test_name = "TPSettingDisabled_Incognito_Disabled",
         .is_incognito = true,
         .tps_fp_setting_enabled = false,
         .expected_level = ActivationLevel::kDisabled,
         .expected_decision = ActivationDecision::ACTIVATION_DISABLED},
        {.test_name = "TPSettingDisabled_NonIncognito_Disabled",
         .is_incognito = false,
         .tps_fp_setting_enabled = false,
         .expected_level = ActivationLevel::kDisabled,
         .expected_decision = ActivationDecision::UNKNOWN}};

INSTANTIATE_TEST_SUITE_P(
    FPFPageActivationThrottleWithTrackingProtectionSettingTestSuiteInstantiation,
    FPFPageActivationThrottleWithTrackingProtectionSettingTest,
    testing::ValuesIn<FPFGetActivationWithTrackingProtectionSettingTestCase>(
        kGetActivationWithTrackingProtectionSettingTestCases),
    [](const testing::TestParamInfo<
        FPFPageActivationThrottleWithTrackingProtectionSettingTest::ParamType>&
           info) { return info.param.test_name; });

TEST_P(FPFPageActivationThrottleWithTrackingProtectionSettingTest,
       GetActivationComputesLevelAndDecision) {
  const FPFGetActivationWithTrackingProtectionSettingTestCase& test_case =
      GetParam();

  // Set FPP in Tracking Protection settings.
  test_support_.prefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled,
                                    test_case.tps_fp_setting_enabled);

  // Create ActivationThrottle.
  auto test_throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.prefs(), test_case.is_incognito);

  GetActivationResult activation = test_throttle.GetActivation();
  EXPECT_EQ(activation.level, test_case.expected_level);
  EXPECT_EQ(activation.decision, test_case.expected_decision);
}

struct FPFGetActivationTestCase {
  std::string test_name;

  // Configuration
  bool is_fp_feature_enabled;
  ActivationLevel activation_level_param;
  bool is_refresh_heuristic_breakage_exception_enabled;
  bool site_has_refresh_heuristic_breakage_exception;
  bool only_if_3pc_blocked_param;
  bool is_localhost;
  content_settings::CookieControlsMode cookie_controls_mode =
      content_settings::CookieControlsMode::kBlockThirdParty;

  // Expectations
  ActivationLevel expected_level;
  ActivationDecision expected_decision;
};

class FPFPageActivationThrottleTestGetActivationTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<FPFGetActivationTestCase> {
 public:
  FPFPageActivationThrottleTestGetActivationTest() = default;

  GURL GetTestUrl() { return GURL("http://cool.things.com"); }
  GURL GetLocalhostUrl() { return GURL("http://localhost:8000"); }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    mock_nav_handle_ = std::make_unique<content::MockNavigationHandle>(
        RenderViewHostTestHarness::web_contents());
    mock_nav_registry_ =
        std::make_unique<content::MockNavigationThrottleRegistry>(
            mock_nav_handle_.get(),
            content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  }

  void TearDown() override {
    mock_nav_registry_.reset();
    mock_nav_handle_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  void SetFingerprintingProtectionFeatureEnabled(
      const FPFGetActivationTestCase& test_case) {
    if (test_case.is_fp_feature_enabled) {
      std::string activation_level_param;
      if (test_case.activation_level_param == ActivationLevel::kEnabled) {
        activation_level_param = "enabled";
      }
      if (test_case.activation_level_param == ActivationLevel::kDisabled) {
        activation_level_param = "disabled";
      }
      if (test_case.activation_level_param == ActivationLevel::kDryRun) {
        activation_level_param = "dry_run";
      }
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kEnableFingerprintingProtectionFilter,
            {{"enable_only_if_3pc_blocked",
              base::ToString(test_case.only_if_3pc_blocked_param)},
             {"activation_level", activation_level_param},
             {features::kRefreshHeuristicExceptionThresholdParam,
              test_case.is_refresh_heuristic_breakage_exception_enabled
                  ? "3"
                  : "0"}}}},
          {});
    } else {
      EXPECT_FALSE(test_case.only_if_3pc_blocked_param);
      scoped_feature_list_.InitAndDisableFeature(
          features::kEnableFingerprintingProtectionFilter);
    }
  }

 protected:
  TestSupport test_support_;
  std::unique_ptr<content::MockNavigationHandle> mock_nav_handle_;
  std::unique_ptr<content::MockNavigationThrottleRegistry> mock_nav_registry_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

const FPFGetActivationTestCase kGetActivationTestCases[] = {
    {.test_name = "FPFEnabled_ActivationEnabled_NoException_NotOnlyIf3pc",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .only_if_3pc_blocked_param = false,
     .is_localhost = false,

     .expected_level = ActivationLevel::kEnabled,
     .expected_decision = ActivationDecision::ACTIVATED},
    {.test_name =
         "FPFEnabled_ActivationEnabled_NoException_OnlyIf3pc_3pcBlocked",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .only_if_3pc_blocked_param = true,
     .is_localhost = false,
     .cookie_controls_mode =
         content_settings::CookieControlsMode::kBlockThirdParty,

     .expected_level = ActivationLevel::kEnabled,
     .expected_decision = ActivationDecision::ACTIVATED},
    {.test_name = "FPFEnabled_ActivationEnabled_OnlyIf3pc_3pcNotBlocked",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .only_if_3pc_blocked_param = true,
     .is_localhost = false,
     .cookie_controls_mode = content_settings::CookieControlsMode::kOff,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET},
    {.test_name =
         "FPFEnabled_ActivationEnabled_BreakageExceptionsDisabled_HasException",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .is_refresh_heuristic_breakage_exception_enabled = false,
     .site_has_refresh_heuristic_breakage_exception = true,
     .only_if_3pc_blocked_param = false,
     .is_localhost = false,
     .cookie_controls_mode = content_settings::CookieControlsMode::kOff,

     .expected_level = ActivationLevel::kEnabled,
     .expected_decision = ActivationDecision::ACTIVATED},
    {.test_name =
         "FPFEnabled_ActivationEnabled_BreakageExceptionsEnabled_HasException",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .is_refresh_heuristic_breakage_exception_enabled = true,
     .site_has_refresh_heuristic_breakage_exception = true,
     .only_if_3pc_blocked_param = false,
     .is_localhost = false,
     .cookie_controls_mode = content_settings::CookieControlsMode::kOff,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::URL_ALLOWLISTED},
    {.test_name = "FPFEnabled_ActivationEnabled_BreakageExceptionsEnabled_"
                  "HasNoException",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .is_refresh_heuristic_breakage_exception_enabled = true,
     .site_has_refresh_heuristic_breakage_exception = false,
     .only_if_3pc_blocked_param = false,
     .is_localhost = false,
     .cookie_controls_mode = content_settings::CookieControlsMode::kOff,

     .expected_level = ActivationLevel::kEnabled,
     .expected_decision = ActivationDecision::ACTIVATED},
    // Not testing all permutations with FPF disabled because the expected
    // return value is the same.
    {.test_name = "FPFDisabled",
     .is_fp_feature_enabled = false,
     .activation_level_param = ActivationLevel::kDisabled,
     .only_if_3pc_blocked_param = false,
     .is_localhost = false,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::UNKNOWN},
    {.test_name = "FPFDisabled_Localhost",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .only_if_3pc_blocked_param = false,
     .is_localhost = true,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET},
    // Not testing all permutations with dry_run because the expected return
    // value is the same.
    {.test_name = "FPFEnabled_ActivationDryRun_NoException",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kDryRun,
     .only_if_3pc_blocked_param = false,
     .is_localhost = false,

     .expected_level = ActivationLevel::kDryRun,
     .expected_decision = ActivationDecision::ACTIVATED},
    {.test_name = "FPFEnabled_ActivationDryRun_Exception",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kDryRun,
     .only_if_3pc_blocked_param = false,
     .is_localhost = false,

     .expected_level = ActivationLevel::kDryRun,
     .expected_decision = ActivationDecision::ACTIVATED},
};

INSTANTIATE_TEST_SUITE_P(
    FPFPageActivationThrottleTestGetActivationTestTestSuiteInstantiation,
    FPFPageActivationThrottleTestGetActivationTest,
    testing::ValuesIn<FPFGetActivationTestCase>(kGetActivationTestCases),
    [](const testing::TestParamInfo<
        FPFPageActivationThrottleTestGetActivationTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(FPFPageActivationThrottleTestGetActivationTest,
       GetActivationComputesLevelAndDecision) {
  const FPFGetActivationTestCase& test_case = GetParam();

  // Initialize feature flags and params.
  SetFingerprintingProtectionFeatureEnabled(test_case);

  // Set cookie controls mode
  test_support_.prefs()->SetInteger(
      ::prefs::kCookieControlsMode,
      static_cast<int>(test_case.cookie_controls_mode));

  // Add exceptions
  if (test_case.site_has_refresh_heuristic_breakage_exception) {
    AddBreakageException(GURL(GetTestUrl()), *test_support_.prefs());
  }

  // Navigate to the test url, use localhost url when testing localhost.
  mock_nav_handle_->set_url(test_case.is_localhost ? GetLocalhostUrl()
                                                   : GetTestUrl());

  // Prepare the manager under test and input with initial_decision param.
  auto test_throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.prefs());
  GetActivationResult activation = test_throttle.GetActivation();

  EXPECT_EQ(activation.level, test_case.expected_level);
  EXPECT_EQ(activation.decision, test_case.expected_decision);
}

}  // namespace fingerprinting_protection_filter
