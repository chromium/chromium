// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://shortcut-customization.
 * To run all tests in a single instance (default, faster):
 * `browser_tests --gtest_filter=ShortcutCustomizationApp*`
 *
 * To run each test in a new instance:
 * `browser_tests --run-manual \
 *      --gtest_filter=ShortcutCustomizationAppBrowserTest.MANUAL_*`
 *
 * To run a single test suite, such as 'ShortcutCustomizationApp':
 * `browser_tests --run-manual --gtest_filter= \
 *     ShortcutCustomizationAppBrowserTest.MANUAL_ShortcutCustomizationApp`
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ui/base/ui_base_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends {PolymerTest}
 */
function ShortcutCustomizationAppBrowserTest() {}

ShortcutCustomizationAppBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://shortcut-customization/test_loader.html' +
      '?module=chromeos/shortcut_customization/' +
      'shortcut_customization_unified_test.js&host=test',

  featureList: {enabled: ['features::kShortcutCustomizationApp']},
};

// List of names of suites in unified test to register for individual debugging.
// You must register all suites in unified test here as well for consistency,
// although technically is not necessary.
const debug_suites_list = [
  'AcceleratorEditViewTest',
  'AcceleratorLookupManagerTest',
  'AcceleratorViewTest',
  'AcceleratorRowTest',
  'AcceleratorEditDialogTest',
  'AcceleratorSubsectionTest',
  'FakeShortcutProviderTest',
  'ShortcutCustomizationApp',
];

TEST_F('ShortcutCustomizationAppBrowserTest', 'All', function() {
  assertDeepEquals(
      debug_suites_list, test_suites_list,
      'List of registered tests suites and debug suites do not match.\n' +
          'Did you forget to add your test in debug_suites_list?');
  mocha.run();
});

// Register each suite listed as individual tests for debugging purposes.
for (const suiteName of debug_suites_list) {
  TEST_F(
      'ShortcutCustomizationAppBrowserTest', `MANUAL_${suiteName}`, function() {
        runMochaSuite(suiteName);
      });
}
