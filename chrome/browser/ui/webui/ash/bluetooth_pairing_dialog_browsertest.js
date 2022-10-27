// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for Bluetooth pairing dialog WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function BluetoothPairingDialogTest() {}

BluetoothPairingDialogTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  typedefCppFixture: 'BluetoothPairingDialogTest',

  /** @override */
  testGenCppIncludes: function() {
    GEN('#include "chrome/browser/ui/webui/ash/' +
        'bluetooth_pairing_dialog_browsertest-inl.h"');
  },

  /** @override */
  testGenPreamble: function() {
    GEN('ShowDialog();');
  },
};

// TODO(crbug.com/1203380)  Disabled for flakiness.
TEST_F('BluetoothPairingDialogTest', 'DISABLED_Basic', function() {
  assertEquals('chrome://bluetooth-pairing/', document.location.href);
});
