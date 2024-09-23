// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/strings/escape.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/page_info/about_this_site_side_panel.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
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
const char kInvalidUrl[] = "127.0.0.1";
const char kAboutThisSiteUrl[] = "c.test";
}  // namespace

class AboutThisSiteSidePanelCoordinatorBrowserTest
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

  std::string CreateAboutThisSiteUrl(const GURL& url) {
    return base::StringPrintf(
        "https://www.google.com/search?"
        "q=About+%s"
        "&tbm=ilp&ctx=chrome_nav",
        base::EscapeQueryParamValue(url.spec(), true).c_str());
  }

  page_info::proto::SiteInfo CreateSiteInfo() {
    page_info::proto::SiteInfo info;
    info.mutable_more_about()->set_url(CreateUrl(kAboutThisSiteUrl).spec());
    return info;
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

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowOnRefresh) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that the side panel remains open on refresh.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that side panel remains open on navigation.
  GURL kRegularGURL2 = CreateUrl(kRegularUrl2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL2));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that the AboutThisSite url was updated.
  std::string kAboutThisSiteRegularUrl2 = CreateAboutThisSiteUrl(kRegularGURL2);

  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kAboutThisSiteRegularUrl2);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavRef) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  GURL kAboutThisSiteGURL = CreateUrl(kAboutThisSiteUrl);
  ShowAboutThisSiteSidePanel(web_contents(), kAboutThisSiteGURL);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that side panel remains open on navigation with an anchor.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kRegularGURL1.Resolve("#ref")));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that the AboutThisSite url remains the same.
  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kAboutThisSiteGURL);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavSameDocumentPushState) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Push state with new path.
  GURL kRegularGURL1WithPath2 = kRegularGURL1.Resolve("/title2.html");
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "history.pushState({},'','title2.html')"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Check that side panel remains open on push state.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that the AboutThisSite url was updated.
  std::string kAboutThisSiteRegularUrl1WithPath2 =
      CreateAboutThisSiteUrl(kRegularGURL1WithPath2);

  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kAboutThisSiteRegularUrl1WithPath2);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavSameDocumentReplaceState) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Replace state with new path.
  GURL kRegularGURL1WithPath2 = kRegularGURL1.Resolve("/title2.html");
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "history.replaceState({},'','title2.html')"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Check that side panel remains open on replace state.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that the AboutThisSite url was updated.
  std::string kAboutThisSiteRegularUrl1WithPath2 =
      CreateAboutThisSiteUrl(kRegularGURL1WithPath2);

  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kAboutThisSiteRegularUrl1WithPath2);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavSameDocumentReplaceStateRef) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  GURL kAboutThisSiteGURL = CreateUrl(kAboutThisSiteUrl);
  ShowAboutThisSiteSidePanel(web_contents(), kAboutThisSiteGURL);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Replace state with anchor.
  ASSERT_TRUE(
      content::ExecJs(web_contents(), "history.replaceState({},'','#ref')"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Check that side panel remains open on replace state.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that the AboutThisSite url remains the same.
  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kAboutThisSiteGURL);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavWithInvalidOrigin) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing the side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that side panel remains open on navigation to an invalid url with a
  // path
  GURL kInvalidGURL = CreateUrl(kInvalidUrl);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(kInvalidGURL.spec() + "/index.html")));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that the AboutThisSite url was updated with the invalid origin but
  // with an empty path.
  std::string kAboutThisSiteInvalidUrl =
      CreateAboutThisSiteUrl(kInvalidGURL.GetWithEmptyPath());

  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kAboutThisSiteInvalidUrl);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       RemainsClosedOnSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Close side panel.
  side_panel_coordinator()->Close();
  SidePanelWaiter(side_panel_coordinator()).WaitForSidePanelClose();
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Check that side panel remains closed on navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl2)));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       HistogramEmissionOnSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), std::nullopt);

  // Show side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  base::HistogramTester t;

  // Navigate on the same tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl2)));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that the histogram was emitted.
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteInteraction",
                       page_info::AboutThisSiteService::
                           AboutThisSiteInteraction::kSameTabNavigation,
                       1);
}

// TODO(crbug.com/40222735): Cover additional AboutThisSite side panel behavior.
