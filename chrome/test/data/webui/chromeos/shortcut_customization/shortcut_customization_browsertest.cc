// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test suite for chrome://shortcut-customization.
 */

namespace ash {

namespace {

class ShortcutCustomizationAppBrowserTest : public WebUIMochaBrowserTest {
 public:
  ShortcutCustomizationAppBrowserTest() {
    set_test_loader_host(::ash::kChromeUIShortcutCustomizationAppHost);
  }

 protected:
  void RunTestAtPath(const std::string& testFilePath) {
    auto testPath = base::StringPrintf("chromeos/shortcut_customization/%s",
                                       testFilePath.c_str());
    WebUIMochaBrowserTest::RunTest(testPath, "mocha.run()");
  }
};

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       AcceleratorEditViewTest) {
  RunTestAtPath("accelerator_edit_view_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       AcceleratorLookupManagerTest) {
  RunTestAtPath("accelerator_lookup_manager_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       AcceleratorRowTest) {
  RunTestAtPath("accelerator_row_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       AcceleratorEditDialogTest) {
  RunTestAtPath("accelerator_edit_dialog_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       AcceleratorSubsectionTest) {
  RunTestAtPath("accelerator_subsection_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       BottomNavContentTest) {
  RunTestAtPath("bottom_nav_content_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       FakeShortcutProviderTest) {
  RunTestAtPath("fake_shortcut_provider_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       FakeShortcutSearchHandlerTest) {
  RunTestAtPath("fake_shortcut_search_handler_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest, RouterTest) {
  RunTestAtPath("router_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest, SearchBoxTest) {
  RunTestAtPath("search_box_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       SearchResultRowTest) {
  RunTestAtPath("search_result_row_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       SearchResultBoldingTest) {
  RunTestAtPath("search_result_bolding_test.js");
}

// TODO(crbug.com/369506934): De-flake and re-enable
IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       DISABLED_ShortcutCustomizationApp) {
  RunTestAtPath("shortcut_customization_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       ShortcutSearchHandlerTest) {
  RunTestAtPath("shortcut_search_handler_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest, ShortcutsPageTest) {
  RunTestAtPath("shortcuts_page_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest, ShortcutUtils) {
  RunTestAtPath("shortcut_utils_test.js");
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationAppBrowserTest,
                       TextAcceleratorTest) {
  RunTestAtPath("text_accelerator_test.js");
}

}  // namespace

}  // namespace ash
