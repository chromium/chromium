// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the CROSTINI_SHARED_USB_DEVICES
 * route.
 * Chrome OS only.
 */

GEN_INCLUDE([
  'crostini_accessibility_test.js',
]);

AccessibilityTest.define('CrostiniAccessibilityTest', {
  /** @override */
  name: 'CROSTINI_SHARED_USB_DEVICES',
  /** @override */
  axeOptions: SettingsAccessibilityTest.axeOptions,
  /** @override */
  setup: function() {
    settings.router.navigateTo(settings.routes.CROSTINI_SHARED_USB_DEVICES);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: SettingsAccessibilityTest.violationFilter,
});
