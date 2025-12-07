// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_group_home/constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class TabGroupHomeBrowserTest : public WebUIMochaBrowserTest {
 protected:
  TabGroupHomeBrowserTest() { set_test_loader_host(tabs::kTabGroupHomeHost); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{tabs::kTabGroupHome};
};

class TabUrlItemGridTest : public TabGroupHomeBrowserTest {
 protected:
  void RunTestSuite(const std::string& suiteName) {
    TabGroupHomeBrowserTest::RunTest(
        "tab_group_home/tab_url_item_grid_test.js",
        base::StringPrintf("runMochaSuite('%s');", suiteName.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(TabUrlItemGridTest, GridRendering) {
  RunTestSuite("GridRendering");
}

class UrlItemTest : public TabGroupHomeBrowserTest {
 protected:
  void RunTestSuite(const std::string& suiteName) {
    TabGroupHomeBrowserTest::RunTest(
        "tab_group_home/url_item_test.js",
        base::StringPrintf("runMochaSuite('%s');", suiteName.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(UrlItemTest, All) {
  RunTestSuite("UrlItemElementTest");
}
