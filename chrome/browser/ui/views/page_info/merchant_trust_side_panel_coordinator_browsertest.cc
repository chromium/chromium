// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/strings/escape.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/page_info/merchant_trust_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/page_info/merchant_trust_side_panel.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_info/web_view_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/merchant_trust_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using testing::Invoke;
using testing::Return;

namespace {
const char kMerchantReviewsUrl[] = "reviews.test";
const char kUrlWithMerchantTrustData[] = "a.test";
const char kUrlWithoutMerchantTrustData[] = "b.test";

page_info::MerchantData CreateValidMerchantData() {
  page_info::MerchantData merchant_data;
  merchant_data.star_rating = 3.8;
  merchant_data.count_rating = 45;
  merchant_data.page_url = GURL(kMerchantReviewsUrl);
  merchant_data.reviews_summary = "Test summary";
  return merchant_data;
}
}  // namespace

class MockMerchantTrustService : public page_info::MerchantTrustService {
 public:
  MockMerchantTrustService()
      : MerchantTrustService(nullptr, nullptr, false, nullptr) {}
  MOCK_METHOD(void,
              GetMerchantTrustInfo,
              (const GURL&, page_info::MerchantDataCallback),
              (const, override));
  MOCK_METHOD(void,
              RecordMerchantTrustInteraction,
              (const GURL&, page_info::MerchantTrustInteraction),
              (const, override));
};

