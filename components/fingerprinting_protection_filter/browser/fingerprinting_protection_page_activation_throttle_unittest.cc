// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include <gmock/gmock.h>

#include <memory>

#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "base/test/metrics/histogram_tester.h"
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
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

  // Expect that NotifyResult is called with UNKNOWN ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(WithActivationDecision(
                  subresource_filter::ActivationDecision::UNKNOWN)))
      .WillOnce(testing::Return());
  EXPECT_EQ(mock_throttle.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);

  // Initialize a real throttle to test histograms are emitted as expected.
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

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
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

  // Expect NotifyResult is called with ACTIVATED ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(WithActivationDecision(
                  subresource_filter::ActivationDecision::ACTIVATED)))
      .WillOnce(testing::Return());
  EXPECT_EQ(mock_throttle.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);

  // Initialize a real throttle to test histograms are emitted as expected.
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

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
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

  // Expect that NotifyResult is called with ACTIVATED ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(WithActivationDecision(
                  subresource_filter::ActivationDecision::ACTIVATED)))
      .WillOnce(testing::Return());
  EXPECT_EQ(mock_throttle.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);

  // Initialize a real throttle to test histograms are emitted as expected.
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

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
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

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
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

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
       DefaultActivatedParams_TrackingProtectionException_IsAllowlisted) {
  base::HistogramTester histograms;
  ukm::InitializeSourceUrlRecorderForWebContents(
      mock_nav_handle_->GetWebContents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Enable the feature with disabling params, i.e. activation_level = disabled.
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);

  // Initialize a real throttle to test histograms are emitted as expected.
  mock_nav_handle_->set_url(GURL("http://cool.things.com"));

  test_support_.tracking_protection_settings()->AddTrackingProtectionException(
      GURL("http://cool.things.com"));

  auto throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

  throttle.WillProcessResponse();

  histograms.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::URL_ALLOWLISTED, 1);
  histograms.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);

  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::FingerprintingProtectionException::kEntryName);
  EXPECT_EQ(1u, test_ukm_recorder.entries_count());
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::FingerprintingProtectionException::kSourceName,
      static_cast<int64_t>(ExceptionSource::USER_BYPASS));
}

TEST_F(FPFPageActivationThrottleTest,
       DefaultActivatedParams_CookieException_IsAllowlisted) {
  base::HistogramTester histograms;
  ukm::InitializeSourceUrlRecorderForWebContents(
      mock_nav_handle_->GetWebContents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  scoped_feature_list_.InitWithFeatures(
      {features::kEnableFingerprintingProtectionFilter},
      {privacy_sandbox::kActUserBypassUx});

  // Initialize a real throttle to test histograms are emitted as expected.
  mock_nav_handle_->set_url(GURL("http://cool.things.com"));

  test_support_.content_settings()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURL(GURL("http://cool.things.com")),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);

  auto throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

  throttle.WillProcessResponse();

  histograms.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::URL_ALLOWLISTED, 1);
  histograms.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::FingerprintingProtectionException::kEntryName);
  EXPECT_EQ(1u, test_ukm_recorder.entries_count());
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::FingerprintingProtectionException::kSourceName,
      static_cast<int64_t>(ExceptionSource::COOKIES));
}

TEST_F(
    FPFPageActivationThrottleTest,
    DefaultActivatedParams_CookieException_UserBypassEnabled_IsNotAllowlisted) {
  base::HistogramTester histograms;
  scoped_feature_list_.InitWithFeatures(
      {features::kEnableFingerprintingProtectionFilter,
       privacy_sandbox::kActUserBypassUx},
      {});

  mock_nav_handle_->set_url(GURL("http://cool.things.com"));

  test_support_.content_settings()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURL(GURL("http://cool.things.com")),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);

  auto throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());

  throttle.WillProcessResponse();

  histograms.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 1);
  histograms.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kEnabled, 1);
}

