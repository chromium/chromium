// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"

#include <string_view>

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/fixed_flat_set.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {
constexpr auto kDefaultUrls = base::MakeFixedFlatSet<std::string_view>(
    {"https://onedrive.live.com", "https://1drv.ms", "https://www.office.com",
     "https://m365.cloud.microsoft", "https://www.microsoft365.com"});
constexpr auto kDefaultDomains = base::MakeFixedFlatSet<std::string_view>(
    {"https://sharepoint.com", "https://cloud.microsoft"});

void ValidateScopeExtensions(const base::flat_set<std::string_view>& urls,
                             const base::flat_set<std::string_view>& domains,
                             const ScopeExtensions& scope_extensions) {
  EXPECT_EQ(scope_extensions.size(), urls.size() + domains.size());
  for (auto scope_extension : scope_extensions) {
    const std::string origin = scope_extension.origin.Serialize();
    if (scope_extension.has_origin_wildcard) {
      EXPECT_TRUE(domains.contains(origin)) << origin;
    } else {
      EXPECT_TRUE(urls.contains(origin)) << origin;
    }
  }
}

void ExpectDefaultScopeExtensions(const ScopeExtensions& scope_extensions) {
  ValidateScopeExtensions(base::flat_set<std::string_view>(kDefaultUrls.begin(),
                                                           kDefaultUrls.end()),
                          base::flat_set<std::string_view>(
                              kDefaultDomains.begin(), kDefaultDomains.end()),
                          scope_extensions);
}
}  // namespace

class ChromeOsWebAppExperimentsTest
    : public testing::Test,
      public testing::WithParamInterface<
          apps::test::LinkCapturingFeatureVersion> {
 protected:
  ChromeOsWebAppExperimentsTest() = default;
  ChromeOsWebAppExperimentsTest(const ChromeOsWebAppExperimentsTest&) = delete;
  ChromeOsWebAppExperimentsTest& operator=(
      const ChromeOsWebAppExperimentsTest&) = delete;
  ~ChromeOsWebAppExperimentsTest() override = default;

  void TearDown() override { scoped_feature_list_.Reset(); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ChromeOsWebAppExperimentsTest, GetScopeExtensions_NotM365) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      apps::test::GetFeaturesToEnableLinkCapturingUX(GetParam()), {});

  const std::string web_app_id = "test_id";
  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(web_app_id);

  EXPECT_TRUE(scope_extensions.empty());
}

TEST_P(ChromeOsWebAppExperimentsTest, GetScopeExtensions_M365) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      apps::test::GetFeaturesToEnableLinkCapturingUX(GetParam()), {});

  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(ash::kMicrosoft365AppId);

  ExpectDefaultScopeExtensions(scope_extensions);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeOsWebAppExperimentsTest,
    testing::Values(apps::test::LinkCapturingFeatureVersion::kV2DefaultOff,
                    apps::test::LinkCapturingFeatureVersion::kV2DefaultOn),
    apps::test::LinkCapturingVersionToString);

}  // namespace web_app
