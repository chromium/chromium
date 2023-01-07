// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for chrome://bluetooth-internals
 */

GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for BluetoothInternals WebUI testing.
 * @constructor
 * @extends testing.Test
 */
function BluetoothInternalsTest() {}

BluetoothInternalsTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload:
      'chrome://bluetooth-internals/bluetooth_internals.html?module=bluetooth_internals/bluetooth_internals_test.js',

  /** @override */
  isAsync: true,

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],
};

TEST_F('BluetoothInternalsTest', 'Startup_BluetoothInternals', function() {
  // Run all registered tests.
  mocha.run();
});
