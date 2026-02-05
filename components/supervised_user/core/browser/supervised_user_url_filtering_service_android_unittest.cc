// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

class SupervisedUserUrlFilteringServiceTestBase : public testing::Test {
 protected:
  void TearDown() override { test_environment_.Shutdown(); }

  SupervisedUserTestEnvironment& test_environment() {
    return test_environment_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  SupervisedUserTestEnvironment test_environment_;
};

struct WebFilterTypeTestParams {
  std::string test_name;
  WebFilterType family_link;
  WebFilterType device;
  WebFilterType result;
  // Special exceptions for few cases that have different behavior in the MVP
  // implementation of the URL filtering service (which is ignoring device
  // settings in favor of family link settings).
  std::optional<WebFilterType> mvp_result;
};

class SupervisedUserUrlFilteringServiceWebFilterTypeAndroidTest
    : public WithFeatureOverrideAndParamInterface<WebFilterTypeTestParams>,
      public SupervisedUserUrlFilteringServiceTestBase {
 protected:
  SupervisedUserUrlFilteringServiceWebFilterTypeAndroidTest()
      : WithFeatureOverrideAndParamInterface(
            kSupervisedUserUseUrlFilteringService) {}
};

TEST_P(SupervisedUserUrlFilteringServiceWebFilterTypeAndroidTest,
       WebFilterTypeTest) {
  if (GetTestCase().family_link != WebFilterType::kDisabled) {
    EnableParentalControls(*test_environment().pref_service());
    test_environment().SetWebFilterType(GetTestCase().family_link);
  }

  AndroidParentalControls& parental_controls =
      test_environment().device_parental_controls();
  switch (GetTestCase().device) {
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

  if (!IsFeatureEnabled() &&
      GetTestCase().mvp_result.has_value()) {
    // MVP implementation (pre-feature)didn't allow for specific combinations of
    // settings and ignored the device settings in favor of the family link
    // settings.
    EXPECT_EQ(*GetTestCase().mvp_result,
              test_environment().url_filtering_service()->GetWebFilterType());
  } else {
    EXPECT_EQ(GetTestCase().result,
              test_environment().url_filtering_service()->GetWebFilterType());
  }
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
     .result = WebFilterType::kTryToBlockMatureSites,
     .mvp_result = WebFilterType::kCertainSites},
    {.test_name = "TryToBlockMatureSites_5",
     .family_link = WebFilterType::kAllowAllSites,
     .device = WebFilterType::kTryToBlockMatureSites,
     .result = WebFilterType::kTryToBlockMatureSites,
     .mvp_result = WebFilterType::kAllowAllSites},
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
     .result = WebFilterType::kAllowAllSites,
     .mvp_result = WebFilterType::kDisabled},

    // Then both disabled, the result is kDisabled.
    {.test_name = "Disabled_1",
     .family_link = WebFilterType::kDisabled,
     .device = WebFilterType::kDisabled,
     .result = WebFilterType::kDisabled},
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SupervisedUserUrlFilteringServiceWebFilterTypeAndroidTest,
    testing::Combine(testing::Bool(),
                     testing::ValuesIn(kWebFilterTypeTestParams)),
    [](const testing::TestParamInfo<
        SupervisedUserUrlFilteringServiceWebFilterTypeAndroidTest::ParamType>&
           info) {
      return std::string(std::get<0>(info.param) ? "With" : "Without") +
             std::string(kSupervisedUserUseUrlFilteringService.name) + "_" +
             std::get<1>(info.param).test_name;
    });

// This suite simply proves that the sync filtering behavior is not affected
// by the device parental controls. To see comprehensive suite for family link
// sync filtering behavior, see family link specific suites
// (family_link*unittest.cc).
class SupervisedUserUrlFilteringServiceSyncBehaviorAndroidTest
    : public ::base::test::WithFeatureOverride,
      public SupervisedUserUrlFilteringServiceTestBase {
 protected:
  SupervisedUserUrlFilteringServiceSyncBehaviorAndroidTest()
      : ::base::test::WithFeatureOverride(
            kSupervisedUserUseUrlFilteringService) {}
  WebFilteringResult GetSyncFilteringBehavior(std::string_view url) {
    return test_environment().url_filtering_service()->GetFilteringBehavior(
        GURL(url));
  }
};

TEST_P(SupervisedUserUrlFilteringServiceSyncBehaviorAndroidTest,
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

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    SupervisedUserUrlFilteringServiceSyncBehaviorAndroidTest);

}  // namespace
}  // namespace supervised_user
