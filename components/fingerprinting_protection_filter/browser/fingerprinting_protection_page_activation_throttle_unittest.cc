// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"

#include <gmock/gmock.h>

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/subresource_filter/core/common/activation_decision.h"
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
  // Disable the feature.
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableFingerprintingProtectionFilter);

  // Initialize the WebContentsHelper and Throttle to be tested.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      navigation_handle()->GetWebContents());
  auto throttle_under_test = MockFingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

  // Expect that NotifyResult is called with UNKNOWN ActivationDecision.
  EXPECT_CALL(throttle_under_test,
              NotifyResult(subresource_filter::ActivationDecision::UNKNOWN))
      .WillOnce(testing::Return());
  EXPECT_EQ(throttle_under_test.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);
}

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       FlagEnabledDefaultActivatedParams_IsActivated) {
  // Enable the feature with default params, i.e. activation_level = enabled.
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableFingerprintingProtectionFilter);

  // Initialize the WebContentsHelper and Throttle to be tested.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      navigation_handle()->GetWebContents());
  auto throttle_under_test = MockFingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

  // Expect NotifyResult is called with ACTIVATED ActivationDecision.
  EXPECT_CALL(throttle_under_test,
              NotifyResult(subresource_filter::ActivationDecision::ACTIVATED))
      .WillOnce(testing::Return());
  EXPECT_EQ(throttle_under_test.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);
}

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       FlagEnabledWithDryRun_IsActivated) {
  // Enable the feature with dry_run params: activation_level = dry_run.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter,
      {{"activation_level", "dry_run"}});

  // Initialize the WebContentsHelper and Throttle to be tested.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      navigation_handle()->GetWebContents());
  auto throttle_under_test = MockFingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

  // Expect that NotifyResult is called with ACTIVATED ActivationDecision.
  EXPECT_CALL(throttle_under_test,
              NotifyResult(subresource_filter::ActivationDecision::ACTIVATED))
      .WillOnce(testing::Return());
  EXPECT_EQ(throttle_under_test.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);
}

TEST_F(FingerprintingProtectionPageActivationThrottleTest,
       FlagEnabledWithAllSitesDisabledParams_IsDisabled) {
  // Enable the feature with disabling params, i.e. activation_level = disabled.
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kEnableFingerprintingProtectionFilter,
      {{"activation_level", "disabled"}});

  // Initialize the WebContentsHelper and Throttle to be tested.
  FingerprintingProtectionWebContentsHelper::CreateForWebContents(
      navigation_handle()->GetWebContents());
  auto throttle_under_test = MockFingerprintingProtectionPageActivationThrottle(
      navigation_handle(), nullptr);

  // Expect that NotifyResult is called with ACTIVATION_DISABLED
  // ActivationDecision.
  EXPECT_CALL(
      throttle_under_test,
      NotifyResult(subresource_filter::ActivationDecision::ACTIVATION_DISABLED))
      .WillOnce(testing::Return());
  EXPECT_EQ(throttle_under_test.WillProcessResponse().action(),
            content::NavigationThrottle::ThrottleAction::PROCEED);
}

}  // namespace fingerprinting_protection_filter
