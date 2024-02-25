// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "content/public/test/browser_test.h"

class PasswordManagerInternalsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    OpenInternalsPage(WindowOpenDisposition::CURRENT_TAB);
  }

  // Navigates to the internals page in a tab specified by |disposition| (and
  // optionally, by |browser|).
  void OpenInternalsPage(WindowOpenDisposition disposition) {
    OpenInternalsPageWithBrowser(browser(), disposition);
  }

  void OpenInternalsPageWithBrowser(Browser* browser,
                                    WindowOpenDisposition disposition) {
    std::string url_string("chrome://");
    url_string += chrome::kChromeUIPasswordManagerInternalsHost;
    ui_test_utils::NavigateToURLWithDisposition(
        browser, GURL(url_string), disposition,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }
};

IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsBrowserTest, LogText) {
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(log_router);
  log_router->ProcessLog("<script> text for testing");
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(RunTestOnWebContents(
      web_contents,
      "password_manager_internals/password_manager_internals_test.js",
      "runMochaTest('PasswordManagerInternals', 'LogText')",
      /*skip_test_loader=*/true));
}

// Test that a single internals page is empty on load.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsBrowserTest, LogEmpty) {
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(log_router);
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(RunTestOnWebContents(
      web_contents,
      "password_manager_internals/password_manager_internals_test.js",
      "runMochaTest('PasswordManagerInternals', 'LogEmpty')",
      /*skip_test_loader=*/true));
}

// Test that a single internals page is flushed on reload.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsBrowserTest,
                       LogEmptyAfterReload) {
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(log_router);
  log_router->ProcessLog("<script> text for testing");
  OpenInternalsPage(WindowOpenDisposition::CURRENT_TAB);  // Reload.
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(RunTestOnWebContents(
      web_contents,
      "password_manager_internals/password_manager_internals_test.js",
      "runMochaTest('PasswordManagerInternals', 'LogEmpty')",
      /*skip_test_loader=*/true));
}

// Test that navigation away from the internals page works OK.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsBrowserTest, NavigateAway) {
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(log_router);
  log_router->ProcessLog("<script> text for testing");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIVersionURL)));
}

// Test that the description is correct in a non-Incognito tab.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsBrowserTest,
                       NonIncognitoDescription) {
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(log_router);
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(RunTestOnWebContents(
      web_contents,
      "password_manager_internals/password_manager_internals_test.js",
      "runMochaTest('PasswordManagerInternals', 'NonIncognitoDescription')",
      /*skip_test_loader=*/true));
}

class PasswordManagerInternalsIncognitoBrowserTest
    : public PasswordManagerInternalsBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PasswordManagerInternalsBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIncognito);
  }
};

// Test that the description is correct in an Incognito tab.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsIncognitoBrowserTest,
                       IncognitoDescription) {
  EXPECT_TRUE(browser()->profile()->IsOffTheRecord());
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  EXPECT_FALSE(log_router);  // There should be no log_router for Incognito.
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(RunTestOnWebContents(
      web_contents,
      "password_manager_internals/password_manager_internals_test.js",
      "runMochaTest('PasswordManagerInternals', 'IncognitoDescription')",
      /*skip_test_loader=*/true));
}
