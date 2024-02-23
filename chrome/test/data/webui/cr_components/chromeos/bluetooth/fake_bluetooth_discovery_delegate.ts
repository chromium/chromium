// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BluetoothDeviceProperties, BluetoothDiscoveryDelegateInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

/**
 * @fileoverview Fake implementation of BluetoothDiscoveryDelegate for
 * testing.
 */
export class FakeBluetoothDiscoveryDelegate implements
    BluetoothDiscoveryDelegateInterface {
  discoveredDevices: BluetoothDeviceProperties[] = [];
  deviceListChangedCallbacks: Function[] = [];

  onDiscoveredDevicesListChanged(discoveredDevices:
                                     BluetoothDeviceProperties[]): void {
    this.discoveredDevices = discoveredDevices;
    this.notifyCallbacksDiscoveredDevicesListChanged_();
  }

  onBluetoothDiscoveryStarted(): void {
    // TODO(crbug.com/1010321): Implement this function.
  }

  onBluetoothDiscoveryStopped(): void {
    // TODO(crbug.com/1010321): Implement this function.
  }

  addDeviceListChangedCallback(callback: Function) {
    this.deviceListChangedCallbacks.push(callback);
  }

  /**
   * Notifies callbacks that discoveredDevices has changed.
   */
  private notifyCallbacksDiscoveredDevicesListChanged_(): void {
    this.deviceListChangedCallbacks.forEach(
        callback => callback(this.discoveredDevices));
  }
}
