// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include <gmock/gmock.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_profile_interaction_manager.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/browser/test_support.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

namespace {
using ::testing::_;

class MockFingerprintingProtectionPageActivationThrottle
    : public FingerprintingProtectionPageActivationThrottle {
 public:
  MOCK_METHOD(void,
              NotifyResult,
              (subresource_filter::ActivationDecision),
              (override));
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

class FakeProfileInteractionManager : public ProfileInteractionManager {
 public:
  FakeProfileInteractionManager()
      : ProfileInteractionManager(nullptr, nullptr) {}
  subresource_filter::mojom::ActivationLevel OnPageActivationComputed(
      content::NavigationHandle* handle,
      subresource_filter::mojom::ActivationLevel level,
      subresource_filter::ActivationDecision* decision) override {
    CHECK(handle->IsInMainFrame());
    if (allowlisted_hosts_.count(handle->GetURL().host())) {
      if (level == subresource_filter::mojom::ActivationLevel::kEnabled) {
        *decision = subresource_filter::ActivationDecision::URL_ALLOWLISTED;
      }
      return subresource_filter::mojom::ActivationLevel::kDisabled;
    }
    return level;
  }

  content_settings::SettingSource GetTrackingProtectionSettingSource(
      const GURL&) override {
    return content_settings::SettingSource::kUser;
  }

  void AllowlistInCurrentWebContents(const GURL& url) {
    ASSERT_TRUE(url.SchemeIsHTTPOrHTTPS());
    allowlisted_hosts_.insert(url.host());
  }

  void ClearAllowlist() { allowlisted_hosts_.clear(); }

 private:
  std::set<std::string> allowlisted_hosts_;
};

}  // namespace

class FingerprintingProtectionPageActivationThrottleTest
    : public content::RenderViewHostTestHarness {
 public:
  FingerprintingProtectionPageActivationThrottleTest() = default;

  FingerprintingProtectionPageActivationThrottleTest(
      const FingerprintingProtectionPageActivationThrottleTest&) = delete;
  FingerprintingProtectionPageActivationThrottleTest& operator=(
      const FingerprintingProtectionPageActivationThrottleTest&) = delete;

  ~FingerprintingProtectionPageActivationThrottleTest() override = default;

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

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       FlagDisabled_IsUnknown) {
  base::HistogramTester histograms;

  // Disable the feature.
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableFingerprintingProtectionFilter);

  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      /*dealer_handle_=*/nullptr, test_support_.tracking_protection_settings(),
      test_support_.prefs());

  // Expect that NotifyResult is called with UNKNOWN ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(subresource_filter::ActivationDecision::UNKNOWN))
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

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       FlagEnabledDefaultActivatedParams_IsActivated) {
  base::HistogramTester histograms;

  // Enable the feature with default params, i.e. activation_level = enabled.
  scoped_feature_list_.InitWithFeatures(
      {features::kEnableFingerprintingProtectionFilter}, {});

  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      /*dealer_handle_=*/nullptr, test_support_.tracking_protection_settings(),
      test_support_.prefs());

  // Expect NotifyResult is called with ACTIVATED ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(subresource_filter::ActivationDecision::ACTIVATED))
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

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       FlagEnabledWithDryRun_IsActivated) {
  base::HistogramTester histograms;

  // Enable the feature with dry_run params: activation_level = dry_run.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{features::kEnableFingerprintingProtectionFilter,
        {{"activation_level", "dry_run"}}}},
      {});

  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      /*dealer_handle_=*/nullptr, test_support_.tracking_protection_settings(),
      test_support_.prefs());

  // Expect that NotifyResult is called with ACTIVATED ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(subresource_filter::ActivationDecision::ACTIVATED))
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

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
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
      /*dealer_handle_=*/nullptr, test_support_.tracking_protection_settings(),
      test_support_.prefs());

  // Expect that NotifyResult is called with ACTIVATION_DISABLED
  // ActivationDecision.
  EXPECT_CALL(
      mock_throttle,
      NotifyResult(subresource_filter::ActivationDecision::ACTIVATION_DISABLED))
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

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       FlagEnabledDefaultActivatedParams_IsAllowlisted) {
  base::HistogramTester histograms;

  // Enable the feature with disabling params, i.e. activation_level = disabled.
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);

  // Initialize a real throttle to test histograms are emitted as expected.
  mock_nav_handle_->set_url(GURL("http://cool.things.com"));
  FakeProfileInteractionManager fake_delegate;
  fake_delegate.AllowlistInCurrentWebContents(GURL("http://cool.things.com"));
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      mock_nav_handle_.get(), test_support_.tracking_protection_settings(),
      test_support_.prefs());
  throttle.profile_interaction_manager_ =
      std::make_unique<FakeProfileInteractionManager>(fake_delegate);

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

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
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

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
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
    FingerprintingProtectionPageActivationThrottleTest,
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

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       FlagEnabled_MeasurePerformanceRate) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter,
      {{"performance_measurement_rate", "1.0"}});

  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      /*dealer_handle_=*/nullptr, test_support_.tracking_protection_settings(),
      test_support_.prefs());

  EXPECT_EQ(
      mock_throttle.GetEnablePerformanceMeasurements(/*is_incognito=*/false),
      true);
}

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       IncognitoFlagEnabled_MeasurePerformanceRate) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilterInIncognito,
      {{"performance_measurement_rate", "1.0"}});

  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      /*dealer_handle_=*/nullptr, test_support_.tracking_protection_settings(),
      test_support_.prefs());

  EXPECT_EQ(
      mock_throttle.GetEnablePerformanceMeasurements(/*is_incognito=*/true),
      true);
}

TEST_F(
    FingerprintingProtectionPageActivationThrottleTest,
    PerformancemanceMeasurementRateNotSet_NonIncognito_DoNotMeasurePerformance) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter,
      /*params*/ {});

  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      /*dealer_handle_=*/nullptr, test_support_.tracking_protection_settings(),
      test_support_.prefs());

  EXPECT_EQ(
      mock_throttle.GetEnablePerformanceMeasurements(/*is_incognito=*/false),
      false);
}

TEST_F(
    FingerprintingProtectionPageActivationThrottleTest,
    PerformancemanceMeasurementRateNotSet_Incognito_DoNotMeasurePerformance) {
  base::HistogramTester histograms;

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilterInIncognito,
      /*params*/ {});

  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      /*dealer_handle_=*/nullptr, test_support_.tracking_protection_settings(),
      test_support_.prefs());

  EXPECT_EQ(
      mock_throttle.GetEnablePerformanceMeasurements(/*is_incognito=*/true),
      false);
}

}  // namespace fingerprinting_protection_filter
