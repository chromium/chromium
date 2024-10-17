// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include <gmock/gmock.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/browser/test_support.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom-shared.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
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
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestSupport test_support_;
  std::unique_ptr<content::MockNavigationHandle> mock_nav_handle_;
};

MATCHER_P(WithActivationDecision,
          decision,
          "Matches an object `obj` such that `obj.decision == "
          "decision`") {
  return arg.decision == decision;
}

TEST_F(FPFPageActivationThrottleTest, FlagDisabled_IsUnknown) {
  base::HistogramTester histograms;

  // Disable the feature.
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableFingerprintingProtectionFilter);

  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  // Expect that NotifyResult is called with UNKNOWN ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(WithActivationDecision(
                  subresource_filter::ActivationDecision::UNKNOWN)))
      .WillOnce(testing::Return());
  EXPECT_EQ(mock_throttle.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);

  // Initialize a real throttle to test histograms are emitted as expected.
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  throttle.WillProcessResponse();

  // Expect no histograms are emitted when the feature flag is disabled.
  histograms.ExpectTotalCount(ActivationDecisionHistogramName, 0);
  histograms.ExpectTotalCount(ActivationLevelHistogramName, 0);
}

TEST_F(FPFPageActivationThrottleTest,
       FlagEnabledDefaultActivatedParams_IsActivated) {
  base::HistogramTester histograms;

  // Enable the feature with default params, i.e. activation_level = enabled.
  scoped_feature_list_.InitWithFeatures(
      {features::kEnableFingerprintingProtectionFilter}, {});

  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  // Expect NotifyResult is called with ACTIVATED ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(WithActivationDecision(
                  subresource_filter::ActivationDecision::ACTIVATED)))
      .WillOnce(testing::Return());
  EXPECT_EQ(mock_throttle.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);

  // Initialize a real throttle to test histograms are emitted as expected.
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  throttle.WillProcessResponse();

  histograms.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 1);
  histograms.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kEnabled, 1);
}

TEST_F(FPFPageActivationThrottleTest, FlagEnabledWithDryRun_IsActivated) {
  base::HistogramTester histograms;

  // Enable the feature with dry_run params: activation_level = dry_run.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{features::kEnableFingerprintingProtectionFilter,
        {{"activation_level", "dry_run"}}}},
      {});

  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  // Expect that NotifyResult is called with ACTIVATED ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(WithActivationDecision(
                  subresource_filter::ActivationDecision::ACTIVATED)))
      .WillOnce(testing::Return());
  EXPECT_EQ(mock_throttle.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);

  // Initialize a real throttle to test histograms are emitted as expected.
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  throttle.WillProcessResponse();

  histograms.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 1);
  histograms.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDryRun, 1);
}

TEST_F(FPFPageActivationThrottleTest,
       FlagEnabledWithAllSitesDisabledParams_IsDisabled) {
  base::HistogramTester histograms;

  // Enable the feature with disabling params, i.e. activation_level = disabled.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{features::kEnableFingerprintingProtectionFilter,
        {{"activation_level", "disabled"}}}},
      {});

  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  // Expect that NotifyResult is called with ACTIVATION_DISABLED
  // ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(WithActivationDecision(
                  subresource_filter::ActivationDecision::ACTIVATION_DISABLED)))
      .WillOnce(testing::Return());
  EXPECT_EQ(mock_throttle.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);

  // Initialize a real throttle to test histograms are emitted as expected.
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  throttle.WillProcessResponse();

  // Expect that NotifyResult is called with ACTIVATION_DISABLED
  // ActivationDecision.
  histograms.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATION_DISABLED, 1);
  histograms.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);
}

