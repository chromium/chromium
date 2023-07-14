// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class NewTabPageBrowserTest : public WebUIMochaBrowserTest {
 protected:
  NewTabPageBrowserTest() {
    set_test_loader_host(chrome::kChromeUINewTabPageHost);
  }
};

using NewTabPageAppTest = NewTabPageBrowserTest;

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, Misc) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Misc')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, OgbThemingRemoveScrimFalse) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest OgbThemingRemoveScrim_false')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, OgbThemingRemoveScrimTrue) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest OgbThemingRemoveScrim_true')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, OgbScrim) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest OgbScrim')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, Theming) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Theming')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, Promo) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Promo')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, Clicks) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Clicks')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, Modules) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Modules')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, V2Modules) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest V2Modules')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, CounterfactualModules) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest CounterfactualModules')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, CustomizeDialog) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest CustomizeDialog')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, CustomizeChromeSidePanel) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest CustomizeChromeSidePanel')");
}

IN_PROC_BROWSER_TEST_F(NewTabPageAppTest, LensUploadDialog) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest LensUploadDialog')");
}
