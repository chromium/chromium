// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BluetoothDeviceProperties, BluetoothDiscoveryDelegateInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

/**
 * @fileoverview Fake implementation of BluetoothDiscoveryDelegate for
 * testing.
 */

/**
 * @implements {BluetoothDiscoveryDelegateInterface}
 */
export class FakeBluetoothDiscoveryDelegate {
  constructor() {
    /**
     * @private {!Array<!BluetoothDeviceProperties>}
     */
    this.discoveredDevices_ = [];

    /** @private {!Array<Function>} */
    this.deviceListChangedCallbacks_ = [];
  }

  /**
   * @override
   * @param {!Array<!BluetoothDeviceProperties>}
   *     discoveredDevices
   */
  onDiscoveredDevicesListChanged(discoveredDevices) {
    this.discoveredDevices_ = discoveredDevices;
    this.notifyCallbacksDiscoveredDevicesListChanged_();
  }

  /** @override */
  onBluetoothDiscoveryStarted() {
    // TODO(crbug.com/1010321): Implement this function.
  }

  /** @override */
  onBluetoothDiscoveryStopped() {
    // TODO(crbug.com/1010321): Implement this function.
  }

  /**
   * @param {Function} callback
   */
  addDeviceListChangedCallback(callback) {
    this.deviceListChangedCallbacks_.push(callback);
  }

  /**
   * @private
   * Notifies callbacks that discoveredDevices_ has changed.
   */
  notifyCallbacksDiscoveredDevicesListChanged_() {
    this.deviceListChangedCallbacks_.forEach(
        callback => callback(this.discoveredDevices_));
  }
}
