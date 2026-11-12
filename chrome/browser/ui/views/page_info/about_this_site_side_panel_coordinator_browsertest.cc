// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_info/about_this_site_side_panel.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_info/web_view_side_panel_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/google/core/common/google_switches.h"
#include "components/page_info/core/about_this_site_service.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
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
    https_server_.SetCertHostnames(
        {"a.test", "b.test", "c.test", "127.0.0.1", "www.google.com",
         "customerreviews.google.com", "attacker.storage.googleapis.com",
         "attacker.googleusercontent.com"});
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreGooglePortNumbers);
    InProcessBrowserTest::SetUpCommandLine(command_line);
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
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  SidePanelEntry* GetAboutThisSiteEntryForActiveTab() {
    return SidePanelRegistry::From(browser()->GetActiveTabInterface())
        ->GetEntryForKey(SidePanelEntryKey(SidePanelEntryId::kAboutThisSite));
  }

  bool IsAboutThisSiteSidePanelShowing() {
    return SidePanelUI::From(browser())->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kAboutThisSite));
  }

  content::WebContents* ShowAboutThisSiteSidePanelAndGetWebContents(
      const GURL& url) {
    RegisterAboutThisSiteSidePanel(web_contents(), url);
    auto* entry = GetAboutThisSiteEntryForActiveTab();
    if (!entry) {
      return nullptr;
    }
    entry->CacheView(entry->GetContent());
    auto* side_panel_view =
        static_cast<WebViewSidePanelView*>(entry->CachedView().get());
    if (!side_panel_view) {
      return nullptr;
    }
    ShowAboutThisSiteSidePanel(web_contents(), url);
    return side_panel_view->web_contents();
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  base::test::ScopedFeatureList feature_list_;

 private:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowOnRefresh) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_FALSE(IsAboutThisSiteSidePanelShowing());

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that the side panel remains open on refresh.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_FALSE(IsAboutThisSiteSidePanelShowing());

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that side panel remains open on navigation.
  GURL kRegularGURL2 = CreateUrl(kRegularUrl2);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL2));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that the AboutThisSite url was updated.
  std::string kAboutThisSiteRegularUrl2 = CreateAboutThisSiteUrl(kRegularGURL2);

  EXPECT_TRUE(GetAboutThisSiteEntryForActiveTab());
  EXPECT_EQ(GetAboutThisSiteEntryForActiveTab()->GetOpenInNewTabURL(),
            kAboutThisSiteRegularUrl2);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavRef) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_FALSE(IsAboutThisSiteSidePanelShowing());

  // Test showing a side panel.
  GURL kAboutThisSiteGURL = CreateUrl(kAboutThisSiteUrl);
  ShowAboutThisSiteSidePanel(web_contents(), kAboutThisSiteGURL);
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that side panel remains open on navigation with an anchor.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), kRegularGURL1.Resolve("#ref")));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that the AboutThisSite url remains the same.
  EXPECT_TRUE(GetAboutThisSiteEntryForActiveTab());
  EXPECT_EQ(GetAboutThisSiteEntryForActiveTab()->GetOpenInNewTabURL(),
            kAboutThisSiteGURL);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavSameDocumentPushState) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_FALSE(IsAboutThisSiteSidePanelShowing());

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Push state with new path.
  GURL kRegularGURL1WithPath2 = kRegularGURL1.Resolve("/title2.html");
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "history.pushState({},'','title2.html')"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Check that side panel remains open on push state.
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that the AboutThisSite url was updated.
  std::string kAboutThisSiteRegularUrl1WithPath2 =
      CreateAboutThisSiteUrl(kRegularGURL1WithPath2);

  EXPECT_TRUE(GetAboutThisSiteEntryForActiveTab());
  EXPECT_EQ(GetAboutThisSiteEntryForActiveTab()->GetOpenInNewTabURL(),
            kAboutThisSiteRegularUrl1WithPath2);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavSameDocumentReplaceState) {
  GURL kRegularGURL1 = CreateUrl(kRegularUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kRegularGURL1));
  ASSERT_FALSE(IsAboutThisSiteSidePanelShowing());

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Replace state with new path.
  GURL kRegularGURL1WithPath2 = kRegularGURL1.Resolve("/title2.html");
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "history.replaceState({},'','title2.html')"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Check that side panel remains open on replace state.
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that the AboutThisSite url was updated.
  std::string kAboutThisSiteRegularUrl1WithPath2 =
      CreateAboutThisSiteUrl(kRegularGURL1WithPath2);

  EXPECT_TRUE(GetAboutThisSiteEntryForActiveTab());
  EXPECT_EQ(GetAboutThisSiteEntryForActiveTab()->GetOpenInNewTabURL(),
            kAboutThisSiteRegularUrl1WithPath2);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavSameDocumentReplaceStateRef) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_FALSE(IsAboutThisSiteSidePanelShowing());

  // Test showing a side panel.
  GURL kAboutThisSiteGURL = CreateUrl(kAboutThisSiteUrl);
  ShowAboutThisSiteSidePanel(web_contents(), kAboutThisSiteGURL);
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Replace state with anchor.
  ASSERT_TRUE(
      content::ExecJs(web_contents(), "history.replaceState({},'','#ref')"));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Check that side panel remains open on replace state.
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that the AboutThisSite url remains the same.
  EXPECT_TRUE(GetAboutThisSiteEntryForActiveTab());
  EXPECT_EQ(GetAboutThisSiteEntryForActiveTab()->GetOpenInNewTabURL(),
            kAboutThisSiteGURL);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       ShowSameTabNavWithInvalidOrigin) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_FALSE(IsAboutThisSiteSidePanelShowing());

  // Test showing the side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that side panel remains open on navigation to an invalid url with a
  // path
  GURL kInvalidGURL = CreateUrl(kInvalidUrl);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(kInvalidGURL.spec() + "/index.html")));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that the AboutThisSite url was updated with the invalid origin but
  // with an empty path.
  std::string kAboutThisSiteInvalidUrl =
      CreateAboutThisSiteUrl(kInvalidGURL.GetWithEmptyPath());

  EXPECT_TRUE(GetAboutThisSiteEntryForActiveTab());
  EXPECT_EQ(GetAboutThisSiteEntryForActiveTab()->GetOpenInNewTabURL(),
            kAboutThisSiteInvalidUrl);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       RemainsClosedOnSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_FALSE(IsAboutThisSiteSidePanelShowing());

  // Test showing a side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Close side panel.
  SidePanelUI::From(browser())->Close();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return BrowserView::GetBrowserViewForBrowser(browser())
               ->side_panel()
               ->state() == SidePanel::State::kClosed;
  }));
  EXPECT_FALSE(IsAboutThisSiteSidePanelShowing());

  // Check that side panel remains closed on navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl2)));
  EXPECT_FALSE(IsAboutThisSiteSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       HistogramEmissionOnSameTabNav) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));
  ASSERT_FALSE(IsAboutThisSiteSidePanelShowing());

  // Show side panel.
  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  base::HistogramTester t;

  // Navigate on the same tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl2)));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());

  // Check that the histogram was emitted.
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteInteraction",
                       page_info::AboutThisSiteService::
                           AboutThisSiteInteraction::kSameTabNavigation,
                       1);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       WebContentsModalDialogManagerWired) {
  RegisterAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  auto* entry = GetAboutThisSiteEntryForActiveTab();
  ASSERT_TRUE(entry);
  entry->CacheView(entry->GetContent());
  auto* side_panel_view =
      static_cast<WebViewSidePanelView*>(entry->CachedView().get());
  ASSERT_TRUE(side_panel_view);

  ShowAboutThisSiteSidePanel(web_contents(), CreateUrl(kAboutThisSiteUrl));
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());
  auto* side_panel_contents = side_panel_view->web_contents();
  ASSERT_TRUE(side_panel_contents);

  auto* dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          side_panel_contents);
  ASSERT_TRUE(dialog_manager);
  EXPECT_EQ(dialog_manager->delegate(), side_panel_view);

  // Verify safe teardown when the live side panel view hierarchy is closed.
  SidePanelUI::From(browser())->DisableAnimationsForTesting();
  SidePanelUI::From(browser())->Close();
  EXPECT_FALSE(IsAboutThisSiteSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       AllowsRedirectToGoogleDomainInPanel) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));

  GURL target_url =
      https_server().GetURL("customerreviews.google.com", "/title2.html?q=1");
  GURL redirect_url = https_server().GetURL(
      "www.google.com", "/server-redirect?" + target_url.spec());
  GURL expected_url = net::AppendOrReplaceQueryParameter(
      target_url, page_info::AboutThisSiteRenderModeParameterName,
      page_info::AboutThisSiteRenderModeParameterValue);

  content::WebContents* side_panel_contents =
      ShowAboutThisSiteSidePanelAndGetWebContents(redirect_url);
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());
  ASSERT_TRUE(side_panel_contents);
  EXPECT_TRUE(content::WaitForLoadStop(side_panel_contents));

  EXPECT_EQ(browser()->GetTabStripModel()->count(), 1);
  EXPECT_EQ(side_panel_contents->GetLastCommittedURL(), expected_url);
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       BlocksRedirectToNonGoogleDomainInPanel) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));

  for (const char* untrusted_host :
       {"attacker.storage.googleapis.com", "attacker.googleusercontent.com",
        "b.test"}) {
    GURL target_url = https_server().GetURL(untrusted_host, "/title2.html?q=1");
    GURL redirect_url = https_server().GetURL(
        "www.google.com", "/server-redirect?" + target_url.spec());
    GURL expected_url = net::AppendOrReplaceQueryParameter(
        target_url, page_info::AboutThisSiteRenderModeParameterName,
        page_info::AboutThisSiteRenderModeParameterValue);

    int initial_tab_count = browser()->GetTabStripModel()->count();
    ui_test_utils::TabAddedWaiter tab_waiter(browser());
    ShowAboutThisSiteSidePanel(web_contents(), redirect_url);
    content::WebContents* new_tab = tab_waiter.Wait();
    EXPECT_TRUE(content::WaitForLoadStop(new_tab));

    EXPECT_EQ(browser()->GetTabStripModel()->count(), initial_tab_count + 1);
    EXPECT_EQ(new_tab->GetLastCommittedURL(), expected_url);

    // Switch back to the original tab for the next iteration.
    browser()->GetTabStripModel()->ActivateTabAt(0);
  }
}

