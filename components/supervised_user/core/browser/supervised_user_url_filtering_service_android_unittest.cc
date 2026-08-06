// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

using ::testing::_;

class SupervisedUserUrlFilteringServiceTestBase : public testing::Test {
 protected:
  void TearDown() override { test_environment_.Shutdown(); }

  SupervisedUserTestEnvironment& test_environment() {
    return test_environment_;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_;
  SupervisedUserTestEnvironment test_environment_;
};

struct WebFilterTypeTestParams {
  std::string test_name;
  WebFilterType family_link;
  WebFilterType device;
  WebFilterType result;
};

class SupervisedUserUrlFilteringServiceWebFilterTypeAndroidTest
    : public testing::WithParamInterface<WebFilterTypeTestParams>,
      public SupervisedUserUrlFilteringServiceTestBase {};

TEST_P(SupervisedUserUrlFilteringServiceWebFilterTypeAndroidTest,
       WebFilterTypeTest) {
  if (GetParam().family_link != WebFilterType::kDisabled) {
    EnableParentalControls(*test_environment().pref_service());
    test_environment().SetWebFilterType(GetParam().family_link);
  }

  AndroidParentalControls& parental_controls =
      test_environment().device_parental_controls();
  switch (GetParam().device) {
    case WebFilterType::kTryToBlockMatureSites:
      parental_controls.SetSearchContentFiltersEnabledForTesting(false);
      parental_controls.SetBrowserContentFiltersEnabledForTesting(true);
      break;
    case WebFilterType::kAllowAllSites:
      parental_controls.SetSearchContentFiltersEnabledForTesting(true);
      parental_controls.SetBrowserContentFiltersEnabledForTesting(false);
      break;
    case WebFilterType::kDisabled:
      parental_controls.SetSearchContentFiltersEnabledForTesting(false);
      parental_controls.SetBrowserContentFiltersEnabledForTesting(false);
      break;
    default:
      NOTREACHED() << "Not supported by Android parental controls.";
  }

  EXPECT_EQ(GetParam().result,
            test_environment().url_filtering_service()->GetWebFilterType());
}

const WebFilterTypeTestParams kWebFilterTypeTestParams[] = {
    // If any of the settings is kTryToBlockMatureSites, the result is
    // kTryToBlockMatureSites.
    {.test_name = "TryToBlockMatureSites_1",
     .family_link = WebFilterType::kTryToBlockMatureSites,
     .device = WebFilterType::kTryToBlockMatureSites,
     .result = WebFilterType::kTryToBlockMatureSites},
    {.test_name = "TryToBlockMatureSites_2",
     .family_link = WebFilterType::kTryToBlockMatureSites,
     .device = WebFilterType::kAllowAllSites,
     .result = WebFilterType::kTryToBlockMatureSites},
    {.test_name = "TryToBlockMatureSites_3",
     .family_link = WebFilterType::kTryToBlockMatureSites,
     .device = WebFilterType::kDisabled,
     .result = WebFilterType::kTryToBlockMatureSites},
    {.test_name = "TryToBlockMatureSites_4",
     .family_link = WebFilterType::kCertainSites,
     .device = WebFilterType::kTryToBlockMatureSites,
     .result = WebFilterType::kTryToBlockMatureSites},
    {.test_name = "TryToBlockMatureSites_5",
     .family_link = WebFilterType::kAllowAllSites,
     .device = WebFilterType::kTryToBlockMatureSites,
     .result = WebFilterType::kTryToBlockMatureSites},
    {.test_name = "TryToBlockMatureSites_6",
     .family_link = WebFilterType::kDisabled,
     .device = WebFilterType::kTryToBlockMatureSites,
     .result = WebFilterType::kTryToBlockMatureSites},

    // Then if family link is kCertainSites, the result is kCertainSites.
    {.test_name = "CertainSites_1",
     .family_link = WebFilterType::kCertainSites,
     .device = WebFilterType::kAllowAllSites,
     .result = WebFilterType::kCertainSites},
    {.test_name = "CertainSites_2",
     .family_link = WebFilterType::kCertainSites,
     .device = WebFilterType::kDisabled,
     .result = WebFilterType::kCertainSites},

    // Then if any setting is kAllowAllSites, the result is kAllowAllSites.
    {.test_name = "AllowAllSites_1",
     .family_link = WebFilterType::kAllowAllSites,
     .device = WebFilterType::kAllowAllSites,
     .result = WebFilterType::kAllowAllSites},
    {.test_name = "AllowAllSites_2",
     .family_link = WebFilterType::kAllowAllSites,
     .device = WebFilterType::kDisabled,
     .result = WebFilterType::kAllowAllSites},
    {.test_name = "AllowAllSites_3",
     .family_link = WebFilterType::kDisabled,
     .device = WebFilterType::kAllowAllSites,
     .result = WebFilterType::kAllowAllSites},

    // Then both disabled, the result is kDisabled.
    {.test_name = "Disabled_1",
     .family_link = WebFilterType::kDisabled,
     .device = WebFilterType::kDisabled,
     .result = WebFilterType::kDisabled},
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserUrlFilteringServiceWebFilterTypeAndroidTest,
    testing::ValuesIn(kWebFilterTypeTestParams),
    [](const testing::TestParamInfo<
        SupervisedUserUrlFilteringServiceWebFilterTypeAndroidTest::ParamType>&
           info) {
      return info.param.test_name;
    });

// This suite simply proves that the sync filtering behavior is not affected
// by the device parental controls. To see comprehensive suite for family link
// sync filtering behavior, see family link specific suites
// (family_link*unittest.cc).
class SupervisedUserUrlFilteringServiceSyncBehaviorAndroidTest
    : public SupervisedUserUrlFilteringServiceTestBase {
 protected:
  WebFilteringResult GetSyncFilteringBehavior(std::string_view url) {
    return test_environment().url_filtering_service()->GetFilteringBehavior(
        GURL(url));
  }
};

TEST_F(SupervisedUserUrlFilteringServiceSyncBehaviorAndroidTest,
       EnabledDeviceParentalControls_DontAffectSyncBehavior) {
  EnableParentalControls(*test_environment().pref_service());
  test_environment().SetWebFilterType(WebFilterType::kCertainSites);
  test_environment().SetManualFilterForHost("http://google.com",
                                            /*allowlist=*/true);

  EXPECT_TRUE(GetSyncFilteringBehavior("http://google.com").IsAllowed());
  EXPECT_FALSE(GetSyncFilteringBehavior("http://example.com").IsAllowed());

  AndroidParentalControls& parental_controls =
      test_environment().device_parental_controls();
  // Set device parental controls to allow all sites.
  parental_controls.SetSearchContentFiltersEnabledForTesting(true);
  EXPECT_TRUE(GetSyncFilteringBehavior("http://google.com").IsAllowed());
  EXPECT_FALSE(GetSyncFilteringBehavior("http://example.com").IsAllowed());

  // Set device parental controls to use safe sites.
  parental_controls.SetBrowserContentFiltersEnabledForTesting(true);
  EXPECT_TRUE(GetSyncFilteringBehavior("http://google.com").IsAllowed());
  EXPECT_FALSE(GetSyncFilteringBehavior("http://example.com").IsAllowed());
}

class SupervisedUserUrlFilteringServiceAsyncBehaviorAndroidTest
    : public SupervisedUserUrlFilteringServiceTestBase {
 protected:
  void GetAsyncFilteringBehavior(const GURL& url,
                                 WebFilteringResult::Callback callback) {
    test_environment().url_filtering_service()->GetFilteringBehavior(
        url, /*skip_manual_parent_filter=*/false, std::move(callback),
        WebFilterMetricsOptions());
  }

  void GetAsyncFilteringBehaviorForSubFrame(
      const GURL& url,
      WebFilteringResult::Callback callback) {
    test_environment().url_filtering_service()->GetFilteringBehaviorForSubFrame(
        url, GURL("http://main.example.com"), std::move(callback),
        WebFilterMetricsOptions());
  }
};

TEST_F(SupervisedUserUrlFilteringServiceAsyncBehaviorAndroidTest,
       OnlyFamilyLinkFilterIsUsed) {
  EnableParentalControls(*test_environment().pref_service());
  ASSERT_FALSE(
      test_environment().device_parental_controls().IsWebFilteringEnabled());

  GURL url = GURL("http://example.com");
  EXPECT_CALL(test_environment().device_parental_controls_url_checker_client(),
              CheckURL(url, _))
      .Times(0);

  // Family link client will be called once (for main frame and subframe
  // checks; second check is returned from url checker client's cache).
  EXPECT_CALL(test_environment().family_link_url_checker_client(),
              CheckURL(url, _))
      .Times(1);
  test_environment().family_link_url_checker_client().ScheduleResolution(
      safe_search_api::ClientClassification::kAllowed);

  GetAsyncFilteringBehavior(
      url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_TRUE(result.IsAllowed());
      }));
  GetAsyncFilteringBehaviorForSubFrame(
      url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_TRUE(result.IsAllowed());
      }));

  // Histograms are recorded twice (for main frame and subframe checks).
  histogram_tester().ExpectUniqueSample(
      "SupervisedUsers.All.TopLevelFilteringResult.Default",
      SupervisedUserFilterTopLevelResult::kAllow, 2);
  histogram_tester().ExpectUniqueSample(
      "SupervisedUsers.Account.TopLevelFilteringResult.Default",
      SupervisedUserFilterTopLevelResult::kAllow, 2);
}

