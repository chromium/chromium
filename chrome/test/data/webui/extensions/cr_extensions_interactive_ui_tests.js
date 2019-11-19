// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Extensions interactive UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);
GEN('#include "chrome/browser/ui/webui/extensions/' +
    'extension_settings_browsertest.h"');
GEN('#include "services/network/public/cpp/features.h"');

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

  /** @override */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
  }

  /** @override */
  get featureList() {
    return {enabled: ['network::features::kOutOfBlinkCors']};
  }
};


/** Test fixture for Sync Page. */
// eslint-disable-next-line no-var
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
