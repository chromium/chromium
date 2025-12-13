// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/focus_handler.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
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
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
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

IN_PROC_BROWSER_TEST_F(FocusHandlerBrowserTest, IncognitoIsolation_NoMatch) {
  const GURL test_url("https://example.com/secret");

  // Create an incognito browser and navigate to a test URL
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  ui_test_utils::NavigateToURLWithDisposition(
      incognito_browser, test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verify the incognito tab exists
  EXPECT_EQ(test_url, incognito_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());

  // Try to focus from regular profile - should NOT find incognito tab
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "https://example.com/*");

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  // Should not match incognito tabs from regular profile
  EXPECT_EQ(FocusStatus::kNoMatch, result.status);

  // Incognito browser should still be the active one (unchanged)
  EXPECT_EQ(test_url, incognito_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(FocusHandlerBrowserTest,
                       IncognitoIsolation_WithinProfile) {
  const GURL test_url("https://example.com/shared");

  // Navigate regular browser to test URL
  NavigateToURLInCurrentTab(test_url);

  // Create incognito browser and navigate to same URL
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  ui_test_utils::NavigateToURLWithDisposition(
      incognito_browser, test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Try to focus from incognito profile context
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, "https://example.com/*");

  // Focus should work within incognito profile but only find incognito tabs
  FocusResult result =
      ProcessFocusRequest(command_line, *incognito_browser->profile());

  EXPECT_EQ(FocusStatus::kFocused, result.status);

  // Should focus the incognito tab, not the regular browser tab
  EXPECT_EQ(test_url, incognito_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetLastCommittedURL());
}

// Tests for focus handler app activation functionality.
class FocusHandlerWebAppBrowserTest : public web_app::WebAppBrowserTestBase {
 protected:
  webapps::AppId InstallTestApp(const std::string& app_name,
                                const std::string& app_path = "/test_app") {
    const GURL app_url("https://app.site.test" + app_path);
    return InstallPWA(app_url);
  }

  // Helper to get the manifest ID from an installed app
  std::string GetManifestIdForApp(const webapps::AppId& app_id) {
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForTest(browser()->profile());
    EXPECT_TRUE(provider);
    const web_app::WebApp* web_app =
        provider->registrar_unsafe().GetAppById(app_id);
    EXPECT_TRUE(web_app);
    return web_app->manifest_id().spec();
  }
};

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_SingleApp) {
  webapps::AppId app_id = InstallTestApp("Test App");
  std::string manifest_id = GetManifestIdForApp(app_id);

  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser);
  ASSERT_TRUE(app_browser->is_type_app());

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, manifest_id);

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kFocused, result.status);
  EXPECT_TRUE(result.matched_selector.has_value());
  EXPECT_EQ(manifest_id, result.matched_selector.value());
}

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_MultipleApps) {
  const GURL app_url1("https://app1.site.test/");
  const GURL app_url2("https://app2.site.test/");
  webapps::AppId app_id1 = InstallPWA(app_url1);
  webapps::AppId app_id2 = InstallPWA(app_url2);
  std::string manifest_id1 = GetManifestIdForApp(app_id1);
  std::string manifest_id2 = GetManifestIdForApp(app_id2);

  Browser* app_browser1 =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id1);
  Browser* app_browser2 =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id2);
  ASSERT_TRUE(app_browser1);
  ASSERT_TRUE(app_browser2);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, manifest_id1);

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());
  EXPECT_EQ(FocusStatus::kFocused, result.status);

  command_line = base::CommandLine(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, manifest_id2);

  result = ProcessFocusRequest(command_line, *browser()->profile());
  EXPECT_EQ(FocusStatus::kFocused, result.status);
}

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_NonExistentApp) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus,
                                 "https://nonexistent.app.test/");

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kNoMatch, result.status);
  EXPECT_FALSE(result.matched_selector.has_value());
}

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_AppNotRunning) {
  webapps::AppId app_id = InstallTestApp("Test App");
  std::string manifest_id = GetManifestIdForApp(app_id);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, manifest_id);

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kNoMatch, result.status);
}

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_SwitchBetweenTabAndApp) {
  webapps::AppId app_id = InstallTestApp("Test App", "/app");
  std::string manifest_id = GetManifestIdForApp(app_id);

  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser);

  const GURL tab_url("https://tab.site.test/");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), tab_url));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, manifest_id);

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
  std::string manifest_id = GetManifestIdForApp(app_id);

  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus,
                                 "https://nonexistent1.test/," + manifest_id +
                                     ",https://nonexistent2.test/");

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kFocused, result.status);
  EXPECT_TRUE(result.matched_selector.has_value());
  EXPECT_EQ(manifest_id, result.matched_selector.value());
}