class MerchantTrustSidePanelCoordinatorBrowserTest
    : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    service_ = static_cast<MockMerchantTrustService*>(
        MerchantTrustServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(&MerchantTrustSidePanelCoordinatorBrowserTest::
                                    BuildMockMerchantTrustService,
                                base::Unretained(this))));

    // Mock GetMerchanTrustInfo based on the requested URL.
    ON_CALL(*service(), GetMerchantTrustInfo(_, _))
        .WillByDefault(Invoke(
            [](const GURL& url, page_info::MerchantDataCallback callback) {
              std::move(callback).Run(
                  url, url == GURL(kUrlWithMerchantTrustData)
                           ? std::make_optional(CreateValidMerchantData())
                           : std::nullopt);
            }));
  }

  GURL CreateUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return browser()->GetFeatures().side_panel_coordinator();
  }

  base::test::ScopedFeatureList feature_list_;

  MockMerchantTrustService* service() { return service_; }

 private:
  std::unique_ptr<KeyedService> BuildMockMerchantTrustService(
      content::BrowserContext* context) {
    return std::make_unique<::testing::NiceMock<MockMerchantTrustService>>();
  }
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  raw_ptr<MockMerchantTrustService, DanglingUntriaged> service_;
};

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       ShowOnRefreshingMerchantSite) {
  GURL kGURLWithMerchantTrustData = CreateUrl(kUrlWithMerchantTrustData);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kGURLWithMerchantTrustData));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  ShowMerchantTrustSidePanel(web_contents(), CreateUrl(kMerchantReviewsUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Refresh the page and check that the side panel is still open.
  EXPECT_CALL(*service(), GetMerchantTrustInfo(_, _))
      .WillRepeatedly(
          Invoke([](const GURL& url, page_info::MerchantDataCallback callback) {
            std::move(callback).Run(
                url, std::make_optional(CreateValidMerchantData()));
          }));

  EXPECT_CALL(*service(), RecordMerchantTrustInteraction(
                              _, page_info::MerchantTrustInteraction::
                                     kSidePanelOpenedOnSameTabNavigation))
      .Times(1);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kGURLWithMerchantTrustData));

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return side_panel_coordinator()->IsSidePanelShowing(); }));
}

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       ClosePanelOnNavigationToNoMerchantSite) {
  GURL kGURLWithMerchantTrustData = CreateUrl(kUrlWithMerchantTrustData);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kGURLWithMerchantTrustData));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  ShowMerchantTrustSidePanel(web_contents(), CreateUrl(kMerchantReviewsUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Navigate to a different URL with no merchant trust data.
  EXPECT_CALL(*service(), GetMerchantTrustInfo(_, _))
      .WillRepeatedly(
          Invoke([](const GURL& url, page_info::MerchantDataCallback callback) {
            std::move(callback).Run(url, std::nullopt);
          }));
  EXPECT_CALL(*service(), RecordMerchantTrustInteraction(
                              _, page_info::MerchantTrustInteraction::
                                     kSidePanelClosedOnSameTabNavigation))
      .Times(1);
  GURL kGURLWithoutMerchantTrustData = CreateUrl(kUrlWithoutMerchantTrustData);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kGURLWithoutMerchantTrustData));
  // Side panel should eventually close, after the animation.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !side_panel_coordinator()->IsSidePanelShowing(); }));
}

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavRef) {
  GURL kGURLWithMerchantTrustData = CreateUrl(kUrlWithMerchantTrustData);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kGURLWithMerchantTrustData));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  GURL kMerchantReviewsGURL = CreateUrl(kMerchantReviewsUrl);
  ShowMerchantTrustSidePanel(web_contents(), kMerchantReviewsGURL);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Check that side panel remains open on navigation with an anchor.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), kGURLWithMerchantTrustData.Resolve("#ref")));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Check that the MerchantTrust url remains the same.
  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kMerchantReviewsGURL);
}

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavSameDocumentReplaceState) {
  GURL kGURLWithMerchantTrustData = CreateUrl(kUrlWithMerchantTrustData);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kGURLWithMerchantTrustData));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  GURL kMerchantReviewsGURL = CreateUrl(kMerchantReviewsUrl);
  ShowMerchantTrustSidePanel(web_contents(), kMerchantReviewsGURL);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Replace state with new path.
  GURL kUrlMerchantTrustWithPath2 =
      kGURLWithMerchantTrustData.Resolve("/title2.html");
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "history.replaceState({},'','title2.html')"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Check that side panel remains open on replace state.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Check that the MerchantTrust url remains the same.
  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kMerchantReviewsGURL);
}

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavSameDocumentReplaceStateRef) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), CreateUrl(kUrlWithMerchantTrustData)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  GURL kMerchantReviewsGURL = CreateUrl(kMerchantReviewsUrl);
  ShowMerchantTrustSidePanel(web_contents(), kMerchantReviewsGURL);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Replace state with anchor.
  ASSERT_TRUE(
      content::ExecJs(web_contents(), "history.replaceState({},'','#ref')"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Check that side panel remains open on replace state.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Check that the AboutThisSite url remains the same.
  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kMerchantReviewsGURL);
}

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavSameDocumentPushState) {
  GURL kGURLWithMerchantTrustData = CreateUrl(kUrlWithMerchantTrustData);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kGURLWithMerchantTrustData));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  ShowMerchantTrustSidePanel(web_contents(), kGURLWithMerchantTrustData);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Push state with new path.
  GURL kUrlMerchantTrustWithPath2 =
      kGURLWithMerchantTrustData.Resolve("/title2.html");
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "history.pushState({},'','title2.html')"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Check that side panel remains open on push state.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Check that the MerchantTrust url isn't changed.

  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kGURLWithMerchantTrustData);
}

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       RemainsClosedOnNonMerchantSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), CreateUrl(kUrlWithoutMerchantTrustData)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  ShowMerchantTrustSidePanel(web_contents(), CreateUrl(kMerchantReviewsUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Close side panel.
  side_panel_coordinator()->Close();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().unified_side_panel()->state() ==
           SidePanel::State::kClosed;
  }));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Check that side panel remains closed on navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), CreateUrl(kUrlWithoutMerchantTrustData)));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       SidePanelEntryUrlHasQueryParams) {
  ShowMerchantTrustSidePanel(web_contents(), CreateUrl(kMerchantReviewsUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  auto view = side_panel_coordinator()
                  ->GetCurrentSidePanelEntryForTesting()
                  ->GetContent();
  auto* side_panel_view = static_cast<WebViewSidePanelView*>(view.get());

  EXPECT_EQ(side_panel_view->GetLastUrlForTesting(),
            CreateUrl(kMerchantReviewsUrl).spec() + "?s=CHROME_SIDE_PANEL");
}
