// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/pref_url_list_matcher.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_reporting {

class FakeNavigationDataDelegate
    : public SaasUsageReportingController::NavigationDataDelegate {
 public:
  explicit FakeNavigationDataDelegate(GURL url, std::string encryption_protocol)
      : url_(std::move(url)),
        encryption_protocol_(std::move(encryption_protocol)) {}
  ~FakeNavigationDataDelegate() override = default;

  GURL GetUrl() const override { return url_; }
  std::string GetEncryptionProtocol() const override {
    return encryption_protocol_;
  }

 private:
  GURL url_;
  std::string encryption_protocol_;
};

class SaasUsageReportingControllerTest : public testing::Test {
 public:
  void SetUp() override {
    browser_pref_service_.registry()->RegisterListPref(
        kSaasUsageDomainUrlsForBrowser);
    browser_pref_service_.registry()->RegisterDictionaryPref(kSaasUsageReport);

    profile_pref_service_.registry()->RegisterListPref(
        kSaasUsageDomainUrlsForProfile);
    profile_pref_service_.registry()->RegisterDictionaryPref(kSaasUsageReport);

    controller_ = std::make_unique<SaasUsageReportingController>(
        &browser_pref_service_, &profile_pref_service_,
        std::make_unique<PrefURLListMatcher>(&browser_pref_service_,
                                             kSaasUsageDomainUrlsForBrowser),
        std::make_unique<PrefURLListMatcher>(&profile_pref_service_,
                                             kSaasUsageDomainUrlsForProfile));
  }

 protected:
  void SetBrowserUrls(const std::vector<std::string>& urls) {
    base::ListValue urls_list;
    for (const auto& url : urls) {
      urls_list.Append(url);
    }
    browser_pref_service_.SetList(kSaasUsageDomainUrlsForBrowser,
                                  std::move(urls_list));
  }

  void SetProfileUrls(const std::vector<std::string>& urls) {
    base::ListValue urls_list;
    for (const auto& url : urls) {
      urls_list.Append(url);
    }
    profile_pref_service_.SetList(kSaasUsageDomainUrlsForProfile,
                                  std::move(urls_list));
  }

  const base::DictValue& GetBrowserReport() {
    return browser_pref_service_.GetDict(kSaasUsageReport);
  }

  const base::DictValue& GetProfileReport() {
    return profile_pref_service_.GetDict(kSaasUsageReport);
  }

  FakeNavigationDataDelegate CreateNavigation(std::string_view url,
                                              std::string encryption_protocol) {
    return FakeNavigationDataDelegate(GURL(url),
                                      std::move(encryption_protocol));
  }

  void VerifyReportEntry(const base::DictValue& report,
                         const std::string& domain,
                         int expected_navigation_count,
                         const std::vector<std::string>& expected_protocols) {
    const auto* entry = report.FindDict(domain);
    ASSERT_TRUE(entry) << "No entry found for domain: " << domain;
    EXPECT_EQ(expected_navigation_count,
              entry->FindInt("navigation_count").value());

    const auto* protocols = entry->FindList("encryption_protocols");
    ASSERT_TRUE(protocols) << "No encryption_protocols list found for domain: "
                           << domain;
    ASSERT_EQ(expected_protocols.size(), protocols->size());
    EXPECT_THAT(*protocols,
                testing::UnorderedElementsAreArray(expected_protocols));
  }

  TestingPrefServiceSimple browser_pref_service_;
  TestingPrefServiceSimple profile_pref_service_;
  std::unique_ptr<SaasUsageReportingController> controller_;
};

TEST_F(SaasUsageReportingControllerTest, RecordNavigation_BrowserMatch) {
  SetBrowserUrls({"example.com"});

  controller_->RecordNavigation(
      CreateNavigation("https://example.com/page", "TLS 1.3"));

  VerifyReportEntry(GetBrowserReport(), "example.com", 1, {"TLS 1.3"});
  EXPECT_TRUE(GetProfileReport().empty());
}

TEST_F(SaasUsageReportingControllerTest, RecordNavigation_ProfileMatch) {
  SetProfileUrls({"example.com"});

  controller_->RecordNavigation(
      CreateNavigation("https://example.com/page", "TLS 1.3"));

  VerifyReportEntry(GetProfileReport(), "example.com", 1, {"TLS 1.3"});
  EXPECT_TRUE(GetBrowserReport().empty());
}

TEST_F(SaasUsageReportingControllerTest, RecordNavigation_BothMatch) {
  SetBrowserUrls({"example.com"});
  SetProfileUrls({"example.com"});

  controller_->RecordNavigation(
      CreateNavigation("https://example.com/page", "TLS 1.3"));

  VerifyReportEntry(GetBrowserReport(), "example.com", 1, {"TLS 1.3"});
  VerifyReportEntry(GetProfileReport(), "example.com", 1, {"TLS 1.3"});
}

TEST_F(SaasUsageReportingControllerTest, SubdomainMatching) {
  SetBrowserUrls({"example.com"});

  controller_->RecordNavigation(
      CreateNavigation("https://sub.example.com/page", "TLS 1.3"));

  VerifyReportEntry(GetBrowserReport(), "example.com", 1, {"TLS 1.3"});
}

TEST_F(SaasUsageReportingControllerTest, NoMatch) {
  SetBrowserUrls({"google.com"});
  SetProfileUrls({"google.com"});

  controller_->RecordNavigation(
      CreateNavigation("https://example.com/page", "TLS 1.3"));

  EXPECT_TRUE(GetBrowserReport().empty());
  EXPECT_TRUE(GetProfileReport().empty());
}

TEST_F(SaasUsageReportingControllerTest, MultipleNavigations) {
  SetBrowserUrls({"example.com"});

  controller_->RecordNavigation(
      CreateNavigation("https://example.com/page1", "TLS 1.3"));
  controller_->RecordNavigation(
      CreateNavigation("https://sub.example.com/page2", "TLS 1.2"));

  VerifyReportEntry(GetBrowserReport(), "example.com", 2,
                    {"TLS 1.3", "TLS 1.2"});
}

}  // namespace enterprise_reporting
