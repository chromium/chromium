// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the MANAGE_TTS_SETTINGS route.
 * Chrome OS only.
 */

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
  'settings_accessibility_test.js',
]);

GEN('#include "chromeos/constants/chromeos_features.h"');

// TODO(crbug/950007): refactor this into an OSSettingsAccessibilityTest class
// eslint-disable-next-line no-var
var TtsAccessibilityTest = class extends PolymerTest {
  /** @override */
  get commandLineSwitches() {
    return ['enable-experimental-a11y-features'];
  }

  /** @override */
  get featureList() {
    // Always test with SplitSettings on because the pages are the same in the
    // legacy combined settings and we don't want to test everything twice.
    return {enabled: ['chromeos::features::kSplitSettings']};
  }

  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }
};

AccessibilityTest.define('TtsAccessibilityTest', {
  /** @override */
  name: 'MANAGE_TTS_SETTINGS',
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptions,
  /** @override */
  setup: function() {
    settings.router.navigateTo(settings.routes.MANAGE_TTS_SETTINGS);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: SettingsAccessibilityTest.violationFilter,
});
