// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cros_bluetooth_config.mojom-lite.js depends on url.mojom.Url.
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
// TODO(crbug.com/1010321): Use cros_bluetooth_config.mojom-webui.js instead
// as non-module JS is deprecated.
import 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-lite.js';

/**
 * @fileoverview Fake implementation of BluetoothDiscoveryDelegate for
 * testing.
 */

/**
 * @implements {chromeos.bluetoothConfig.mojom.BluetoothDiscoveryDelegateInterface}
 */
export class FakeBluetoothDiscoveryDelegate {
  constructor() {
    /**
     * @private {!Array<!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties>}
     */
    this.discoveredDevices_ = [];

    /** @private {!Array<Function>} */
    this.deviceListChangedCallbacks_ = [];
  }

  /**
   * @override
   * @param {!Array<!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties>}
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
