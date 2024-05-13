// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include <gmock/gmock.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom-shared.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fingerprinting_protection_filter {

namespace {

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
    content::RenderViewHostTestHarness::SetUp();
    auto* contents = RenderViewHostTestHarness::web_contents();
    mock_nav_handle_ =
        std::make_unique<content::MockNavigationHandle>(contents);
  }

  void TearDown() override {
    scoped_feature_list_.Reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  content::MockNavigationHandle* navigation_handle() {
    return mock_nav_handle_.get();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<content::MockNavigationHandle> mock_nav_handle_;
};

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       FlagDisabled_IsUnknown) {
  base::HistogramTester histograms;
  // Disable the feature.
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableFingerprintingProtectionFilter);

  // Initialize the WebContentsHelper and Throttle to be tested.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      navigation_handle()->GetWebContents(),
      /*tracking_protection_settings=*/nullptr);
  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

  // Expect that NotifyResult is called with UNKNOWN ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(subresource_filter::ActivationDecision::UNKNOWN))
      .WillOnce(testing::Return());
  EXPECT_EQ(mock_throttle.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);

  // Initialize a real throttle to test histograms are emitted as expected.
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

  throttle.WillProcessResponse();

  // Expect no histograms are emitted when the feature flag is disabled.
  histograms.ExpectTotalCount(ActivationDecisionHistogramName, 0);
  histograms.ExpectTotalCount(ActivationLevelHistogramName, 0);
}

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       FlagEnabledDefaultActivatedParams_IsActivated) {
  base::HistogramTester histograms;
  // Enable the feature with default params, i.e. activation_level = enabled.
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);

  // Initialize the WebContentsHelper and Throttle to be tested.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      navigation_handle()->GetWebContents(),
      /*tracking_protection_settings=*/nullptr);
  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

  // Expect NotifyResult is called with ACTIVATED ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(subresource_filter::ActivationDecision::ACTIVATED))
      .WillOnce(testing::Return());
  EXPECT_EQ(mock_throttle.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);

  // Initialize a real throttle to test histograms are emitted as expected.
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

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
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter,
      {{"activation_level", "dry_run"}});

  // Initialize the WebContentsHelper and Throttle to be tested.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      navigation_handle()->GetWebContents(),
      /*tracking_protection_settings=*/nullptr);
  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

  // Expect that NotifyResult is called with ACTIVATED ActivationDecision.
  EXPECT_CALL(mock_throttle,
              NotifyResult(subresource_filter::ActivationDecision::ACTIVATED))
      .WillOnce(testing::Return());
  EXPECT_EQ(mock_throttle.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);

  // Initialize a real throttle to test histograms are emitted as expected.
  auto throttle = FingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

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
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter,
      {{"activation_level", "disabled"}});

  // Initialize the WebContentsHelper.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      navigation_handle()->GetWebContents(),
      /*tracking_protection_settings=*/nullptr);
  // Use a mock throttle to test GetActivationDecision() by making EXPECT_CALL
  // on public function.
  auto mock_throttle = MockFingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

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
      navigation_handle(), nullptr);

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

}  // namespace fingerprinting_protection_filter