TEST_F(FPFPageActivationThrottleTest,
       IncognitoFlagEnabledDefaultParams_CookieException_IsAllowlisted) {
  base::HistogramTester histograms;
  ukm::InitializeSourceUrlRecorderForWebContents(
      mock_nav_handle_->GetWebContents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilterInIncognito,
      /*params*/ {});

  // Initialize a real throttle to test histograms are emitted as expected.
  mock_nav_handle_->set_url(GURL("http://cool.things.com"));

  test_support_.content_settings()->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromURL(GURL("http://cool.things.com")),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);

  auto throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs(),
      /*is_incognito=*/true);

  throttle.WillProcessResponse();

  histograms.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::URL_ALLOWLISTED, 1);
  histograms.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDisabled, 1);
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::FingerprintingProtectionException::kEntryName);
  EXPECT_EQ(1u, test_ukm_recorder.entries_count());
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::FingerprintingProtectionException::kSourceName,
      static_cast<int64_t>(ExceptionSource::COOKIES));
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
          *mock_nav_registry_, test_support_.content_settings(),
          test_support_.tracking_protection_settings(), test_support_.prefs());

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
          test_support_.tracking_protection_settings(), test_support_.prefs(),
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
          test_support_.tracking_protection_settings(), test_support_.prefs());

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
        // FingerprintingProtectionUx flag isn't used together with
        // `EnableFingerprintingProtectionFilter(InIncognito)`.
        {privacy_sandbox::kFingerprintingProtectionUx},
        /*disabled_features=*/{});
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
         .expected_decision = ActivationDecision::ACTIVATION_DISABLED},
        {.test_name = "TPSettingDisabled_Incognito_Disabled",
         .is_incognito = true,
         .tps_fp_setting_enabled = false,
         .expected_level = ActivationLevel::kDisabled,
         .expected_decision = ActivationDecision::ACTIVATION_DISABLED},
        {.test_name = "TPSettingDisabled_NonIncognito_Disabled",
         .is_incognito = false,
         .tps_fp_setting_enabled = false,
         .expected_level = ActivationLevel::kDisabled,
         .expected_decision = ActivationDecision::ACTIVATION_DISABLED}};

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

  // Create TrackingProtectionSettings with specified incognito mode.
  auto tracking_protection_settings =
      std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
          test_support_.prefs(), test_support_.content_settings(),
          /*management_service=*/nullptr, test_case.is_incognito);

  // Create ActivationThrottle with the TrackingProtectionSettings, and
  // specified incognito mode.
  auto test_throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      tracking_protection_settings.get(), test_support_.prefs(),
      test_case.is_incognito);

  GetActivationResult activation = test_throttle.GetActivation();
  EXPECT_EQ(activation.level, test_case.expected_level);
  EXPECT_EQ(activation.decision, test_case.expected_decision);
}

struct FPFRefreshHeuristicUmaTestCase {
  std::string test_name;

  // Configuration
  bool is_refresh_heuristic_enabled;
  bool has_refresh_heuristic_exception;

  // Expectations
  bool expect_has_exception_uma;
  bool expect_latency_uma;
};

class FPFPageActivationThrottleTestRefreshHeuristicUmaTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<FPFRefreshHeuristicUmaTestCase> {
 public:
  FPFPageActivationThrottleTestRefreshHeuristicUmaTest() = default;

  GURL GetTestUrl() { return GURL("http://cool.things.com"); }

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

  void InitializeFeatureFlag(const FPFRefreshHeuristicUmaTestCase& test_case) {
    static constexpr std::string kRefreshHeuristicThreshold = "3";
    static constexpr std::string kRefreshHeuristicThresholdDisabled = "0";
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kEnableFingerprintingProtectionFilter,
          {{"activation_level", "enabled"},
           {"performance_measurement_rate", "1.0"},
           {features::kRefreshHeuristicExceptionThresholdParam,
            test_case.is_refresh_heuristic_enabled
                ? kRefreshHeuristicThreshold
                : kRefreshHeuristicThresholdDisabled}}}},
        {});
  }

 protected:
  TestSupport test_support_;
  std::unique_ptr<content::MockNavigationHandle> mock_nav_handle_;
  std::unique_ptr<content::MockNavigationThrottleRegistry> mock_nav_registry_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

