// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test fixture for utility components in //ash/webui/common.
 */

namespace ash {

namespace {

class AshCommonBrowserTest : public WebUIMochaBrowserTest {
 protected:
  void RunTestAtPath(const std::string& testFilePath) {
    auto testPath =
        base::StringPrintf("chromeos/ash_common/%s", testFilePath.c_str());
    WebUIMochaBrowserTest::RunTest(testPath, "mocha.run()");
  }
};

IN_PROC_BROWSER_TEST_F(AshCommonBrowserTest, FakeObservables) {
  RunTestAtPath("fake_observables_test.js");
}

IN_PROC_BROWSER_TEST_F(AshCommonBrowserTest, FakeMethodResolver) {
  RunTestAtPath("fake_method_resolver_test.js");
}

IN_PROC_BROWSER_TEST_F(AshCommonBrowserTest, KeyboardDiagram) {
  RunTestAtPath("keyboard_diagram_test.js");
}

IN_PROC_BROWSER_TEST_F(AshCommonBrowserTest, NavigationSelector) {
  RunTestAtPath("navigation_selector_test.js");
}

IN_PROC_BROWSER_TEST_F(AshCommonBrowserTest, DISABLED_NavigationViewPanel) {
  RunTestAtPath("navigation_view_panel_test.js");
}

IN_PROC_BROWSER_TEST_F(AshCommonBrowserTest, PageToolbar) {
  RunTestAtPath("page_toolbar_test.js");
}

}  // namespace

}  // namespace ash
