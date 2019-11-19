// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://help-app.
 */

GEN('#include "chromeos/constants/chromeos_features.h"');

const HOST_ORIGIN = 'chrome://help-app';
const GUEST_ORIGIN = 'chrome://help-app-guest';

var HelpAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kHelpAppV2']};
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }
};

// Tests that chrome://help-app goes somewhere instead of 404ing or crashing.
TEST_F('HelpAppUIBrowserTest', 'HasChromeSchemeURL', () => {
  const guest = document.querySelector('iframe');

  assertEquals(document.location.origin, HOST_ORIGIN);
  assertEquals(guest.src, GUEST_ORIGIN + "/app.html");
});
