// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ThirdPartyNTPUiTest : public InProcessBrowserTest,
                            public InstantTestBase {
 public:
  ThirdPartyNTPUiTest() = default;

  ThirdPartyNTPUiTest(const ThirdPartyNTPUiTest&) = delete;
  ThirdPartyNTPUiTest& operator=(const ThirdPartyNTPUiTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_test_server().Start());
  }
};

// Verifies that Chrome won't steal focus from the Omnibox and focus the tab
// contents instead after navigations that don't leave the NTP (like reloads).
IN_PROC_BROWSER_TEST_F(ThirdPartyNTPUiTest, Reloads) {
  GURL base_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  SetupInstant(browser()->profile(), base_url, ntp_url);

  // Verify that at the start of the test the tab contents has focus.
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Open NTP in |tab1|.
  content::WebContents* tab1;
  {
    content::WebContentsAddedObserver tab1_observer;
    chrome::NewTab(browser());
    tab1 = tab1_observer.GetWebContents();
    ASSERT_TRUE(WaitForLoadStop(tab1));
    EXPECT_EQ(ntp_url, content::EvalJs(tab1, "window.location.href"));
    EXPECT_EQ(1, content::EvalJs(tab1, "history.length"));
  }
  // Verify that the omnibox got focused.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Execute renderer-initiated reload of the NTP.
  {
    content::TestNavigationObserver nav_observer(tab1);
    ASSERT_TRUE(content::ExecJs(tab1, "window.location.reload()"));
    nav_observer.WaitForNavigationFinished();
    ASSERT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(ntp_url, tab1->GetPrimaryMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(1, content::EvalJs(tab1, "history.length"));
  }
  // Verify that the omnibox retained its focus.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Navigate to 3rd-party NTP URL with an appended query string.
  {
    content::TestNavigationObserver nav_observer(tab1);
    ASSERT_TRUE(
        content::ExecJs(tab1, "location.href = location.href + '?foo=bar'"));
    nav_observer.WaitForNavigationFinished();
    ASSERT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(2, content::EvalJs(tab1, "history.length"));
  }
  // Verify that the omnibox still retained its focus.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}
