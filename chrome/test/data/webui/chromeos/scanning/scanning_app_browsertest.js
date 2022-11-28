// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://scanning. Tests
 * individual polymer components in isolation. To run all tests in a single
 * instance (default, faster):
 * `browser_tests --gtest_filter=ScanningApp*`
 * To run a single test suite such as 'ActionToolbar':
 * browser_tests --gtest_filter=ScanningAppActionToolbar.All
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

this.ScanningAppBrowserTest = class extends PolymerTest {};

const tests = [
  ['ActionToolbar', 'action_toolbar_test.js'],
  ['ColorModeSelect', 'color_mode_select_test.js'],
  ['FileTypeSelect', 'file_type_select_test.js'],
  ['LoadingPage', 'loading_page_test.js'],
  ['MultiPageCheckbox', 'multi_page_checkbox_test.js'],
  ['MultiPageScan', 'multi_page_scan_test.js'],
  ['PageSizeSelect', 'page_size_select_test.js'],
  ['ResolutionSelect', 'resolution_select_test.js'],
  ['ScanApp', 'scanning_app_test.js'],
  ['ScanDoneSection', 'scan_done_section_test.js'],
  ['ScannerSelect', 'scanner_select_test.js'],
  ['ScanPreview', 'scan_preview_test.js'],
  ['ScanToSelect', 'scan_to_select_test.js'],
  ['SourceSelect', 'source_select_test.js'],
];


tests.forEach(test => registerTest(...test));

/*
 * Add a `caseName` to a specific test to disable it i.e. 'DISABLED_All'
 * @param {string} testName
 * @param {string} module
 * @param {string} caseName
 */
function registerTest(testName, module, caseName) {
  const className = `ScanningApp${testName}`;
  this[className] = class extends ScanningAppBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://scanning/test_loader.html` +
          `?module=chromeos/scanning/${module}&host=test`;
    }
  };
  TEST_F(className, caseName || 'All', () => mocha.run());
}
