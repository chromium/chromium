// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
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
