// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1010321): Use cros_bluetooth_config.mojom-webui.js instead
// as non-module JS is deprecated.
import 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-lite.js';

/**
 * @fileoverview Fake implementation of CrosBluetoothConfig for testing.
 */

/** @implements {chromeos.bluetoothConfig.mojom.CrosBluetoothConfigInterface} */
export class FakeBluetoothConfig {
  constructor() {
    /** @private {!chromeos.bluetoothConfig.mojom.BluetoothSystemProperties} */
    this.systemProperties_ = {
      systemState:
          chromeos.bluetoothConfig.mojom.BluetoothSystemState.kDisabled,
    };

    /**
     * @private {!Array<
     *     !chromeos.bluetoothConfig.mojom.SystemPropertiesObserver>}
     */
    this.observers_ = [];
  }

  /**
   * @override
   * @param {SystemPropertiesObserver} observer
   */
  observeSystemProperties(observer) {
    this.observers_.push(observer);
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * @override
   * Begins the operation to enable/disable Bluetooth. If the systemState is
   * current disabled, transitions to enabling. If the systemState is
   * currently enabled, transitions to disabled. Does nothing if already in the
   * requested state. This method should be followed by a call to
   * completeSetBluetoothEnabledStateForTest() to complete the operation.
   * @param {boolean} enabled
   */
  setBluetoothEnabledState(enabled) {
    const bluetoothSystemState =
        chromeos.bluetoothConfig.mojom.BluetoothSystemState;
    const systemState = this.systemProperties_.systemState;
    if ((enabled && systemState === bluetoothSystemState.kEnabled) ||
        (!enabled && systemState === bluetoothSystemState.kDisabled)) {
      return;
    }

    this.setSystemState(
        enabled ? bluetoothSystemState.kEnabling :
                  bluetoothSystemState.kDisabling);
  }

  /**
   * @param {chromeos.bluetoothConfig.mojom.BluetoothSystemState} systemState
   */
  setSystemState(systemState) {
    const newSystemProperties = {...this.systemProperties_};
    newSystemProperties.systemState = systemState;
    this.systemProperties_ = newSystemProperties;
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * Completes the Bluetooth enable/disable operation. This method should be
   * called after setBluetoothEnabledState(). If the systemState is already
   * enabled or disabled, does nothing.
   * @param {boolean} success Whether the operation should succeed or not.
   */
  completeSetBluetoothEnabledState(success) {
    const bluetoothSystemState =
        chromeos.bluetoothConfig.mojom.BluetoothSystemState;
    const systemState = this.systemProperties_.systemState;
    if (systemState === bluetoothSystemState.kDisabled ||
        systemState === bluetoothSystemState.kEnabled) {
      return;
    }

    if (success) {
      this.setSystemState(
          systemState === bluetoothSystemState.kDisabling ?
              bluetoothSystemState.kDisabled :
              bluetoothSystemState.kEnabled);
    } else {
      this.setSystemState(
          systemState === bluetoothSystemState.kDisabling ?
              bluetoothSystemState.kEnabled :
              bluetoothSystemState.kDisabled);
    }
  }

  /**
   * @private
   * Notifies the observer list that systemProperties_ has changed.
   */
  notifyObserversPropertiesUpdated_() {
    this.observers_.forEach(o => o.onPropertiesUpdated(this.systemProperties_));
  }
}