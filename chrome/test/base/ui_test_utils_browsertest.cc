// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ui_test_utils.h"

#include "base/containers/contains.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

class UITestUtilsBrowserTest : public InProcessBrowserTest {
 public:
  UITestUtilsBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// This is a regression test for crbug.com/1187241, where
// NavigateToURLWithDisposition incorrectly inferred the index in the tab strip
// of the second background tab and waited for the wrong tab to finish loading
// (which never happened).
IN_PROC_BROWSER_TEST_F(UITestUtilsBrowserTest, OpenTwoTabsInBackground) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("a.com", "/title2.html");
  const GURL url3 = embedded_test_server()->GetURL("a.com", "/title3.html");

  NavigateToURLWithDisposition(browser(), url1,
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  NavigateToURLWithDisposition(browser(), url2,
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // This test will pass if this call does not time out, which requires
  //   // NavigateToURLWithDisposition to pick correct tab to wait for.
  NavigateToURLWithDisposition(browser(), url3,
                               WindowOpenDisposition::NEW_BACKGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

IN_PROC_BROWSER_TEST_F(UITestUtilsBrowserTest, FindMatchingBrowsers) {
  BrowserWindowInterface* const regular_browser1 = browser();
  BrowserWindowInterface* const regular_browser2 =
      CreateBrowser(browser()->profile());
  BrowserWindowInterface* const incognito_browser = CreateIncognitoBrowser();

  // All browsers.
  auto all_browsers = ui_test_utils::FindMatchingBrowsers(
      [](BrowserWindowInterface*) { return true; });
  EXPECT_EQ(3u, all_browsers.size());
  EXPECT_TRUE(base::Contains(all_browsers, regular_browser1));
  EXPECT_TRUE(base::Contains(all_browsers, regular_browser2));
  EXPECT_TRUE(base::Contains(all_browsers, incognito_browser));

  // No browsers.
  auto no_browsers = ui_test_utils::FindMatchingBrowsers(
      [](BrowserWindowInterface*) { return false; });
  EXPECT_TRUE(no_browsers.empty());

  // Incognito browsers.
  auto incognito_browsers =
      ui_test_utils::FindMatchingBrowsers([](BrowserWindowInterface* browser) {
        return browser->GetProfile()->IsIncognitoProfile();
      });
  EXPECT_EQ(1u, incognito_browsers.size());
  EXPECT_EQ(incognito_browser, incognito_browsers[0]);

  // Regular browsers.
  auto regular_browsers =
      ui_test_utils::FindMatchingBrowsers([](BrowserWindowInterface* browser) {
        return !browser->GetProfile()->IsIncognitoProfile();
      });
  EXPECT_EQ(2u, regular_browsers.size());
  EXPECT_TRUE(base::Contains(regular_browsers, regular_browser1));
  EXPECT_TRUE(base::Contains(regular_browsers, regular_browser2));
}
