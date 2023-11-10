// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test fixture for utility components in //ash/webui/common.
 *
 * To run all tests:
 * `browser_tests --gtest_filter=AshCommonBrowserTest*``
 *
 */

GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

this['AshCommon'] = class extends PolymerTest {
  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

// List of names of suites as well as their corresponding module. To disable a
// test suite add 'DISABLED_All' after the module path.
// Ex. ['FakeObservables', 'fake_observables_test.js', 'DISABLED_All'],
const tests = [
  ['FakeObservables', 'fake_observables_test.js'],
  ['FakeMethodResolver', 'fake_method_resolver_test.js'],
  ['KeyboardDiagram', 'keyboard_diagram_test.js'],
  ['NavigationSelector', 'navigation_selector_test.js'],
  ['NavigationViewPanel', 'navigation_view_panel_test.js'],
  ['PageToolbar', 'page_toolbar_test.js'],
];

for (const [suiteName, module, caseName] of tests) {
  const className = `AshCommonBrowserTest_${suiteName}`;
  this[className] = class extends AshCommon {
    /** @override */
    get browsePreload() {
      return `chrome://webui-test/test_loader.html?module=chromeos/ash_common/${
          module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
