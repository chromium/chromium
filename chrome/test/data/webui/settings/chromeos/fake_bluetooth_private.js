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

    /** @private {?chrome.bluetoothPrivate.NewAdapterState} */
    this.lastSetAdapterStateValue_ = null;

    /** @private {?function()} */
    this.lastSetAdapterStateCallback_ = null;

    /** @type {!Object<!chrome.bluetoothPrivate.SetPairingResponseOptions>} */
    this.pairingResponses_ = {};
  }

  FakeBluetoothPrivate.prototype = {
    // Public testing methods.

    simulateSuccessfulSetAdapterStateCallForTest: function() {
      // Swap values here to avoid reentrancy issues when we run the callback.
      const lastStateValue = this.lastSetAdapterStateValue_;
      this.lastSetAdapterStateValue_ = null;
      const callback = this.lastSetAdapterStateCallback_;
      this.lastSetAdapterStateCallback_ = null;

      // The underlying Bluetooth API runs the SetAdapterState callback before
      // notifying the that the adapter changed states.
      //
      // setAdapterState()'s callback parameter is optional.
      if (callback) {
        callback();
      }

      const newState = Object.assign(
          this.bluetoothApi_.getAdapterStateForTest(), lastStateValue);

      this.bluetoothApi_.simulateAdapterStateChangedForTest(newState);
    },

    /** @returns {?chrome.bluetoothPrivate.NewAdapterState} */
    getLastSetAdapterStateValueForTest: function() {
      return this.lastSetAdapterStateValue_;
    },

    /** @override */
    setAdapterState: function(state, opt_callback) {
      this.lastSetAdapterStateValue_ = state;
      if (opt_callback !== undefined) {
        this.lastSetAdapterStateCallback_ = opt_callback;
      }
      // Use simulateSuccessfulSetAdapterStateCallForTest to complete the
      // action.
    },

    /** @override */
    setPairingResponse: function(options, opt_callback) {
      this.pairingResponses_[options.device.address] = options;
      if (opt_callback) {
        opt_callback();
      }
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
      this.bluetoothApi_.simulateDeviceUpdatedForTest(device);
      if (opt_callback) {
        opt_callback(chrome.bluetoothPrivate.ConnectResultType.IN_PROGRESS);
      }
    },

    /** @override */
    pair: assertNotReached,

    /** @type {!FakeChromeEvent} */
    onPairing: new FakeChromeEvent(),
  };

  return {FakeBluetoothPrivate: FakeBluetoothPrivate};
});