TEST_F(FPFPageActivationThrottleTest,
       FlagEnabledDefaultActivatedParams_IsAllowlisted) {
  base::HistogramTester histograms;

  // Enable the feature with disabling params, i.e. activation_level = disabled.
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);

  // Initialize a real throttle to test histograms are emitted as expected.
  mock_nav_handle_->set_url(GURL("http://cool.things.com"));
  test_support_.tracking_protection_settings()->AddTrackingProtectionException(
      GURL("http://cool.things.com"));
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  throttle.WillProcessResponse();

  histograms.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::URL_ALLOWLISTED, 1);
  histograms.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);
}

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
          mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
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
          mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
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

TEST_F(
    FPFPageActivationThrottleTest,
    LoggingParamDisabledNonIncognito_DoesntPassEnableLoggingInActivationState) {
  // Enable non-incognito feature without `enable_console_logging` param.
  scoped_feature_list_.InitAndEnableFeature(
      {features::kEnableFingerprintingProtectionFilter});

  // Use a mock throttle to mock NotifyPageActivationComputed
  auto mock_throttle =
      MockActivationThrottleMockingNotifyPageActivationComputed(
          mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
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

TEST_F(FPFPageActivationThrottleTest, FlagEnabled_MeasurePerformanceRate) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter,
      {{"performance_measurement_rate", "1.0"}});

  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  EXPECT_EQ(
      mock_throttle.GetEnablePerformanceMeasurements(/*is_incognito=*/false),
      true);
}

TEST_F(FPFPageActivationThrottleTest,
       IncognitoFlagEnabled_MeasurePerformanceRate) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilterInIncognito,
      {{"performance_measurement_rate", "1.0"}});

  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  EXPECT_EQ(
      mock_throttle.GetEnablePerformanceMeasurements(/*is_incognito=*/true),
      true);
}

TEST_F(
    FPFPageActivationThrottleTest,
    PerformancemanceMeasurementRateNotSet_NonIncognito_DoNotMeasurePerformance) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter,
      /*params*/ {});

  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  EXPECT_EQ(
      mock_throttle.GetEnablePerformanceMeasurements(/*is_incognito=*/false),
      false);
}

TEST_F(
    FPFPageActivationThrottleTest,
    PerformancemanceMeasurementRateNotSet_Incognito_DoNotMeasurePerformance) {
  base::HistogramTester histograms;

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilterInIncognito,
      /*params*/ {});

  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());

  EXPECT_EQ(
      mock_throttle.GetEnablePerformanceMeasurements(/*is_incognito=*/true),
      false);
}

struct FPFGetActivationTestCase {
  std::string test_name;

  // Configuration
  bool is_fp_feature_enabled;
  ActivationLevel activation_level_param;
  bool site_has_tp_exception;
  bool only_if_3pc_blocked_param;
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
      ActivationLevel activation_level,
      bool only_if_3pc_blocked) {
    if (is_enabled) {
      std::string activation_level_param;
      if (activation_level == ActivationLevel::kEnabled) {
        activation_level_param = "enabled";
      }
      if (activation_level == ActivationLevel::kDisabled) {
        activation_level_param = "disabled";
      }
      if (activation_level == ActivationLevel::kDryRun) {
        activation_level_param = "dry_run";
      }
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kEnableFingerprintingProtectionFilter,
            {{"enable_only_if_3pc_blocked",
              only_if_3pc_blocked ? "true" : "false"},
             {"activation_level", activation_level_param}}}},
          {});
    } else {
      EXPECT_FALSE(only_if_3pc_blocked);
      scoped_feature_list_.InitAndDisableFeature(
          features::kEnableFingerprintingProtectionFilter);
    }
  }

 protected:
  TestSupport test_support_;
  std::unique_ptr<content::MockNavigationHandle> mock_nav_handle_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

