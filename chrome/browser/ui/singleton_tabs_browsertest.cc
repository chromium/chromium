// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/singleton_tabs.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace {

class SingletonTabsTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // InProcessBrowserTest starts with one preexisting tab at GURL(about:blank)
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), kUrl1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), kUrl2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), kUrl3, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  NavigateParams NavigateParamsForTest(
      GURL url,
      WindowOpenDisposition disposition =
          WindowOpenDisposition::SINGLETON_TAB) {
    NavigateParams params(browser()->profile(), url,
                          ui::PageTransition::PAGE_TRANSITION_TYPED);
    params.disposition = disposition;
    return params;
  }

  const GURL kUrl1{"http://1.chromium.org/1"};
  const GURL kUrl2{"http://2.chromium.org/2"};
  const GURL kUrl3{"https://3.chromium.org/3"};
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SingletonTabsTest, FindsExactMatch) {
  EXPECT_EQ(GetIndexOfExistingTab(browser(), NavigateParamsForTest(kUrl1)), 1);
  EXPECT_EQ(GetIndexOfExistingTab(browser(), NavigateParamsForTest(kUrl2)), 2);
}

IN_PROC_BROWSER_TEST_F(SingletonTabsTest, FindsWithDifferentRef) {
  auto params = NavigateParamsForTest(kUrl1.Resolve("/1#ref"));
  EXPECT_EQ(GetIndexOfExistingTab(browser(), params), 1);
}

IN_PROC_BROWSER_TEST_F(SingletonTabsTest, DoesNotFindNonSingletonDisposition) {
  auto params =
      NavigateParamsForTest(kUrl1, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_EQ(GetIndexOfExistingTab(browser(), params), -1);
}

IN_PROC_BROWSER_TEST_F(SingletonTabsTest, DoesNotFindViewSource) {
  auto params =
      NavigateParamsForTest(GURL("view-source:http://1.chromium.org"));
  EXPECT_EQ(GetIndexOfExistingTab(browser(), params), -1);
}

IN_PROC_BROWSER_TEST_F(SingletonTabsTest, DoesNotFindDifferentPath) {
  auto params = NavigateParamsForTest(kUrl1.Resolve("/a"));
  EXPECT_EQ(GetIndexOfExistingTab(browser(), params), -1);
}
