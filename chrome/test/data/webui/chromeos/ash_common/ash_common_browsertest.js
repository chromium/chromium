// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test fixture for utility components in //ash/webui/common.
 *
 * To run all tests in a single instance (default, faster):
 * `browser_tests --gtest_filter=AshCommon*``
 *
 * To run each test in a new instance:
 * `browser_tests --run-manual --gtest_filter=AshCommon.MANUAL_*``
 *
 * To run a single test suite, such as 'FakeObservables':
 * `browser_tests --run-manual --gtest_filter=AshCommon.MANUAL_FakeObservables`
 *
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const testSuites = 'chromeos/ash_common/ash_common_unified_test.js';

this['AshCommon'] = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return `chrome://webui-test/test_loader.html?module=${
        testSuites}&host=test`;
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

// List of names of suites in unified test to register for individual debugging.
// You must register all suites in unified test here as well for consistency,
// although technically is not necessary.
const debug_suites_list = [
  'FakeObservables',
  'FakeMethodResolver',
  'KeyboardDiagram',
  'NavigationSelector',
  'NavigationViewPanel',
  'PageToolbar',
];

TEST_F('AshCommon', 'BrowserTest', function() {
  assertDeepEquals(
      debug_suites_list, test_suites_list,
      'List of registered tests suites and debug suites do not match.\n' +
          'Did you forget to add your test in debug_suites_list?');
  mocha.run();
});

// Register each suite listed as individual tests for debugging purposes.
for (const suiteName of debug_suites_list) {
  TEST_F('AshCommon', `MANUAL_${suiteName}`, function() {
    runMochaSuite(suiteName);
  });
}
