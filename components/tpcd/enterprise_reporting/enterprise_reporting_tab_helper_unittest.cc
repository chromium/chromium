// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/enterprise_reporting/enterprise_reporting_tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/features.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(content::BrowserContext* browser_context) {
    browser_context->GetDefaultStoragePartition()->SetNetworkContextForTesting(
        receiver_.BindNewPipeAndPassRemote());
  }

  MockNetworkContext(const MockNetworkContext&) = delete;
  MockNetworkContext& operator=(const MockNetworkContext&) = delete;

  ~MockNetworkContext() override = default;

  bool WaitAndGetInvokedQueueEnterpriseReport() {
    return invoked_queue_enterprise_report_.Get();
  }

  bool InvokedQueueEnterpriseReportSet() {
    return invoked_queue_enterprise_report_.IsReady();
  }

  void QueueEnterpriseReport(const std::string& type,
                             const std::string& group,
                             const GURL& url,
                             base::Value::Dict body) override {
    invoked_queue_enterprise_report_.SetValue(true);
  }

 private:
  mojo::Receiver<network::mojom::NetworkContext> receiver_{this};
  base::test::TestFuture<bool> invoked_queue_enterprise_report_;
};

class EnterpriseReportingTabHelperTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kReportingApiEnableEnterpriseCookieIssues);
    content::RenderViewHostTestHarness::SetUp();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(EnterpriseReportingTabHelperTest, ReportingFeatureDisabled) {
  // Disable the reporting feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);

  auto network_context =
      std::make_unique<MockNetworkContext>(browser_context());

  tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
      CreateForWebContents(web_contents());
  tpcd::enterprise_reporting::EnterpriseReportingTabHelper* tab_helper =
      tpcd::enterprise_reporting::EnterpriseReportingTabHelper::FromWebContents(
          web_contents());
  const GURL url("http://www.google.com");

  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(url, "A=1", base::Time::Now()));

  // Create a status that would cause a report to be queued if the feature
  // was enabled, but the report won't be queued since the feature is disabled.
  net::CookieInclusionStatus status;
  status.AddWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT);

  const content::CookieAccessDetails& details = content::CookieAccessDetails(
      content::CookieAccessDetails::Type::kChange, url, url,
      {{*cookie, net::CookieAccessResult(status)}}, 1u);
  tab_helper->OnCookiesAccessed(main_rfh(), details);
  // QueueEnterpriseReport() shouldn't have been called
  EXPECT_FALSE(network_context->InvokedQueueEnterpriseReportSet());
}

TEST_F(EnterpriseReportingTabHelperTest,
       OnCookiesAccessedWithExcludeThirdPartyBlockedWithinFirstPartySetError) {
  auto network_context =
      std::make_unique<MockNetworkContext>(browser_context());

  tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
      CreateForWebContents(web_contents());
  tpcd::enterprise_reporting::EnterpriseReportingTabHelper* tab_helper =
      tpcd::enterprise_reporting::EnterpriseReportingTabHelper::FromWebContents(
          web_contents());
  const GURL url("http://www.google.com");

  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(url, "A=1", base::Time::Now()));

  net::CookieInclusionStatus status;
  status.AddExclusionReason(
      net::CookieInclusionStatus::
          EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET);

  const content::CookieAccessDetails& details = content::CookieAccessDetails(
      content::CookieAccessDetails::Type::kChange, url, url,
      {{*cookie, net::CookieAccessResult(status)}}, 1u);
  tab_helper->OnCookiesAccessed(main_rfh(), details);
  EXPECT_TRUE(network_context->WaitAndGetInvokedQueueEnterpriseReport());
}

TEST_F(EnterpriseReportingTabHelperTest,
       OnCookiesAccessedWithExcludeThirdPartyPhaseoutError) {
  auto network_context =
      std::make_unique<MockNetworkContext>(browser_context());

  tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
      CreateForWebContents(web_contents());
  tpcd::enterprise_reporting::EnterpriseReportingTabHelper* tab_helper =
      tpcd::enterprise_reporting::EnterpriseReportingTabHelper::FromWebContents(
          web_contents());
  const GURL url("http://www.google.com");

  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(url, "A=1", base::Time::Now()));

  net::CookieInclusionStatus status;
  status.AddExclusionReason(
      net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT);

  const content::CookieAccessDetails& details = content::CookieAccessDetails(
      content::CookieAccessDetails::Type::kChange, url, url,
      {{*cookie, net::CookieAccessResult(status)}}, 1u);
  tab_helper->OnCookiesAccessed(main_rfh(), details);
  EXPECT_TRUE(network_context->WaitAndGetInvokedQueueEnterpriseReport());
}

TEST_F(EnterpriseReportingTabHelperTest,
       OnCookiesAccessedWithWarnThirdPartyPhaseoutWarning) {
  auto network_context =
      std::make_unique<MockNetworkContext>(browser_context());

  tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
      CreateForWebContents(web_contents());
  tpcd::enterprise_reporting::EnterpriseReportingTabHelper* tab_helper =
      tpcd::enterprise_reporting::EnterpriseReportingTabHelper::FromWebContents(
          web_contents());
  const GURL url("http://www.google.com");

  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(url, "A=1", base::Time::Now()));

  net::CookieInclusionStatus status;
  status.AddWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT);

  const content::CookieAccessDetails& details = content::CookieAccessDetails(
      content::CookieAccessDetails::Type::kChange, url, url,
      {{*cookie, net::CookieAccessResult(status)}}, 1u);
  tab_helper->OnCookiesAccessed(main_rfh(), details);
  EXPECT_TRUE(network_context->WaitAndGetInvokedQueueEnterpriseReport());
}

TEST_F(EnterpriseReportingTabHelperTest,
       OnCookiesAccessedWithoutQueueingReport) {
  auto network_context =
      std::make_unique<MockNetworkContext>(browser_context());

  tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
      CreateForWebContents(web_contents());
  tpcd::enterprise_reporting::EnterpriseReportingTabHelper* tab_helper =
      tpcd::enterprise_reporting::EnterpriseReportingTabHelper::FromWebContents(
          web_contents());
  const GURL url("http://www.google.com");

  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(url, "A=1", base::Time::Now()));

  const content::CookieAccessDetails& details = content::CookieAccessDetails(
      content::CookieAccessDetails::Type::kChange, url, url, {{*cookie}}, 1u);
  tab_helper->OnCookiesAccessed(main_rfh(), details);
  // QueueEnterpriseReport() shouldn't have been called
  EXPECT_FALSE(network_context->InvokedQueueEnterpriseReportSet());
}

}  // namespace
