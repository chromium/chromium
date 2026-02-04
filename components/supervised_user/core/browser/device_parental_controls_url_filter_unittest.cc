// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/device_parental_controls_url_filter.h"

#include "base/test/bind.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/supervised_user_synthetic_field_trial_service_delegate.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

class DeviceParentalControlsTestImpl : public DeviceParentalControls {
 public:
  DeviceParentalControlsTestImpl() = default;
  ~DeviceParentalControlsTestImpl() override = default;

  // Reasonable default for these tests.
  bool IsIncognitoModeDisabled() const override {
    return is_web_filtering_enabled_ || is_safe_search_enabled_;
  }
  bool IsSafeSearchForced() const override { return is_safe_search_enabled_; }
  bool IsWebFilteringEnabled() const override {
    return is_web_filtering_enabled_;
  }
  bool IsEnabled() const override {
    return is_safe_search_enabled_ || is_web_filtering_enabled_;
  }

  void SetWebFilteringEnabled(bool enabled) {
    is_web_filtering_enabled_ = enabled;
  }

  void SetSafeSearchForced(bool enabled) { is_safe_search_enabled_ = enabled; }

  void RegisterDeviceLevelSyntheticFieldTrials(
      SynteticFieldTrialDelegate& synthetic_field_trial_delegate)
      const override {}

 private:
  bool is_web_filtering_enabled_ = false;
  bool is_safe_search_enabled_ = false;
};

class DeviceParentalControlsUrlFilterTest : public testing::Test {
 protected:
  DeviceParentalControlsTestImpl& device_parental_controls() {
    return device_parental_controls_;
  }

  DeviceParentalControlsUrlFilter& under_test() { return url_filter_; }

  MockUrlCheckerClient& url_checker_client() { return url_checker_client_; }

 private:
  MockUrlCheckerClient url_checker_client_;
  DeviceParentalControlsTestImpl device_parental_controls_;
  DeviceParentalControlsUrlFilter url_filter_{
      device_parental_controls_,
      std::make_unique<UrlCheckerClientWrapper>(url_checker_client_)};
};

TEST_F(DeviceParentalControlsUrlFilterTest, WebFilterTypeIsDisabled) {
  ASSERT_FALSE(device_parental_controls().IsEnabled());
  EXPECT_EQ(WebFilterType::kDisabled, under_test().GetWebFilterType());
}

TEST_F(DeviceParentalControlsUrlFilterTest,
       WebFilterTypeAllowsAllWhenWebFilteringIsDisabled) {
  device_parental_controls().SetSafeSearchForced(true);

  EXPECT_TRUE(device_parental_controls().IsEnabled());
  EXPECT_FALSE(device_parental_controls().IsWebFilteringEnabled());
  EXPECT_EQ(WebFilterType::kAllowAllSites, under_test().GetWebFilterType());
}

TEST_F(DeviceParentalControlsUrlFilterTest,
       WebFilterIsInTryToBlockMatureSitesMode) {
  device_parental_controls().SetWebFilteringEnabled(true);

  EXPECT_TRUE(device_parental_controls().IsEnabled());
  EXPECT_TRUE(device_parental_controls().IsWebFilteringEnabled());
  EXPECT_EQ(WebFilterType::kTryToBlockMatureSites,
            under_test().GetWebFilterType());
}

TEST_F(DeviceParentalControlsUrlFilterTest,
       SyncCheckReturnsAllowWhenFilterIsDisabled) {
  ASSERT_FALSE(device_parental_controls().IsEnabled());
  ASSERT_FALSE(device_parental_controls().IsWebFilteringEnabled());

  WebFilteringResult result =
      under_test().GetFilteringBehavior(GURL("http://example.com"));
  EXPECT_EQ(FilteringBehavior::kAllow, result.behavior);
  EXPECT_EQ(FilteringBehaviorReason::FILTER_DISABLED, result.reason);
}

