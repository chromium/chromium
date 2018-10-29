// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.bluetoothPrivate for testing.
 */
cr.define('settings', function() {
  /**
   * Fake of the chrome.bluetooth API.
   * @param {!Bluetooth} bluetoothApi
   * @constructor
   * @implements {BluetoothPrivate}
   */
  function FakeBluetoothPrivate(bluetoothApi) {
    /** @private {!Bluetooth} */ this.bluetoothApi_ = bluetoothApi;

    /** @type {!Set<string>} */ this.connectedDevices_ = new Set();

    /** @type {!Object<!chrome.bluetoothPrivate.SetPairingResponseOptions>} */
    this.pairingResponses_ = {};
  }

  FakeBluetoothPrivate.prototype = {
    /** @override */
    setAdapterState: function(state, opt_callback) {
      this.bluetoothApi_.setAdapterState(state);
      if (opt_callback)
        opt_callback();
    },

    /** @override */
    setPairingResponse: function(options, opt_callback) {
      this.pairingResponses_[options.device.address] = options;
      if (opt_callback)
        opt_callback();
    },

    /** @override */
    disconnectAll: assertNotReached,

    /** @override */
    forgetDevice: assertNotReached,

    /** @override */
    setDiscoveryFilter: assertNotReached,

    /** @override */
    connect: function(address, opt_callback) {
      let device =
          this.bluetoothApi_.getDeviceForTest(address) || {address: address};
      device = Object.assign({}, device);
      device.paired = true;
      device.connecting = true;
      this.bluetoothApi_.updateDeviceForTest(device);
      if (opt_callback)
        opt_callback(chrome.bluetoothPrivate.ConnectResultType.IN_PROGRESS);
    },

    /** @override */
    pair: assertNotReached,

    /** @type {!FakeChromeEvent} */
    onPairing: new FakeChromeEvent(),
  };

  return {FakeBluetoothPrivate: FakeBluetoothPrivate};
});
