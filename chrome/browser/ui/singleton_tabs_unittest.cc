// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/browser_with_test_window_test.h"

namespace {

class SingletonTabsTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    AddTab(browser(), kUrl3);
    AddTab(browser(), kUrl2);
    AddTab(browser(), kUrl1);
  }

  NavigateParams NavigateParamsForTest(
      GURL url,
      WindowOpenDisposition disposition =
          WindowOpenDisposition::SINGLETON_TAB) {
    NavigateParams params(profile(), url,
                          ui::PageTransition::PAGE_TRANSITION_TYPED);
    params.disposition = disposition;
    return params;
  }

  const GURL kUrl1{"http://1.chromium.org/1"};
  const GURL kUrl2{"http://2.chromium.org/2"};
  const GURL kUrl3{"https://3.chromium.org/3"};
};

}  // namespace

TEST_F(SingletonTabsTest, FindsExactMatch) {
  EXPECT_EQ(GetIndexOfExistingTab(browser(), NavigateParamsForTest(kUrl1)), 0);
  EXPECT_EQ(GetIndexOfExistingTab(browser(), NavigateParamsForTest(kUrl2)), 1);
}

TEST_F(SingletonTabsTest, FindsWithDifferentRef) {
  auto params = NavigateParamsForTest(kUrl1.Resolve("/1#ref"));
  EXPECT_EQ(GetIndexOfExistingTab(browser(), params), 0);
}

TEST_F(SingletonTabsTest, DoesNotFindNonSingletonDisposition) {
  auto params =
      NavigateParamsForTest(kUrl1, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_EQ(GetIndexOfExistingTab(browser(), params), -1);
}

TEST_F(SingletonTabsTest, DoesNotFindViewSource) {
  auto params =
      NavigateParamsForTest(GURL("view-source:http://1.chromium.org"));
  EXPECT_EQ(GetIndexOfExistingTab(browser(), params), -1);
}

TEST_F(SingletonTabsTest, DoesNotFindDifferentPath) {
  auto params = NavigateParamsForTest(kUrl1.Resolve("/a"));
  EXPECT_EQ(GetIndexOfExistingTab(browser(), params), -1);
}