IN_PROC_BROWSER_TEST_F(AboutThisSiteSidePanelCoordinatorBrowserTest,
                       BlocksPushStateRedirectToNonGoogleDomainInPanel) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), CreateUrl(kRegularUrl1)));

  GURL panel_url = https_server().GetURL("www.google.com", "/title1.html");
  content::WebContents* side_panel_contents =
      ShowAboutThisSiteSidePanelAndGetWebContents(panel_url);
  EXPECT_TRUE(IsAboutThisSiteSidePanelShowing());
  ASSERT_TRUE(side_panel_contents);
  EXPECT_TRUE(content::WaitForLoadStop(side_panel_contents));

  GURL target_url = https_server().GetURL("attacker.storage.googleapis.com",
                                          "/title2.html");
  std::string redirect_path = "/server-redirect?" + target_url.spec();

  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  ASSERT_TRUE(content::ExecJs(
      side_panel_contents,
      content::JsReplace(
          "history.pushState({}, '', $1); location.href = location.href;",
          redirect_path)));
  content::WebContents* new_tab = tab_waiter.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(new_tab));

  EXPECT_EQ(browser()->GetTabStripModel()->count(), 2);
  EXPECT_EQ(new_tab->GetLastCommittedURL(), target_url);
}

// TODO(crbug.com/40222735): Cover additional AboutThisSite side panel behavior.
