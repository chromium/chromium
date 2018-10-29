// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the MULTIDEVICE_FEATURES route.
 */

// This is only for Chrome OS.
GEN('#if defined(OS_CHROMEOS)');

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

AccessibilityTest.define('SettingsAccessibilityTest', {
  /** @override */
  name: 'MULTIDEVICE_FEATURES_ACCESSIBILITY',
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptions,
  /** @override */
  setup: function() {
    settings.router.navigateTo(settings.routes.MULTIDEVICE_FEATURES);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter:
      Object.assign({}, SettingsAccessibilityTest.violationFilter, {
        // Excuse link without an underline.
        // TODO(https://crbug.com/894602): Remove this exception when settled
        // with UX.
        'link-in-text-block': function(nodeResult) {
          return nodeResult.element.parentElement.id == 'multideviceSubLabel';
        },
      }),
});

GEN('#endif  // defined(OS_CHROMEOS)');
