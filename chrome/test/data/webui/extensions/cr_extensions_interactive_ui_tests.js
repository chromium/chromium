// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Extensions interactive UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');
GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for interactive Polymer Extensions elements.
 * @constructor
 * @extends {PolymerInteractiveUITest}
 */
const CrExtensionsInteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/';
  }

  // The name of the mocha suite. Should be overridden by subclasses.
  get suiteName() {
    return null;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};


/** Test fixture for Sync Page. */
var CrExtensionsOptionsPageTest = class extends CrExtensionsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/extension_options_dialog_test.js';
  }

  /** @override */
  testGenPreamble() {
    GEN('  InstallExtensionWithInPageOptions();');
  }

  /** @override */
  get typedefCppFixture() {
    return 'ExtensionSettingsUIBrowserTest';
  }
};

// Disabled due to flakiness, see https://crbug.com/945654
TEST_F('CrExtensionsOptionsPageTest', 'DISABLED_All', function() {
  mocha.run();
});

var CrExtensionsShortcutInputTest =
    class extends CrExtensionsInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://extensions/test_loader.html?module=extensions/shortcut_input_test.js';
  }

  /** @override */
  get suiteName() {
    return extension_shortcut_input_tests.suiteName;
  }
};

TEST_F('CrExtensionsShortcutInputTest', 'Basic', function() {
  this.runMochaTest(extension_shortcut_input_tests.TestNames.Basic);
});
