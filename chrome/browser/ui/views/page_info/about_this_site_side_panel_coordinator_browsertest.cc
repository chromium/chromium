// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/escape.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_info/about_this_site_side_panel.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
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
    SetUpFeatureList();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  GURL CreateUrl(const std::string& host) {
    return https_server_.GetURL(host, "/title1.html");
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
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->side_panel_coordinator();
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  virtual void SetUpFeatureList() {
    feature_list_.InitAndEnableFeature(
        page_info::kPageInfoAboutThisSiteMoreInfo);
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowAndClose) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Test showing a sidepanel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Check that it closes on navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl2)));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Check that navigating to reloading that URL is works fine
  // (See https://crbug.com/1393000).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl2)));
}

// Tests that the ATS Side Panel remains open and updated on same tab
// navigations including refreshes.
class AboutThisSiteKeepSidePanelOpenBrowserTest
    : public AboutThisSiteSidePanelCoordinatorBrowserTest {
  void SetUpFeatureList() override {
    feature_list_.InitWithFeatures(
        {page_info::kPageInfoAboutThisSiteMoreInfo,
         page_info::kPageInfoAboutThisSiteKeepSidePanelOnSameTabNavs},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(AboutThisSiteKeepSidePanelOpenBrowserTest,
                       ShowOnRefresh) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

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

IN_PROC_BROWSER_TEST_F(AboutThisSiteKeepSidePanelOpenBrowserTest,
                       ShowSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

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

  // Check that the diner url was updated.
  std::string kAboutThisSiteRegularUrl2 = base::StringPrintf(
      "https://www.google.com/search?"
      "q=About+%s"
      "&tbm=ilp&ctx=chrome_nav",
      base::EscapeQueryParamValue(kRegularGURL2.spec(), true).c_str());

  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kAboutThisSiteRegularUrl2);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteKeepSidePanelOpenBrowserTest,
                       ShowSameTabNavWithInvalidOrigin) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

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

  // Check that the diner url was updated with the invalid origin but with an
  // empty path.
  std::string kAboutThisSiteInvalidUrl = base::StringPrintf(
      "https://www.google.com/search?"
      "q=About+%s"
      "&tbm=ilp&ctx=chrome_nav",
      base::EscapeQueryParamValue(kInvalidGURL.GetWithEmptyPath().spec(), true)
          .c_str());

  EXPECT_TRUE(side_panel_coordinator()->GetCurrentSidePanelEntryForTesting());
  EXPECT_EQ(side_panel_coordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->GetOpenInNewTabURL(),
            kAboutThisSiteInvalidUrl);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteKeepSidePanelOpenBrowserTest,
                       RemainsClosedOnSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kAboutThisSite);

  // Close side panel.
  side_panel_coordinator()->Close();
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Check that side panel remains closed on navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl2)));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteKeepSidePanelOpenBrowserTest,
                       HistogramEmissionOnSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

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

// TODO(crbug.com/1318000): Cover additional AboutThisSite side panel behavior.
