// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://shortcut-customization. Tests
 * individual polymer components in isolation. To run all tests in a single
 * instance (default, faster):
 * `browser_tests --gtest_filter=ShortcutCustomizationApp*`
 * To run a single test suite such as 'AcceleratorRowTest':
 * browser_tests --gtest_filter=ShortcutCustomizationAppAcceleratorRowTest.All
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ui/base/ui_base_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "ash/constants/ash_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');

var ShortcutCustomizationAppBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://shortcut-customization/';
  }

  get featureList() {
    return {
      enabled: [
        'features::kShortcutCustomizationApp',
        'features::kShortcutCustomization',
        // TODO(b/276493795): Remove jelly and
        // shortcut-customization-jelly after the Jelly experiment is launched.
        'chromeos::features::kJelly',
        'ash::features::kShortcutCustomizationJelly',
      ]
    };
  }
};

const tests = [
  ['AcceleratorEditViewTest', 'accelerator_edit_view_test.js'],
  ['AcceleratorLookupManagerTest', 'accelerator_lookup_manager_test.js'],
  ['AcceleratorViewTest', 'accelerator_view_test.js'],
  ['AcceleratorRowTest', 'accelerator_row_test.js'],
  ['AcceleratorEditDialogTest', 'accelerator_edit_dialog_test.js'],
  ['AcceleratorSubsectionTest', 'accelerator_subsection_test.js'],
  ['BottomNavContentTest', 'bottom_nav_content_test.js'],
  ['FakeShortcutProviderTest', 'fake_shortcut_provider_test.js'],
  ['FakeShortcutSearchHandlerTest', 'fake_shortcut_search_handler_test.js'],
  ['InputKeyTest', 'input_key_test.js'],
  ['RouterTest', 'router_test.js'],
  ['SearchBoxTest', 'search_box_test.js'],
  ['SearchResultRowTest', 'search_result_row_test.js'],
  ['SearchResultBoldingTest', 'search_result_bolding_test.js'],
  ['ShortcutCustomizationApp', 'shortcut_customization_test.js'],
  ['ShortcutSearchHandlerTest', 'shortcut_search_handler_test.js'],
  ['ShortcutsPageTest', 'shortcuts_page_test.js'],
  ['ShortcutUtils', 'shortcut_utils_test.js'],
  ['TextAcceleratorTest', 'text_accelerator_test.js'],
];

tests.forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `ShortcutCustomizationApp${testName}`;
  this[className] = class extends ShortcutCustomizationAppBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://shortcut-customization/test_loader.html` +
          `?module=chromeos/shortcut_customization/${module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}