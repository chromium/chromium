// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://scanning.
 * Unified polymer testing suite for scanning app.
 *
 * To run all tests in a single instance (default, faster):
 * `browser_tests --gtest_filter=ScanningApp*`
 *
 * To run each test in a new instance:
 * `browser_tests --run-manual --gtest_filter=ScanningAppBrowserTest.MANUAL_*`
 *
 * To run a single test suite, such as 'ScanApp':
 * `browser_tests --run-manual
 * --gtest_filter=ScanningAppBrowserTest.MANUAL_ScanApp`
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends {PolymerTest}
 */
function ScanningAppBrowserTest() {}

ScanningAppBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://scanning/test_loader.html?module=chromeos/' +
      'scanning/scanning_app_unified_test.js',

  featureList: {enabled: ['chromeos::features::kScanAppMediaLink']},
};

// List of names of suites in unified test to register for individual debugging.
// You must register all suites in unified test here as well for consistency,
// although technically is not necessary.
const debug_suites_list = [
  'ColorModeSelect', 'FileTypeSelect', 'LoadingPage', 'PageSizeSelect',
  'ResolutionSelect', 'ScanApp', 'ScanDoneSection', 'ScannerSelect',
  'ScanPreview', 'ScanToSelect', 'SourceSelect'
];

TEST_F('ScanningAppBrowserTest', 'All', function() {
  assertDeepEquals(
      debug_suites_list, test_suites_list,
      'List of registered tests suites and debug suites do not match.\n' +
          'Did you forget to add your test in debug_suites_list?');
  mocha.run();
});

// Register each suite listed as individual tests for debugging purposes.
for (const suiteName of debug_suites_list) {
  TEST_F('ScanningAppBrowserTest', `MANUAL_${suiteName}`, function() {
    runMochaSuite(suiteName);
  });
}
