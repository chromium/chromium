// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/strings/escape.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/page_info/merchant_trust_side_panel.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_info/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {
const char kRegularUrl1[] = "a.test";
const char kRegularUrl2[] = "b.test";
const char kMerchantReviewsUrl[] = "reviews.test";
}  // namespace

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
  }

  GURL CreateUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
  }

  std::string CreateMerchantTrustUrl(const GURL& url) {
    return base::StringPrintf(
        "https://www.google.com/search?"
        "q=Merchant+trust+%s"
        "&tbm=ilp&ctx=chrome_nav",
        base::EscapeQueryParamValue(url.spec(), true).c_str());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return browser()->GetFeatures().side_panel_coordinator();
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       ShowOnRefresh) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  ShowMerchantTrustSidePanel(web_contents(), CreateUrl(kMerchantReviewsUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Check that the side panel remains open on refresh.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);
}

// TODO(crbug.com/378671877): Add tests for same tab navigations.

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavRef) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  GURL kMerchantTrustGURL = CreateUrl(kMerchantReviewsUrl);
  ShowMerchantTrustSidePanel(web_contents(), kMerchantTrustGURL);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Check that side panel remains open on navigation with an anchor.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kRegularGURL1.Resolve("#ref")));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kMerchantTrust);

  // Check that the MerchantTrust url remains the same.
  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kMerchantTrustGURL);
}

IN_PROC_BROWSER_TEST_F(MerchantTrustSidePanelCoordinatorBrowserTest,
                       RemainsClosedOnSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl2)));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
}