const FPFRefreshHeuristicUmaTestCase kRefreshHeuristicUmaTestCases[] = {
    {.test_name = "RefreshHeuristicEnabled_HasException_LogsUmas",
     .is_refresh_heuristic_enabled = true,
     .has_refresh_heuristic_exception = true,
     .expect_has_exception_uma = true,
     .expect_latency_uma = true},
    {.test_name = "RefreshHeuristicEnabled_NoException_LogsLatencyUma",
     .is_refresh_heuristic_enabled = true,
     .has_refresh_heuristic_exception = false,
     .expect_has_exception_uma = false,
     .expect_latency_uma = true},
    {.test_name = "RefreshHeuristicDisabled_HasException_DoesntLogUmas",
     .is_refresh_heuristic_enabled = false,
     .has_refresh_heuristic_exception = true,
     .expect_has_exception_uma = false,
     .expect_latency_uma = false},
    {.test_name = "RefreshHeuristicDisabled_NoException_DoesntLogUmas",
     .is_refresh_heuristic_enabled = false,
     .has_refresh_heuristic_exception = false,
     .expect_has_exception_uma = false,
     .expect_latency_uma = false}};

INSTANTIATE_TEST_SUITE_P(
    FPFPageActivationThrottleTestRefreshHeuristicUmaTestTestSuiteInstantiation,
    FPFPageActivationThrottleTestRefreshHeuristicUmaTest,
    testing::ValuesIn<FPFRefreshHeuristicUmaTestCase>(
        kRefreshHeuristicUmaTestCases),
    [](const testing::TestParamInfo<
        FPFPageActivationThrottleTestRefreshHeuristicUmaTest::ParamType>&
           info) { return info.param.test_name; });

TEST_P(FPFPageActivationThrottleTestRefreshHeuristicUmaTest,
       RefreshHeuristicUmasAreLoggedCorrectly) {
  base::HistogramTester histograms;
  ukm::InitializeSourceUrlRecorderForWebContents(
      mock_nav_handle_->GetWebContents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  const FPFRefreshHeuristicUmaTestCase& test_case = GetParam();

  // Initialize feature flags and params.
  InitializeFeatureFlag(test_case);

  // Add exception
  if (test_case.has_refresh_heuristic_exception) {
    AddBreakageException(GURL(GetTestUrl()), *test_support_.prefs());
  }

  // Navigate to the test url.
  mock_nav_handle_->set_url(GetTestUrl());

  // Call `GetActivation` on throttle to trigger UMAs.
  auto test_throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());
  test_throttle.GetActivation();

  if (test_case.expect_has_exception_uma) {
    histograms.ExpectTotalCount(HasRefreshCountExceptionHistogramName, 1);
    histograms.ExpectUniqueSample(HasRefreshCountExceptionHistogramName, 1, 1);
    const auto& entries = test_ukm_recorder.GetEntriesByName(
        ukm::builders::FingerprintingProtectionException::kEntryName);
    EXPECT_EQ(1u, test_ukm_recorder.entries_count());
    test_ukm_recorder.ExpectEntryMetric(
        entries[0],
        ukm::builders::FingerprintingProtectionException::kSourceName,
        static_cast<int64_t>(ExceptionSource::REFRESH_HEURISTIC));
  } else {
    histograms.ExpectTotalCount(HasRefreshCountExceptionHistogramName, 0);
  }

  if (test_case.expect_latency_uma) {
    // Just test whether a latency has been logged at all - we don't need to
    // test the latency measurement itself, since that's done entirely by the
    // subresource filter library macro (UMA_HISTOGRAM_CUSTOM_MICRO_TIMES).
    histograms.ExpectTotalCount(
        HasRefreshCountExceptionWallDurationHistogramName, 1);
  } else {
    histograms.ExpectTotalCount(
        HasRefreshCountExceptionWallDurationHistogramName, 0);
  }
}

