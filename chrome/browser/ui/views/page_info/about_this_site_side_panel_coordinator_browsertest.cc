// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/page_info/about_this_site_side_panel.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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

 private:
  virtual void SetUpFeatureList() {
    feature_list_.InitWithFeatures(
        {features::kUnifiedSidePanel, page_info::kPageInfoAboutThisSiteMoreInfo,
         page_info::kPageInfoAboutThisSiteDescriptionPlaceholder},
        {});
  }

  base::test::ScopedFeatureList feature_list_;
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

// TODO(crbug.com/1318000): Cover additional AboutThisSite side panel behavior.