const FPFGetActivationTestCase kTestCases[] = {
    {.test_name = "FPFEnabled_ActivationEnabled_NoException_NotOnlyIf3pc",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .site_has_tp_exception = false,
     .only_if_3pc_blocked_param = false,

     .expected_level = ActivationLevel::kEnabled,
     .expected_decision = ActivationDecision::ACTIVATED},
    {.test_name =
         "FPFEnabled_ActivationEnabled_NoException_OnlyIf3pc_3pcBlocked",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .site_has_tp_exception = false,
     .only_if_3pc_blocked_param = true,
     .cookie_controls_mode =
         content_settings::CookieControlsMode::kBlockThirdParty,

     .expected_level = ActivationLevel::kEnabled,
     .expected_decision = ActivationDecision::ACTIVATED},
    {.test_name =
         "FPFEnabled_ActivationEnabled_NoException_OnlyIf3pc_3pcNotBlocked",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .site_has_tp_exception = false,
     .only_if_3pc_blocked_param = true,
     .cookie_controls_mode = content_settings::CookieControlsMode::kOff,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET},
    {.test_name = "FPFEnabled_ActivationEnabled_Exception_NotOnlyIf3pc",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .site_has_tp_exception = true,
     .only_if_3pc_blocked_param = false,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::URL_ALLOWLISTED},
    {.test_name = "FPFEnabled_ActivationEnabled_Exception_OnlyIf3pc_3pcBlocked",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .site_has_tp_exception = true,
     .only_if_3pc_blocked_param = true,
     .cookie_controls_mode =
         content_settings::CookieControlsMode::kBlockThirdParty,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::URL_ALLOWLISTED},
    {.test_name =
         "FPFEnabled_ActivationEnabled_Exception_OnlyIf3pc_3pcNotBlocked",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .site_has_tp_exception = true,
     .only_if_3pc_blocked_param = true,
     .cookie_controls_mode = content_settings::CookieControlsMode::kOff,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET},
    // Not testing all permutations with FPF disabled because the expected
    // return
    // value is the same.
    {.test_name = "FPFDisabled_NoException",
     .is_fp_feature_enabled = false,
     .activation_level_param = ActivationLevel::kDisabled,
     .site_has_tp_exception = false,
     .only_if_3pc_blocked_param = false,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::UNKNOWN},
    {.test_name = "FPFDisabled_Exception",
     .is_fp_feature_enabled = false,
     .activation_level_param = ActivationLevel::kDisabled,
     .site_has_tp_exception = true,
     .only_if_3pc_blocked_param = false,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::UNKNOWN},
    // Not testing all permutations with dry_run because the expected return
    // value is the same.
    {.test_name = "FPFEnabled_ActivationDryRun_NoException",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kDryRun,
     .site_has_tp_exception = false,
     .only_if_3pc_blocked_param = false,

     .expected_level = ActivationLevel::kDryRun,
     .expected_decision = ActivationDecision::ACTIVATED},
    {.test_name = "FPFEnabled_ActivationDryRun_Exception",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kDryRun,
     .site_has_tp_exception = true,
     .only_if_3pc_blocked_param = false,

     .expected_level = ActivationLevel::kDryRun,
     .expected_decision = ActivationDecision::ACTIVATED},
};

INSTANTIATE_TEST_SUITE_P(
    FPFPageActivationThrottleTestGetActivationTestTestSuiteInstantiation,
    FPFPageActivationThrottleTestGetActivationTest,
    testing::ValuesIn<FPFGetActivationTestCase>(kTestCases),
    [](const testing::TestParamInfo<
        FPFPageActivationThrottleTestGetActivationTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(FPFPageActivationThrottleTestGetActivationTest,
       GetActivationComputesLevelAndDecision) {
  const FPFGetActivationTestCase& test_case = GetParam();

  // Initialize whether the feature is enabled.
  SetFingerprintingProtectionFeatureEnabled(
      test_case.is_fp_feature_enabled, test_case.activation_level_param,
      test_case.only_if_3pc_blocked_param);
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
  auto test_throttle = FingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());
  GetActivationResult activation = test_throttle.GetActivation();

  EXPECT_EQ(activation.level, test_case.expected_level);
  EXPECT_EQ(activation.decision, test_case.expected_decision);
}

}  // namespace fingerprinting_protection_filter