struct FPFGetActivationTestCase {
  std::string test_name;

  // Configuration
  bool is_fp_feature_enabled;
  ActivationLevel activation_level_param;
  bool is_refresh_heuristic_breakage_exception_enabled;
  bool site_has_tp_exception;
  bool site_has_refresh_heuristic_breakage_exception;
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
    {.test_name =
         "FPFEnabled_ActivationEnabled_BreakageExceptionsDisabled_HasException",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .is_refresh_heuristic_breakage_exception_enabled = false,
     .site_has_tp_exception = false,
     .site_has_refresh_heuristic_breakage_exception = true,
     .only_if_3pc_blocked_param = false,
     .cookie_controls_mode = content_settings::CookieControlsMode::kOff,

     .expected_level = ActivationLevel::kEnabled,
     .expected_decision = ActivationDecision::ACTIVATED},
    {.test_name =
         "FPFEnabled_ActivationEnabled_BreakageExceptionsEnabled_HasException",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .is_refresh_heuristic_breakage_exception_enabled = true,
     .site_has_tp_exception = false,
     .site_has_refresh_heuristic_breakage_exception = true,
     .only_if_3pc_blocked_param = false,
     .cookie_controls_mode = content_settings::CookieControlsMode::kOff,

     .expected_level = ActivationLevel::kDisabled,
     .expected_decision = ActivationDecision::URL_ALLOWLISTED},
    {.test_name = "FPFEnabled_ActivationEnabled_BreakageExceptionsEnabled_"
                  "HasNoException",
     .is_fp_feature_enabled = true,
     .activation_level_param = ActivationLevel::kEnabled,
     .is_refresh_heuristic_breakage_exception_enabled = true,
     .site_has_tp_exception = false,
     .site_has_refresh_heuristic_breakage_exception = false,
     .only_if_3pc_blocked_param = false,
     .cookie_controls_mode = content_settings::CookieControlsMode::kOff,

     .expected_level = ActivationLevel::kEnabled,
     .expected_decision = ActivationDecision::ACTIVATED},
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

  if (test_case.site_has_tp_exception) {
    test_support_.tracking_protection_settings()
        ->AddTrackingProtectionException(GetTestUrl());
  }

  // Navigate to the test url.
  mock_nav_handle_->set_url(GetTestUrl());

  // Prepare the manager under test and input with initial_decision param.
  auto test_throttle = FingerprintingProtectionPageActivationThrottle(
      *mock_nav_registry_, test_support_.content_settings(),
      test_support_.tracking_protection_settings(), test_support_.prefs());
  GetActivationResult activation = test_throttle.GetActivation();

  EXPECT_EQ(activation.level, test_case.expected_level);
  EXPECT_EQ(activation.decision, test_case.expected_decision);
}

// Filepath is not found on Android devices, checking on desktop should be
// sufficient.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(FPFPageActivationThrottleTest, ExceptionSourceHistograms) {
  std::optional<base::HistogramEnumEntryMap> sources;
  std::vector<std::string> missing_sources;
  {
    sources =
        base::ReadEnumFromEnumsXml("FingerprintingProtectionExceptionSource");
    ASSERT_TRUE(sources.has_value());
  }
  for (int i = 0; i <= static_cast<int>(ExceptionSource::EXCEPTION_SOURCE_MAX);
       ++i) {
    if (!sources->contains(i)) {
      missing_sources.push_back(base::NumberToString(i));
    }
  }
  ASSERT_TRUE(missing_sources.empty())
      << "Exception sources: " << base::JoinString(missing_sources, ", ")
      << " configured in fingerprinting_protection_page_activation_throttle.h "
         "but no corresponding enum values were added to "
         "FingerprintingProtectionExceptionSource enum in "
         "tools/metrics/histograms/enums.xml.";
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace fingerprinting_protection_filter
