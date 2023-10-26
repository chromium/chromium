// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Bluetooth pairing dialog.
 */
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var BluetoothPairingDialogBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://bluetooth-pairing/test_loader.html?module=chromeos/bluetooth_pairing_test.js';
  }
};

TEST_F('BluetoothPairingDialogBrowserTest', 'All', function() {
  mocha.run();
});
