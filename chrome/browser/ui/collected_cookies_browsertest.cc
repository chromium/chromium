// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class CollectedCookiesTest : public DialogBrowserTest {
 public:
  CollectedCookiesTest() {}

  CollectedCookiesTest(const CollectedCookiesTest&) = delete;
  CollectedCookiesTest& operator=(const CollectedCookiesTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Web modal dialogs' bounds may exceed the display's work area.
    // https://crbug.com/893292.
    set_should_verify_dialog_bounds(false);

    ASSERT_TRUE(embedded_test_server()->Start());

    // Disable cookies.
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

    // Load a page with cookies.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/cookie1.html")));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TabDialogs::FromWebContents(web_contents)->ShowCollectedCookies();
  }
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(CollectedCookiesTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

// If this crashes on Windows, use http://crbug.com/79331
IN_PROC_BROWSER_TEST_F(CollectedCookiesTest, DoubleDisplay) {
  ShowUi(std::string());

  // Click on the info link a second time.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  TabDialogs::FromWebContents(web_contents)->ShowCollectedCookies();
}

// If this crashes on Windows, use http://crbug.com/79331
IN_PROC_BROWSER_TEST_F(CollectedCookiesTest, NavigateAway) {
  ShowUi(std::string());

  // Navigate to another page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/cookie2.html")));
}