IN_PROC_BROWSER_TEST_F(FocusHandlerWebAppBrowserTest,
                       FocusAppWindow_AsyncBrowserClose) {
  webapps::AppId app_id = InstallTestApp("Test App", "/app");
  std::string manifest_id = GetManifestIdForApp(app_id);

  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser);

  // Trigger async browser close. The browser may or may not be immediately
  // marked as closing depending on the platform, but the close process has
  // started.
  chrome::CloseWindow(app_browser);

  // Try to focus the closing browser. This tests that the focus handler
  // gracefully handles browsers that are in the process of closing, whether
  // they're filtered out by the iterator (is_delete_scheduled) or not.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, manifest_id);

  FocusResult result = ProcessFocusRequest(command_line, *browser()->profile());

  // Should return NoMatch - either because the browser is filtered out by
  // is_delete_scheduled(), or because it's no longer in a valid state to
  // be focused.
  EXPECT_EQ(FocusStatus::kNoMatch, result.status);
}

// Tests for focus result file writing functionality.
IN_PROC_BROWSER_TEST_F(FocusHandlerBrowserTest,
                       ResultFile_WrittenForNormalProfile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL test_url("https://example.com/test");
  NavigateToURLInCurrentTab(test_url);

  // Set up command line with focus and result file switches.
  base::FilePath result_file = temp_dir.GetPath().AppendASCII("result.json");
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, test_url.spec());
  command_line.AppendSwitchPath(switches::kFocusResultFile, result_file);

  // Process focus request with result file writing.
  FocusResult result =
      ProcessFocusRequestWithResultFile(command_line, *browser()->profile());

  EXPECT_EQ(FocusStatus::kFocused, result.status);

  // Wait for async file write to complete.
  // The file write is posted to ThreadPool, so we need to flush those tasks.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Verify the result file was written for normal profile.
  ASSERT_TRUE(base::PathExists(result_file));

  std::string file_content;
  ASSERT_TRUE(base::ReadFileToString(result_file, &file_content));
  EXPECT_FALSE(file_content.empty());
  EXPECT_NE(file_content.find("\"status\":\"focused\""), std::string::npos);
  EXPECT_NE(file_content.find("\"exit_code\":0"), std::string::npos);
}

IN_PROC_BROWSER_TEST_F(FocusHandlerBrowserTest,
                       ResultFile_NotWrittenForOTRProfile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const GURL test_url("https://example.com/secret");

  // Create incognito browser and navigate to test URL.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  ui_test_utils::NavigateToURLWithDisposition(
      incognito_browser, test_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Set up command line with focus and result file switches.
  base::FilePath result_file = temp_dir.GetPath().AppendASCII("result.json");
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kFocus, test_url.spec());
  command_line.AppendSwitchPath(switches::kFocusResultFile, result_file);

  // Process focus request with result file writing on OTR profile.
  FocusResult result = ProcessFocusRequestWithResultFile(
      command_line, *incognito_browser->profile());

  EXPECT_EQ(FocusStatus::kFocused, result.status);

  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Verify the result file was NOT written for OTR profile.
  // The file write is skipped synchronously for OTR profiles,
  // so no async task is even posted.
  EXPECT_FALSE(base::PathExists(result_file));
}

}  // namespace focus
