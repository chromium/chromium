// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1010321): Use cros_bluetooth_config.mojom-webui.js instead
// as non-module JS is deprecated.
import 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-lite.js';

import {stringToMojoString16} from 'chrome://resources/cr_components/chromeos/bluetooth/bluetooth_utils.js';
import {assert} from 'chrome://resources/js/assert.m.js';

/**
 * @param {string} id
 * @param {string} publicName
 * @param {boolean} connected
 * @param {string|undefined} nickname
 * @return {!chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
 */
export function createDefaultBluetoothDevice(
    id, publicName, connected, nickname = undefined) {
  const mojom = chromeos.bluetoothConfig.mojom;
  return {
    deviceProperties: {
      id: id,
      publicName: stringToMojoString16(publicName),
      deviceType: mojom.DeviceType.kComputer,
      audioCapability: mojom.AudioOutputCapability.kNotCapableOfAudio,
      connectionState: connected ? mojom.DeviceConnectionState.kConnected :
                                   mojom.DeviceConnectionState.kNotConnected,
    },
    nickname: nickname,
  };
}

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
      pairedDevices: [],
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
    this.systemProperties_.systemState = systemState;
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
   * Adds a list of devices to the current list of paired devices in
   * |systemProperties|.
   * @param {Array<?chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties>}
   *     devices
   */
  appendToPairedDeviceList(devices) {
    if (devices.length === 0) {
      return;
    }

    this.systemProperties_.pairedDevices =
        this.systemProperties_.pairedDevices.concat(devices);
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * Removes a |device| from the list of paired devices in |systemProperties|.
   * @param {chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
   *     device
   */
  removePairedDevice(device) {
    const foundDeviceIndex = this.systemProperties_.pairedDevices.findIndex(
        pairedDevice =>
            pairedDevice.deviceProperties.id === device.deviceProperties.id);

    assert(
        foundDeviceIndex !== -1, `Device with id: ${device.deviceProperties.id}
            was not found.`);

    this.systemProperties_.pairedDevices.splice(foundDeviceIndex, 1);
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * @private
   * Notifies the observer list that systemProperties_ has changed.
   */
  notifyObserversPropertiesUpdated_() {
    this.observers_.forEach(
        o => o.onPropertiesUpdated({...this.systemProperties_}));
  }
}