TEST_F(SupervisedUserUrlFilteringServiceAsyncBehaviorAndroidTest,
       OnlyDeviceParentalControlsFilterIsUsed) {
  test_environment()
      .device_parental_controls()
      .SetBrowserContentFiltersEnabledForTesting(true);

  GURL url = GURL("http://example.com");
  EXPECT_CALL(test_environment().device_parental_controls_url_checker_client(),
              CheckURL(url, _))
      .Times(1);
  test_environment()
      .device_parental_controls_url_checker_client()
      .ScheduleResolution(safe_search_api::ClientClassification::kAllowed);

  // Family link client will be called once (for main frame and subframe
  // checks; second check is returned from url checker client's cache).
  EXPECT_CALL(test_environment().family_link_url_checker_client(),
              CheckURL(url, _))
      .Times(0);
  GetAsyncFilteringBehavior(
      url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_TRUE(result.IsAllowed());
      }));
  GetAsyncFilteringBehaviorForSubFrame(
      url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_TRUE(result.IsAllowed());
      }));

  // Histograms are recorded twice (for main frame and subframe checks).
  histogram_tester().ExpectUniqueSample(
      "SupervisedUsers.All.TopLevelFilteringResult.Default",
      SupervisedUserFilterTopLevelResult::kAllow, 2);
  histogram_tester().ExpectUniqueSample(
      "SupervisedUsers.Device.TopLevelFilteringResult.Default",
      SupervisedUserFilterTopLevelResult::kAllow, 2);
}

