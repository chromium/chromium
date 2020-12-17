// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the GOOGLE_ASSISTANT route.
 * Chrome OS only.
 */

// OSSettingsAccessibilityTest fixture.
GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
  'os_settings_accessibility_test.js',
]);

GEN('#include "content/public/test/browser_test.h"');

// eslint-disable-next-line no-var
var GoogleAssistantA11yTest = class extends Polymer2DeprecatedTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }
};

AccessibilityTest.define('GoogleAssistantA11yTest', {
  /** @override */
  name: 'GOOGLE_ASSISTANT',
  /** @override */
  axeOptions: OSSettingsAccessibilityTest.axeOptions,
  /** @override */
  violationFilter: OSSettingsAccessibilityTest.violationFilter,

  /** @override */
  setup: function() {
    settings.Router.getInstance().navigateTo(settings.routes.GOOGLE_ASSISTANT);
    Polymer.dom.flush();
  },

  /** @override */
  tests: {'Accessible with No Changes': function() {}},
});
