// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/web_ui_browsertest_util.h"

using ChromeWebUIControllerFactoryBrowserTest = InProcessBrowserTest;

// Verify that if there is a chrome-untrusted:// URLDataSource with the same
// host as a chrome:// WebUI, we serve the right resources and we don't use the
// wrong WebUI object.
IN_PROC_BROWSER_TEST_F(ChromeWebUIControllerFactoryBrowserTest,
                       ChromeUntrustedSameHost) {
  content::AddUntrustedDataSource(browser()->profile(),
                                  chrome::kChromeUIVersionHost);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(base::StrCat({"chrome-untrusted://", chrome::kChromeUIVersionHost,
                         "/title2.html"}))));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(u"Title Of Awesomeness", web_contents->GetTitle());
  EXPECT_FALSE(web_contents->GetWebUI());

  // Check that we can navigate to chrome://version and that it serves the right
  // resources and has a WebUI.
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(base::StrCat({"chrome://", chrome::kChromeUIVersionHost,
                                    "/title2.html"}))));
  EXPECT_EQ(u"About Version", web_contents->GetTitle());
  EXPECT_TRUE(web_contents->GetWebUI());
}

IN_PROC_BROWSER_TEST_F(ChromeWebUIControllerFactoryBrowserTest,
                       NoWebUiNtpInIncognitoProfile) {
  auto* incognito_browser = CreateIncognitoBrowser();
  auto* web_contents =
      incognito_browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      incognito_browser, GURL(chrome::kChromeUINewTabPageURL)));
  EXPECT_FALSE(web_contents->GetWebUI());

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      incognito_browser, GURL(chrome::kChromeUINewTabPageThirdPartyURL)));
  EXPECT_FALSE(web_contents->GetWebUI());
}

IN_PROC_BROWSER_TEST_F(ChromeWebUIControllerFactoryBrowserTest,
                       WebUiNtpInNormalProfile) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageURL)));
  EXPECT_TRUE(web_contents->GetWebUI());

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUINewTabPageThirdPartyURL)));
  EXPECT_TRUE(web_contents->GetWebUI());
}