TEST_F(SupervisedUserUrlFilteringServiceAsyncBehaviorAndroidTest,
       DeviceParenalControlsHavePriorityOverFamilyLink) {
  // Both systems are enabled.
  EnableParentalControls(*test_environment().pref_service());
  test_environment()
      .device_parental_controls()
      .SetBrowserContentFiltersEnabledForTesting(true);

  GURL url = GURL("http://example.com");

  // Since preferred device-specific client will yield blocked result, family
  // link client is not called at all.
  EXPECT_CALL(test_environment().family_link_url_checker_client(),
              CheckURL(url, _))
      .Times(0);
  EXPECT_CALL(test_environment().device_parental_controls_url_checker_client(),
              CheckURL(url, _))
      .Times(1);
  test_environment()
      .device_parental_controls_url_checker_client()
      .ScheduleResolution(safe_search_api::ClientClassification::kRestricted);

  GetAsyncFilteringBehavior(
      url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_TRUE(result.IsBlocked());
      }));
  GetAsyncFilteringBehaviorForSubFrame(
      url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_TRUE(result.IsBlocked());
      }));

  // Histograms are recorded twice (for main frame and subframe checks).
  histogram_tester().ExpectUniqueSample(
      "SupervisedUsers.All.TopLevelFilteringResult.Default",
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 2);
  histogram_tester().ExpectUniqueSample(
      "SupervisedUsers.Device.TopLevelFilteringResult.Default",
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 2);
}

TEST_F(SupervisedUserUrlFilteringServiceAsyncBehaviorAndroidTest,
       FamilyLinkIsFallbackToDeviceParentalControls) {
  // Both systems are enabled.
  EnableParentalControls(*test_environment().pref_service());
  test_environment()
      .device_parental_controls()
      .SetBrowserContentFiltersEnabledForTesting(true);

  GURL url = GURL("http://example.com");

  // Preferred device-specific client allows the url, but then the family link
  // client blocks it. Family link client is called second and its result
  // is used.
  EXPECT_CALL(test_environment().device_parental_controls_url_checker_client(),
              CheckURL(url, _))
      .Times(1);
  test_environment()
      .device_parental_controls_url_checker_client()
      .ScheduleResolution(safe_search_api::ClientClassification::kAllowed);

  EXPECT_CALL(test_environment().family_link_url_checker_client(),
              CheckURL(url, _))
      .Times(1);
  test_environment().family_link_url_checker_client().ScheduleResolution(
      safe_search_api::ClientClassification::kRestricted);

  GetAsyncFilteringBehavior(
      url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_TRUE(result.IsBlocked());
      }));
  GetAsyncFilteringBehaviorForSubFrame(
      url, base::BindLambdaForTesting([](WebFilteringResult result) {
        EXPECT_TRUE(result.IsBlocked());
      }));

  // Histograms are recorded twice (for main frame and subframe checks).
  histogram_tester().ExpectUniqueSample(
      "SupervisedUsers.All.TopLevelFilteringResult.Default",
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 2);
  histogram_tester().ExpectUniqueSample(
      "SupervisedUsers.Device.TopLevelFilteringResult.Default",
      SupervisedUserFilterTopLevelResult::kAllow, 2);
  histogram_tester().ExpectUniqueSample(
      "SupervisedUsers.Account.TopLevelFilteringResult.Default",
      SupervisedUserFilterTopLevelResult::kBlockSafeSites, 2);
}
}  // namespace
}  // namespace supervised_user
