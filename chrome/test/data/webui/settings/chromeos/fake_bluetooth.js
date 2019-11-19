// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.bluetooth for testing.
 */
cr.define('settings', function() {
  /**
   * Fake of the chrome.bluetooth API.
   * @constructor
   * @implements {Bluetooth}
   */
  function FakeBluetooth() {
    /** @type {!chrome.bluetooth.AdapterState} */ this.adapterState_ = {
      address: '00:11:22:33:44:55:66',
      name: 'Fake Adapter',
      powered: false,
      available: true,
      discovering: false
    };

    /** @type {!Array<!chrome.bluetooth.Device>} */ this.devices = [];
  }

  FakeBluetooth.prototype = {
    // Public testing methods.

    /**
     * @param {!{
     *    address: (string|undefined),
     *    name: (string|undefined),
     *    powered: (boolean|undefined),
     *    available: (boolean|undefined),
     *    discovering: (boolean|undefined)
     *  }} newState
     */
    simulateAdapterStateChangedForTest: function(newState) {
      Object.assign(this.adapterState_, newState);
      this.onAdapterStateChanged.callListeners(
          Object.assign({}, this.adapterState_));
    },

    /** @return {!chrome.bluetooth.AdapterState} */
    getAdapterStateForTest: function() {
      return Object.assign({}, this.adapterState_);
    },

    clearDevicesForTest: function() {
      this.devices.length = 0;
    },

    /** @param {!Array<!chrome.bluetooth.Device>} devices */
    simulateDevicesAddedForTest: function(devices) {
      const newDevices = devices.slice();
      // Make sure the new devices don't already exist.
      for (const d of newDevices) {
        const found = this.devices.find(element => {
          return element.address == d.address;
        });
        assert(
            !found,
            'Device already added. Use ' +
                'simulateDeviceUpdatedForTest to update existing ' +
                'devices.');
      }
      this.devices.push(...newDevices);
      // The underlying Bluetooth API always returns the devices sorted by
      // address.
      this.devices.sort((d1, d2) => {
        if (d1.address < d2.address) {
          return -1;
        }
        if (d1.address > d2.address) {
          return 1;
        }
        return 0;
      });

      for (const newDevice of newDevices) {
        this.onDeviceAdded.callListeners(newDevice);
      }
    },

    /** @param {!Array<!String>} devices */
    simulateDevicesRemovedForTest: function(deviceAddresses) {
      for (const deviceAddress of deviceAddresses) {
        const removedDeviceIndex = this.devices.findIndex(element => {
          return element.address == deviceAddress;
        });
        assert(
            removedDeviceIndex !== -1,
            'Tried to remove a non-existent device.');

        const [removedDevice] = this.devices.splice(removedDeviceIndex, 1);
        this.onDeviceRemoved.callListeners(removedDevice);
      }
    },

    /** @param {!chrome.bluetooth.Device} updateDevice */
    simulateDeviceUpdatedForTest: function(updatedDevice) {
      const updatedDeviceIndex = this.devices.findIndex(element => {
        return element.address === updatedDevice.address;
      });
      assert(
          updatedDeviceIndex !== -1, 'Tried to update a non-existent device.');
      this.devices[updatedDeviceIndex] = updatedDevice;
      this.onDeviceChanged.callListeners(updatedDevice);
    },

    /**
     * @param {string}
     * @return {!chrome.bluetooth.Device}
     */
    getDeviceForTest: function(address) {
      return this.devices.find(function(d) {
        return d.address == address;
      });
    },

    // Bluetooth overrides.
    /** @override */
    getAdapterState: function(callback) {
      callback(Object.assign({}, this.adapterState_));
    },

    /** @override */
    getDevice: assertNotReached,

    /** @override */
    getDevices: function(opt_filter, opt_callback) {
      if (opt_callback) {
        opt_callback(this.devices.slice());
      }
    },

    /** @override */
    startDiscovery: function(callback) {
      callback();
    },

    /** @override */
    stopDiscovery: assertNotReached,

    /** @override */
    onAdapterStateChanged: new FakeChromeEvent(),

    /** @override */
    onDeviceAdded: new FakeChromeEvent(),

    /** @override */
    onDeviceChanged: new FakeChromeEvent(),

    /** @override */
    onDeviceRemoved: new FakeChromeEvent(),
  };

  return {FakeBluetooth: FakeBluetooth};
});
