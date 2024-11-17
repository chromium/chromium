// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"

#include <string_view>

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/fixed_flat_set.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {
constexpr char kM365UrlsFinchParamName[] = "m365-scope-extensions-urls";
constexpr char kM365DomainsFinchParamName[] = "m365-scope-extensions-domains";

constexpr auto kDefaultUrls = base::MakeFixedFlatSet<std::string_view>(
    {"https://onedrive.live.com", "https://1drv.ms", "https://www.office.com"});
constexpr auto kDefaultDomains =
    base::MakeFixedFlatSet<std::string_view>({"https://sharepoint.com"});

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

class ChromeOsWebAppExperimentsTest : public testing::Test {
 protected:
  ChromeOsWebAppExperimentsTest() = default;
  ChromeOsWebAppExperimentsTest(const ChromeOsWebAppExperimentsTest&) = delete;
  ChromeOsWebAppExperimentsTest& operator=(
      const ChromeOsWebAppExperimentsTest&) = delete;
  ~ChromeOsWebAppExperimentsTest() override = default;

  void TearDown() override { scoped_feature_list_.Reset(); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeOsWebAppExperimentsTest, GetScopeExtensions_NotM365) {
  const std::string web_app_id = "test_id";
  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(web_app_id);

  EXPECT_TRUE(scope_extensions.empty());
}

TEST_F(ChromeOsWebAppExperimentsTest,
       GetScopeExtensions_M365FinchFlagDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      chromeos::features::kMicrosoft365ScopeExtensions);

  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(ash::kMicrosoft365AppId);

  ExpectDefaultScopeExtensions(scope_extensions);
}

TEST_F(ChromeOsWebAppExperimentsTest,
       GetScopeExtensions_M365FinchFlagDefaultParams) {
  scoped_feature_list_.InitAndEnableFeature(
      chromeos::features::kMicrosoft365ScopeExtensions);

  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(ash::kMicrosoft365AppId);

  ExpectDefaultScopeExtensions(scope_extensions);
}

TEST_F(ChromeOsWebAppExperimentsTest,
       GetScopeExtensions_M365FinchFlagParamsSingleValue) {
  const std::string scope_extension_urls = "https://example.com";
  const std::string scope_extension_domains = "https://domain.com";

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chromeos::features::kMicrosoft365ScopeExtensions,
      {{kM365UrlsFinchParamName, scope_extension_urls},
       {kM365DomainsFinchParamName, scope_extension_domains}});

  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(ash::kMicrosoft365AppId);

  ValidateScopeExtensions({"https://example.com"}, {"https://domain.com"},
                          scope_extensions);
}

TEST_F(ChromeOsWebAppExperimentsTest,
       GetScopeExtensions_M365FinchFlagParamsMultipleValues) {
  const std::string scope_extension_urls =
      "https://example.com,http://example2.com , "
      "https://www.example3.xyz/somepath?somequery";
  const std::string scope_extension_domains =
      "https://domain.com      , https://domain2.org";

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chromeos::features::kMicrosoft365ScopeExtensions,
      {{kM365UrlsFinchParamName, scope_extension_urls},
       {kM365DomainsFinchParamName, scope_extension_domains}});

  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(ash::kMicrosoft365AppId);

  ValidateScopeExtensions({"https://example.com", "http://example2.com",
                           "https://www.example3.xyz"},
                          {"https://domain.com", "https://domain2.org"},
                          scope_extensions);
}

TEST_F(ChromeOsWebAppExperimentsTest,
       GetScopeExtensions_M365FinchFlagParamsInvalid) {
  const std::string scope_extension_urls =
      "xyz://abc.def///,https://example.com,about://blank,https://example2.com";
  const std::string scope_extension_domains = "invalid_test,https://domain.com";

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chromeos::features::kMicrosoft365ScopeExtensions,
      {{kM365UrlsFinchParamName, scope_extension_urls},
       {kM365DomainsFinchParamName, scope_extension_domains}});

  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(ash::kMicrosoft365AppId);

  ValidateScopeExtensions({"https://example.com", "https://example2.com"},
                          {"https://domain.com"}, scope_extensions);
}

TEST_F(ChromeOsWebAppExperimentsTest,
       GetScopeExtensions_M365FinchFlagParamsDomainStartsWithDot) {
  const std::string scope_extension_urls = "https://example.com/";
  const std::string scope_extension_domains =
      "https://domain.com,https://.domain2.com";

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chromeos::features::kMicrosoft365ScopeExtensions,
      {{kM365UrlsFinchParamName, scope_extension_urls},
       {kM365DomainsFinchParamName, scope_extension_domains}});

  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(ash::kMicrosoft365AppId);

  ValidateScopeExtensions({"https://example.com"}, {"https://domain.com"},
                          scope_extensions);
}

TEST_F(ChromeOsWebAppExperimentsTest,
       GetScopeExtensions_M365FinchFlagParamsCommaInUrl) {
  const std::string scope_extension_urls =
      "https://example.com/?abc,treated_as_new_value";
  const std::string scope_extension_domains = "https://domain.com";

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chromeos::features::kMicrosoft365ScopeExtensions,
      {{kM365UrlsFinchParamName, scope_extension_urls},
       {kM365DomainsFinchParamName, scope_extension_domains}});

  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(ash::kMicrosoft365AppId);

  ValidateScopeExtensions({"https://example.com"}, {"https://domain.com"},
                          scope_extensions);
}

TEST_F(ChromeOsWebAppExperimentsTest,
       GetScopeExtensions_M365FinchFlagParamsUrlsOnly) {
  const std::string scope_extension_urls =
      "https://example.com,https://example2.com";

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chromeos::features::kMicrosoft365ScopeExtensions,
      {{kM365UrlsFinchParamName, scope_extension_urls}});

  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(ash::kMicrosoft365AppId);

  ValidateScopeExtensions({"https://example.com", "https://example2.com"},
                          base::flat_set<std::string_view>(
                              kDefaultDomains.begin(), kDefaultDomains.end()),
                          scope_extensions);
}

TEST_F(ChromeOsWebAppExperimentsTest,
       GetScopeExtensions_M365FinchFlagParamsDomainsOnly) {
  const std::string scope_extension_domains =
      "https://domain.com,https://domain2.com";

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chromeos::features::kMicrosoft365ScopeExtensions,
      {{kM365DomainsFinchParamName, scope_extension_domains}});

  ScopeExtensions scope_extensions =
      ChromeOsWebAppExperiments::GetScopeExtensions(ash::kMicrosoft365AppId);

  ValidateScopeExtensions(base::flat_set<std::string_view>(kDefaultUrls.begin(),
                                                           kDefaultUrls.end()),
                          {"https://domain.com", "https://domain2.com"},
                          scope_extensions);
}

}  // namespace web_app
