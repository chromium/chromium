// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the ABOUT route.
 */

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

/**
 * Test fixture for ABOUT
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsA11yAbout() {}

SettingsA11yAbout.prototype = {
  __proto__: SettingsAccessibilityTest.prototype,

  extraLibraries: SettingsAccessibilityTest.prototype.extraLibraries.concat([
    '../../test_browser_proxy.js',
    '../test_about_page_browser_proxy.js',
  ]),
};

AccessibilityTest.define('SettingsA11yAbout', {
  /** @override */
  name: 'ABOUT',
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptions,
  /** @override */
  setup: function() {
    // Reset to a blank page.
    PolymerTest.clearBody();

    // Set the URL to be that of specific route to load upon injecting
    // settings-ui. Simply calling settings.navigateTo(route) prevents
    // use of mock APIs for fake data.
    window.history.pushState(
        'object or string', 'Test', settings.routes.ABOUT.path);

    if (AccessibilityTest.isChromeOS) {
      const aboutPageProxy = new TestAboutPageBrowserProxy();
      // Regulatory info is added when the image is loaded async.
      // Add a fake string to mimic the image text.
      aboutPageProxy.setRegulatoryInfo('This is fake regulatory info');
    }
    const settingsUi = document.createElement('settings-ui');
    document.body.appendChild(settingsUi);
  },

  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: SettingsAccessibilityTest.violationFilter,
});
