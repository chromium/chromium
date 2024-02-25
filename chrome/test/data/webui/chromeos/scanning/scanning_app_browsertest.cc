// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanning/url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

/**
 * @fileoverview Test suite for chrome://scanning. Tests
 * individual polymer components in isolation. To run all tests in a single
 * instance (default, faster):
 * `browser_tests --gtest_filter=ScanningApp*`
 * To run a single test suite such as 'ActionToolbar':
 * browser_tests --gtest_filter=ScanningAppActionToolbar.All
 */

namespace ash {
namespace {

class ScanningAppBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ScanningAppBrowserTest() {
    set_test_loader_host(::ash::kChromeUIScanningAppHost);
  }
};

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, ActionToolbar) {
  RunTest("chromeos/scanning/action_toolbar_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, ColorModeSelect) {
  RunTest("chromeos/scanning/color_mode_select_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, FileTypeSelect) {
  RunTest("chromeos/scanning/file_type_select_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, LoadingPage) {
  RunTest("chromeos/scanning/loading_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, MultiPageCheckbox) {
  RunTest("chromeos/scanning/multi_page_checkbox_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, MultiPageScan) {
  RunTest("chromeos/scanning/multi_page_scan_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, PageSizeSelect) {
  RunTest("chromeos/scanning/page_size_select_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, ResolutionSelect) {
  RunTest("chromeos/scanning/resolution_select_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, ScanApp) {
  RunTest("chromeos/scanning/scanning_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, ScanDoneSection) {
  RunTest("chromeos/scanning/scan_done_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, ScannerSelect) {
  RunTest("chromeos/scanning/scanner_select_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, ScanPreview) {
  RunTest("chromeos/scanning/scan_preview_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, ScanToSelect) {
  RunTest("chromeos/scanning/scan_to_select_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ScanningAppBrowserTest, SourceSelect) {
  RunTest("chromeos/scanning/source_select_test.js", "mocha.run()");
}

}  // namespace

}  // namespace ash