TEST_F(DeviceParentalControlsUrlFilterTest,
       SyncCheckReturnsAllowWhenWebFilteringIsDisabled) {
  device_parental_controls().SetSafeSearchForced(true);

  EXPECT_TRUE(device_parental_controls().IsEnabled());
  EXPECT_FALSE(device_parental_controls().IsWebFilteringEnabled());

  WebFilteringResult result =
      under_test().GetFilteringBehavior(GURL("http://example.com"));
  EXPECT_EQ(FilteringBehavior::kAllow, result.behavior);
  EXPECT_EQ(FilteringBehaviorReason::DEFAULT, result.reason);
}

enum class CheckType {
  kMainFrame,
  kSubFrame,
};

// For main frame and sub frame async checks, both filters behave exactly the
// same.
class DeviceParentalControlsUrlFilterAsyncTest
    : public DeviceParentalControlsUrlFilterTest,
      public testing::WithParamInterface<CheckType> {
 protected:
  void GetFilteringBehavior(std::string_view url,
                            WebFilteringResult::Callback callback) {
    switch (GetParam()) {
      case CheckType::kMainFrame:
        under_test().GetFilteringBehavior(
            GURL(url), /*skip_manual_parent_filter=*/false, std::move(callback),
            WebFilterMetricsOptions());
        break;
      case CheckType::kSubFrame:
        under_test().GetFilteringBehaviorForSubFrame(
            GURL(url), GURL("http://mainframe.example.com"),
            std::move(callback), WebFilterMetricsOptions());
        break;
    }
  }
};

TEST_P(DeviceParentalControlsUrlFilterAsyncTest,
       AsyncCheckReturnsAllowWhenFilterIsDisabled) {
  ASSERT_FALSE(device_parental_controls().IsEnabled());
  ASSERT_FALSE(device_parental_controls().IsWebFilteringEnabled());

  GetFilteringBehavior(
      "http://example.com",
      base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_EQ(FilteringBehavior::kAllow, result.behavior);
        EXPECT_EQ(FilteringBehaviorReason::FILTER_DISABLED, result.reason);
      }));
}

TEST_P(DeviceParentalControlsUrlFilterAsyncTest,
       AsyncCheckClassifiesAccordingToCheckerBehavior) {
  device_parental_controls().SetWebFilteringEnabled(true);

  EXPECT_TRUE(device_parental_controls().IsEnabled());
  EXPECT_TRUE(device_parental_controls().IsWebFilteringEnabled());

  // First unique url will be allowed, second will be blocked.
  url_checker_client().ScheduleResolution(
      safe_search_api::ClientClassification::kAllowed);
  url_checker_client().ScheduleResolution(
      safe_search_api::ClientClassification::kRestricted);

  std::string example_url("http://example.com");
  GetFilteringBehavior(
      example_url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_EQ(FilteringBehavior::kAllow, result.behavior);
        EXPECT_EQ(FilteringBehaviorReason::ASYNC_CHECKER, result.reason);
      }));
  GetFilteringBehavior(
      example_url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_EQ(FilteringBehavior::kAllow, result.behavior);
        EXPECT_EQ(FilteringBehaviorReason::ASYNC_CHECKER, result.reason);
      }));

  std::string another_url("http://another.com");
  ASSERT_NE(example_url, another_url);

  GetFilteringBehavior(
      another_url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_EQ(FilteringBehavior::kBlock, result.behavior);
        EXPECT_EQ(FilteringBehaviorReason::ASYNC_CHECKER, result.reason);
      }));
}

INSTANTIATE_TEST_SUITE_P(,
                         DeviceParentalControlsUrlFilterAsyncTest,
                         testing::Values(CheckType::kMainFrame,
                                         CheckType::kSubFrame),
                         [](const testing::TestParamInfo<CheckType>& info) {
                           switch (info.param) {
                             case CheckType::kMainFrame:
                               return std::string("MainFrame");
                             case CheckType::kSubFrame:
                               return std::string("SubFrame");
                           }
                         });

}  // namespace
}  // namespace supervised_user
