// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/enterprise_reporting/enterprise_reporting_tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/features.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tpcd::enterprise_reporting {
namespace {

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver)
      : receiver_(this, std::move(receiver)) {}

  MOCK_METHOD(
      void,
      QueueEnterpriseReport,
      (const std::string&, const std::string&, const GURL&, base::Value::Dict),
      (override));

 private:
  mojo::Receiver<network::mojom::NetworkContext> receiver_;
};

class EnterpriseReportingTabHelperTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kReportingApiEnableEnterpriseCookieIssues);
    content::RenderViewHostTestHarness::SetUp();

    mojo::PendingRemote<network::mojom::NetworkContext> network_context_remote;
    mock_network_context_ = std::make_unique<MockNetworkContext>(
        network_context_remote.InitWithNewPipeAndPassReceiver());
    main_rfh()->GetStoragePartition()->SetNetworkContextForTesting(
        std::move(network_context_remote));

    cookie_ = *net::CanonicalCookie::CreateForTesting(kUrl_, "A=1",
                                                      base::Time::Now());
  }

  ~EnterpriseReportingTabHelperTest() override = default;

 protected:
  MockNetworkContext* mock_network_context() {
    return mock_network_context_.get();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  const GURL kUrl_ = GURL("http://www.google.com");
  net::CanonicalCookie cookie_;

 private:
  std::unique_ptr<MockNetworkContext> mock_network_context_;
};

TEST_F(EnterpriseReportingTabHelperTest, ReportingFeatureDisabled) {
  // QueueEnterpriseReport() shouldn't be called
  EXPECT_CALL(*mock_network_context(), QueueEnterpriseReport).Times(0);

  // Disable the reporting feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);

  tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
      CreateForWebContents(web_contents());
  tpcd::enterprise_reporting::EnterpriseReportingTabHelper* tab_helper =
      tpcd::enterprise_reporting::EnterpriseReportingTabHelper::FromWebContents(
          web_contents());

  // Create a status that would cause a report to be queued if the
  // feature was enabled, but the report won't be queued since the
  // feature is disabled.
  net::CookieInclusionStatus status;
  status.AddWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT);
  const content::CookieAccessDetails& details = content::CookieAccessDetails(
      content::CookieAccessDetails::Type::kChange, kUrl_, kUrl_,
      {{cookie_, net::CookieAccessResult(status)}}, 1u);

  tab_helper->OnCookiesAccessed(main_rfh(), details);
}

TEST_F(EnterpriseReportingTabHelperTest,
       OnCookiesAccessedWithExcludeThirdPartyBlockedWithinFirstPartySetError) {
  // TODO(crbug.com/352737473): Update group parameter to use endpoint from
  // subscription.
  EXPECT_CALL(
      *mock_network_context(),
      QueueEnterpriseReport("enterprise-third-party-cookie-access-error",
                            "enterprise-third-party-cookie-access-error",
                            GURL(""), base::test::IsJson(R"json({
            "frameUrl": "",
             "accessUrl": "http://www.google.com/",
             "name": "A",
             "domain": "www.google.com",
             "path": "/",
             "accessOperation": "write"
          })json")));

  tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
      CreateForWebContents(web_contents());
  tpcd::enterprise_reporting::EnterpriseReportingTabHelper* tab_helper =
      tpcd::enterprise_reporting::EnterpriseReportingTabHelper::FromWebContents(
          web_contents());

  net::CookieInclusionStatus status;
  status.AddExclusionReason(
      net::CookieInclusionStatus::
          EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET);
  const content::CookieAccessDetails& details = content::CookieAccessDetails(
      content::CookieAccessDetails::Type::kChange, kUrl_, kUrl_,
      {{cookie_, net::CookieAccessResult(status)}}, 1u);

  tab_helper->OnCookiesAccessed(main_rfh(), details);
}

TEST_F(EnterpriseReportingTabHelperTest,
       OnCookiesAccessedWithExcludeThirdPartyPhaseoutError) {
  // TODO(crbug.com/352737473): Update group parameter to use endpoint from
  // subscription.
  EXPECT_CALL(
      *mock_network_context(),
      QueueEnterpriseReport("enterprise-third-party-cookie-access-error",
                            "enterprise-third-party-cookie-access-error",
                            GURL(""), base::test::IsJson(R"json({
            "frameUrl": "",
             "accessUrl": "http://www.google.com/",
             "name": "A",
             "domain": "www.google.com",
             "path": "/",
             "accessOperation": "write"
          })json")));

  tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
      CreateForWebContents(web_contents());
  tpcd::enterprise_reporting::EnterpriseReportingTabHelper* tab_helper =
      tpcd::enterprise_reporting::EnterpriseReportingTabHelper::FromWebContents(
          web_contents());

  net::CookieInclusionStatus status;
  status.AddExclusionReason(
      net::CookieInclusionStatus::EXCLUDE_THIRD_PARTY_PHASEOUT);
  const content::CookieAccessDetails& details = content::CookieAccessDetails(
      content::CookieAccessDetails::Type::kChange, kUrl_, kUrl_,
      {{cookie_, net::CookieAccessResult(status)}}, 1u);

  tab_helper->OnCookiesAccessed(main_rfh(), details);
}

TEST_F(EnterpriseReportingTabHelperTest,
       OnCookiesAccessedWithWarnThirdPartyPhaseoutWarning) {
  // TODO(crbug.com/352737473): Update group parameter to use endpoint from
  // subscription.
  EXPECT_CALL(
      *mock_network_context(),
      QueueEnterpriseReport("enterprise-third-party-cookie-access-warning",
                            "enterprise-third-party-cookie-access-warning",
                            GURL(""), base::test::IsJson(R"json({
            "frameUrl": "",
             "accessUrl": "http://www.google.com/",
             "name": "A",
             "domain": "www.google.com",
             "path": "/",
             "accessOperation": "read"
          })json")));

  tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
      CreateForWebContents(web_contents());
  tpcd::enterprise_reporting::EnterpriseReportingTabHelper* tab_helper =
      tpcd::enterprise_reporting::EnterpriseReportingTabHelper::FromWebContents(
          web_contents());

  net::CookieInclusionStatus status;
  status.AddWarningReason(
      net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT);
  const content::CookieAccessDetails& details = content::CookieAccessDetails(
      content::CookieAccessDetails::Type::kRead, kUrl_, kUrl_,
      {{cookie_, net::CookieAccessResult(status)}}, 1u);

  tab_helper->OnCookiesAccessed(main_rfh(), details);
}

TEST_F(EnterpriseReportingTabHelperTest,
       OnCookiesAccessedWithoutQueueingReport) {
  // QueueEnterpriseReport() shouldn't be called
  EXPECT_CALL(*mock_network_context(), QueueEnterpriseReport).Times(0);

  tpcd::enterprise_reporting::EnterpriseReportingTabHelper::
      CreateForWebContents(web_contents());
  tpcd::enterprise_reporting::EnterpriseReportingTabHelper* tab_helper =
      tpcd::enterprise_reporting::EnterpriseReportingTabHelper::FromWebContents(
          web_contents());

  const content::CookieAccessDetails& details =
      content::CookieAccessDetails(content::CookieAccessDetails::Type::kChange,
                                   kUrl_, kUrl_, {{cookie_}}, 1u);

  tab_helper->OnCookiesAccessed(main_rfh(), details);
}

}  // namespace
}  // namespace tpcd::enterprise_reporting
