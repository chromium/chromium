// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://firmware-update.
 * To run all tests in a single instance (default, faster):
 * `browser_tests --gtest_filter=FirmwareUpdateApp*`
 *
 * To run each test in a new instance:
 * `browser_tests --run-manual \
 *      --gtest_filter=FirmwareUpdateAppBrowserTest.MANUAL_*`
 *
 * To run a single test suite, such as 'FirmwareUpdateApp':
 * `browser_tests --run-manual --gtest_filter= \
 *     FirmwareUpdateAppBrowserTest.MANUAL_FirmwareUpdateApp`
 */

GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var FirmwareUpdateAppBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://accessory-update/test_loader.html' +
        '?module=chromeos/firmware_update/' +
        'firmware_update_unified_test.js';
  }
}

// List of names of suites in unified test to register for individual debugging.
// You must register all suites in unified test here as well for consistency,
// although technically is not necessary.
const debug_suites_list = [
  'FakeUpdateControllerTest',
  'FakeUpdateProviderTest',
  'FirmwareUpdateApp',
  'FirmwareUpdateDialog',
  'PeripheralUpdatesListTest',
  'UpdateCardTest',
];

TEST_F('FirmwareUpdateAppBrowserTest', 'All', function() {
  assertDeepEquals(
      debug_suites_list, test_suites_list,
      'List of registered tests suites and debug suites do not match.\n' +
          'Did you forget to add your test in debug_suites_list?');
  mocha.run();
});

// Register each suite listed as individual tests for debugging purposes.
for (const suiteName of debug_suites_list) {
  TEST_F('FirmwareUpdateAppBrowserTest', `MANUAL_${suiteName}`, function() {
    runMochaSuite(suiteName);
  });
}
