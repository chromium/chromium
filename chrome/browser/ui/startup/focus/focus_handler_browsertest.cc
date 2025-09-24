// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/focus_handler.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace focus {

class FocusHandlerBrowserTest : public InProcessBrowserTest {
 protected:
  void NavigateToURLInNewTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void NavigateToURLInCurrentTab(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  int GetActiveTabIndex() {
    return browser()->tab_strip_model()->active_index();
  }

  GURL GetActiveTabURL() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetLastCommittedURL();
  }
};

IN_PROC_BROWSER_TEST_F(FocusHandlerBrowserTest,
                       Basic_FocusExistingTab_ExactMatch) {
  const GURL test_url("https://example.com/test");

  // Navigate to a test URL.
  NavigateToURLInCurrentTab(test_url);
  EXPECT_EQ(test_url, GetActiveTabURL());

  // Open another tab. with different URL.
  NavigateToURLInNewTab(GURL("https://other.com"));
  EXPECT_EQ(1, GetActiveTabIndex());

  // Try to focus the first tab using exact match.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, test_url.spec());

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kFocused, result.status);
  EXPECT_EQ(0, GetActiveTabIndex());  // Should have switched back to first tab
  EXPECT_EQ(test_url, GetActiveTabURL());
}

IN_PROC_BROWSER_TEST_F(FocusHandlerBrowserTest, Basic_UrlCanonicalization) {
  const GURL test_url("https://example.com/test");

  // Navigate to URL without trailing slash.
  NavigateToURLInCurrentTab(test_url);
  EXPECT_EQ(test_url, GetActiveTabURL());

  // Open another tab.
  NavigateToURLInNewTab(GURL("https://other.com"));
  EXPECT_EQ(1, GetActiveTabIndex());

  // Try to focus using URL with trailing slash (should be canonicalized).
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "https://example.com/test/");

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kFocused, result.status);
  EXPECT_EQ(0, GetActiveTabIndex());  // Should have found the match
}

IN_PROC_BROWSER_TEST_F(FocusHandlerBrowserTest,
                       Basic_InvalidSelector_ParseError) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "not-a-valid-url");

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kParseError, result.status);
}

IN_PROC_BROWSER_TEST_F(FocusHandlerBrowserTest, PrefixMatch_SingleTab) {
  const GURL test_url("https://example.com/path/to/page");

  // Navigate to a test URL.
  NavigateToURLInCurrentTab(test_url);

  // Open another tab. with different URL.
  NavigateToURLInNewTab(GURL("https://other.com"));
  EXPECT_EQ(1, GetActiveTabIndex());

  // Try to focus using prefix match.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus,
                                 "https://example.com/path/*");

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kFocused, result.status);
  EXPECT_EQ(0, GetActiveTabIndex());  // Should have switched back to first tab
}

IN_PROC_BROWSER_TEST_F(FocusHandlerBrowserTest, NoMatch_ReturnsNoMatch) {
  const GURL test_url("https://example.com/test");

  // Navigate to a test URL.
  NavigateToURLInCurrentTab(test_url);

  // Try to focus a non-existent URL.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "https://nonexistent.com");

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kNoMatch, result.status);
  // Current tab should remain active.
  EXPECT_EQ(test_url, GetActiveTabURL());
}

// Tests for focus handler app activation functionality.
class FocusHandlerWebAppBrowserTest : public web_app::WebAppBrowserTestBase {
 protected:
  webapps::AppId InstallTestApp(const std::string& app_name,
                                const std::string& app_path = "/test_app") {
    const GURL app_url("https://app.site.test" + app_path);
    return InstallPWA(app_url);
  }
};

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_SingleApp) {
  webapps::AppId app_id = InstallTestApp("Test App");

  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(app_browser->is_type_app());

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "app:" + app_id);

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kFocused, result.status);
  EXPECT_TRUE(result.matched_selector.has_value());
  EXPECT_EQ("app:" + app_id, result.matched_selector.value());
}

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_MultipleApps) {
  const GURL app_url1("https://app1.site.test/");
  const GURL app_url2("https://app2.site.test/");
  webapps::AppId app_id1 = InstallPWA(app_url1);
  webapps::AppId app_id2 = InstallPWA(app_url2);

  Browser* app_browser1 =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id1);
  Browser* app_browser2 =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id2);
  ASSERT_TRUE(app_browser1);
  ASSERT_TRUE(app_browser2);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "app:" + app_id1);

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());
  EXPECT_EQ(FocusStatus::kFocused, result.status);

  command_line = base::CommandLine(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "app:" + app_id2);

  result = ProcessFocusRequest(command_line, *browser()->profile());
  EXPECT_EQ(FocusStatus::kFocused, result.status);
}

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_NonExistentApp) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus,
                                 "app:nonexistentappidthatdoesnotexist");

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kNoMatch, result.status);
  EXPECT_FALSE(result.matched_selector.has_value());
}

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_AppNotRunning) {
  webapps::AppId app_id = InstallTestApp("Test App");

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "app:" + app_id);

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kNoMatch, result.status);
}

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_SwitchBetweenTabAndApp) {
  webapps::AppId app_id = InstallTestApp("Test App", "/app");

  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser);

  const GURL tab_url("https://tab.site.test/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), tab_url));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "app:" + app_id);

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());
  EXPECT_EQ(FocusStatus::kFocused, result.status);

  command_line = base::CommandLine(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, tab_url.spec());

  result = ProcessFocusRequest(command_line, *browser()->profile());
  EXPECT_EQ(FocusStatus::kFocused, result.status);
}

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_MultipleSelectorsWithApp) {
  webapps::AppId app_id = InstallTestApp("Test App", "/app");

  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(
      switches::kFocus, "app:nonexistent1,app:" + app_id + ",app:nonexistent2");

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kFocused, result.status);
  EXPECT_TRUE(result.matched_selector.has_value());
  EXPECT_EQ("app:" + app_id, result.matched_selector.value());
}

}  // namespace focus
