// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://eche-app.
 */

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const HOST_ORIGIN = 'chrome://eche-app';

var EcheAppUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kEcheSWA']};
  }
};

// Tests that chrome://eche-app goes somewhere instead of
// 404ing or crashing.
TEST_F('EcheAppUIBrowserTest', 'HasChromeSchemeURL', () => {
  assertEquals(document.title, 'Eche');
  assertEquals(document.location.origin, HOST_ORIGIN);
  testDone();
});

// Tests that the implementations of echeapi.d.ts are defined.
TEST_F('EcheAppUIBrowserTest', 'HasDefinedEcheapi', () => {
  chai.assert.isDefined(echeapi.webrtc.registerSignalReceiver);
  chai.assert.isDefined(echeapi.webrtc.registerTabletModeChangedReceiver);
  chai.assert.isDefined(echeapi.webrtc.sendSignal);
  chai.assert.isDefined(echeapi.webrtc.tearDownSignal);
  testDone();
});
