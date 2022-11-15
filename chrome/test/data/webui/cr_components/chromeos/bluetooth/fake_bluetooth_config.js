// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/ash/common/bluetooth/bluetooth_utils.js';
import {AudioOutputCapability, BluetoothDeviceProperties, BluetoothDeviceStatusObserverInterface, BluetoothDiscoveryDelegateInterface, BluetoothModificationState, BluetoothSystemProperties, BluetoothSystemState, CrosBluetoothConfigInterface, DeviceConnectionState, DevicePairingHandlerReceiver, DeviceType, DiscoverySessionStatusObserverInterface, PairedBluetoothDeviceProperties, SystemPropertiesObserverInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

import {assertFalse, assertNotReached, assertTrue} from '../../../chromeos/chai_assert.js';

import {FakeDevicePairingHandler} from './fake_device_pairing_handler.js';

/**
 * @param {string} id
 * @param {string} publicName
 * @param {!DeviceConnectionState}
 *     connectionState
 * @param {string=} opt_nickname
 * @param {!AudioOutputCapability=}
 *     opt_audioCapability
 * @param {!DeviceType=}
 *     opt_deviceType
 * @param {boolean=} opt_isBlockedByPolicy
 * @return {!PairedBluetoothDeviceProperties}
 */
export function createDefaultBluetoothDevice(
    id, publicName, connectionState, opt_nickname = undefined,
    opt_audioCapability = AudioOutputCapability.kNotCapableOfAudioOutput,
    opt_deviceType = DeviceType.kUnknown, opt_isBlockedByPolicy = false) {
  return {
    deviceProperties: {
      id: id,
      address: id,
      publicName: stringToMojoString16(publicName),
      deviceType: opt_deviceType,
      audioCapability: opt_audioCapability,
      connectionState: connectionState,
      isBlockedByPolicy: opt_isBlockedByPolicy,
    },
    nickname: opt_nickname,
  };
}

/**
 * @fileoverview Fake implementation of CrosBluetoothConfig for testing.
 */

/** @implements {CrosBluetoothConfigInterface} */
export class FakeBluetoothConfig {
  constructor() {
    /** @private {!BluetoothSystemProperties} */
    this.systemProperties_ = {
      systemState: BluetoothSystemState.kDisabled,
      modificationState: BluetoothModificationState.kCannotModifyBluetooth,
      pairedDevices: [],
    };

    /**
     * @private {!Array<!BluetoothDeviceProperties>}
     */
    this.discoveredDevices_ = [];

    /**
     * @private {!Array<
     *     !SystemPropertiesObserverInterface>}
     */
    this.system_properties_observers_ = [];

    /**
     * @private {!Array<
     *     !BluetoothDeviceStatusObserverInterface>}
     */
    this.bluetooth_device_status_observers_ = [];

    /**
     * @private {?BluetoothDiscoveryDelegateInterface}
     */
    this.lastDiscoveryDelegate_ = null;

    /**
     * Object containing the device ID and callback for the current connect
     * request.
     * @private {?{deviceId: string, callback: function(!{success: boolean})}}
     */
    this.pendingConnectRequest_ = null;

    /**
     * Object containing the device ID and callback for the current disconnect
     * request.
     * @private {?{deviceId: string, callback: function(!{success: boolean})}}
     */
    this.pendingDisconnectRequest_ = null;

    /**
     * Object containing the device ID and callback for the current forget
     * request.
     * @private {?{deviceId: string, callback: function(!{success: boolean})}}
     */
    this.pendingForgetRequest_ = null;

    /**
     * The last pairing handler created. If defined, indicates discovery in is
     * progress. If null, indicates no discovery is occurring.
     * @private {?FakeDevicePairingHandler}
     */
    this.lastPairingHandler_ = null;

    /** @private {number} */
    this.numStartDiscoveryCalls_ = 0;
  }

  /**
   * @override
   * @param {!SystemPropertiesObserverInterface}
   *     observer
   */
  observeSystemProperties(observer) {
    this.system_properties_observers_.push(observer);
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * @override
   * @param {!BluetoothDeviceStatusObserverInterface}
   *     observer
   */
  observeDeviceStatusChanges(observer) {
    this.bluetooth_device_status_observers_.push(observer);
  }

  /**
   * @override
   * @param {!DiscoverySessionStatusObserverInterface}
   *     observer
   */
  observeDiscoverySessionStatusChanges(observer) {
    // This method is left unimplemented since the observer is not used in JS.
    assertNotReached();
  }


  /**
   * @override
   * @param {!BluetoothDiscoveryDelegateInterface}
   *     delegate
   */
  startDiscovery(delegate) {
    this.lastDiscoveryDelegate_ = delegate;
    this.notifyDiscoveryStarted_();
    this.notifyDelegatesPropertiesUpdated_();
    this.numStartDiscoveryCalls_++;
  }

  /**
   * @override
   * Begins the operation to enable/disable Bluetooth. If the systemState is
   * current disabled, transitions to enabling. If the systemState is
   * currently enabled, transitions to disabled. Does nothing if already in the
   * requested state. This method should be followed by a call to
   * completeSetBluetoothEnabledState() to complete the operation.
   * @param {boolean} enabled
   */
  setBluetoothEnabledState(enabled) {
    const bluetoothSystemState = BluetoothSystemState;
    const systemState = this.systemProperties_.systemState;
    if ((enabled && systemState === bluetoothSystemState.kEnabled) ||
        (!enabled && systemState === bluetoothSystemState.kDisabled)) {
      return;
    }

    this.setSystemState(
        enabled ? bluetoothSystemState.kEnabling :
                  bluetoothSystemState.kDisabling);
  }

  /** @override */
  setBluetoothHidDetectionActive() {
    // This method is left unimplemented as it is only used in OOBE.
    assertNotReached();
  }

  /**
   * @override
   * @param {boolean} isUsingBluetooth
   */
  setBluetoothHidDetectionInactive(isUsingBluetooth) {
    // This method is left unimplemented as it is only used in OOBE.
    assertNotReached();
  }

  /**
   * Initiates connecting to a device with id |deviceId|. To finish the
   * operation, call completeConnect().
   * @override
   */
  connect(deviceId) {
    assertFalse(!!this.pendingConnectRequest_);

    const device = this.systemProperties_.pairedDevices.find(
        d => d.deviceProperties.id === deviceId);
    device.deviceProperties.connectionState = DeviceConnectionState.kConnecting;
    this.updatePairedDevice(device);

    return new Promise((resolve, reject) => {
      this.pendingConnectRequest_ = {
        deviceId: deviceId,
        callback: resolve,
      };
    });
  }

  /**
   * Initiates disconnecting from a device with id |deviceId|. To finish the
   * operation, call completeDisconnect().
   * @override
   */
  disconnect(deviceId) {
    assertFalse(!!this.pendingDisconnectRequest_);
    return new Promise((resolve, reject) => {
      this.pendingDisconnectRequest_ = {
        deviceId: deviceId,
        callback: resolve,
      };
    });
  }

  /**
   * Initiates forgetting a device with id |deviceId|. To finish the
   * operation, call completeForget().
   * @override
   */
  forget(deviceId) {
    assertFalse(!!this.pendingForgetRequest_);
    return new Promise((resolve, reject) => {
      this.pendingForgetRequest_ = {
        deviceId: deviceId,
        callback: resolve,
      };
    });
  }

  /** @override */
  setDeviceNickname(deviceId, nickname) {
    const device = this.systemProperties_.pairedDevices.find(
        d => d.deviceProperties.id === deviceId);
    device.nickname = nickname;
    this.updatePairedDevice(device);
  }

  /**
   * @param {BluetoothSystemState} systemState
   */
  setSystemState(systemState) {
    this.systemProperties_.systemState = systemState;
    this.systemProperties_ =
        /**
         * @type {!BluetoothSystemProperties}
         */
        (Object.assign({}, this.systemProperties_));
    this.notifyObserversPropertiesUpdated_();

    // If discovery is in progress and Bluetooth is no longer enabled, stop
    // discovery and any pairing that may be occurring.
    if (!this.lastPairingHandler_) {
      return;
    }

    if (systemState === BluetoothSystemState.kEnabled) {
      return;
    }

    this.lastPairingHandler_.rejectPairDevice();
    this.lastPairingHandler_ = null;
    this.notifyDiscoveryStopped_();
  }

  /**
   * Completes the Bluetooth enable/disable operation. This method should be
   * called after setBluetoothEnabledState(). If the systemState is already
   * enabled or disabled, does nothing.
   * @param {boolean} success Whether the operation should succeed or not.
   */
  completeSetBluetoothEnabledState(success) {
    const bluetoothSystemState = BluetoothSystemState;
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
   * @param {!BluetoothModificationState}
   *     modificationState
   */
  setModificationState(modificationState) {
    this.systemProperties_.modificationState = modificationState;
    this.systemProperties_ =
        /**
         * @type {!BluetoothSystemProperties}
         */
        (Object.assign({}, this.systemProperties_));
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * Adds a list of devices to the current list of paired devices in
   * |systemProperties|.
   * @param {Array<?PairedBluetoothDeviceProperties>}
   *     devices
   */
  appendToPairedDeviceList(devices) {
    if (devices.length === 0) {
      return;
    }

    this.systemProperties_.pairedDevices =
        [...this.systemProperties_.pairedDevices, ...devices];
    this.systemProperties_ =
        /**
         * @type {!BluetoothSystemProperties}
         */
        (Object.assign({}, this.systemProperties_));
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * Removes a |device| from the list of paired devices in |systemProperties|.
   * @param {PairedBluetoothDeviceProperties}
   *     device
   */
  removePairedDevice(device) {
    const pairedDevices = this.systemProperties_.pairedDevices.filter(
        d => d.deviceProperties.id !== device.deviceProperties.id);
    this.systemProperties_.pairedDevices = pairedDevices;
    this.systemProperties_ =
        /**
         * @type {!BluetoothSystemProperties}
         */
        (Object.assign({}, this.systemProperties_));
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * Adds a list of devices to the current list of discovered devices in
   * |discoveredDevices_|.
   * @param {Array<!BluetoothDeviceProperties>}
   *     devices
   */
  appendToDiscoveredDeviceList(devices) {
    if (!devices.length) {
      return;
    }

    this.discoveredDevices_ = [...this.discoveredDevices_, ...devices];
    this.notifyDelegatesPropertiesUpdated_();
  }

  /**
   * Resets discovered devices list to an empty list.
   */
  resetDiscoveredDeviceList() {
    this.discoveredDevices_ = [];
    this.notifyDelegatesPropertiesUpdated_();
  }

  /**
   * Replaces device found in |systemProperties| with |device|.
   * @param {PairedBluetoothDeviceProperties}
   *     device
   */
  updatePairedDevice(device) {
    const pairedDevices = this.systemProperties_.pairedDevices.filter(
        d => d.deviceProperties.id !== device.deviceProperties.id);
    this.systemProperties_.pairedDevices =
        [...pairedDevices, Object.assign({}, device)];
    this.systemProperties_ =
        /**
         * @type {!BluetoothSystemProperties}
         */
        (Object.assign({}, this.systemProperties_));
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * Completes the pending connect() call.
   * @param {boolean} success The result of the operation.
   */
  completeConnect(success) {
    assertTrue(!!this.pendingConnectRequest_);
    const device = this.systemProperties_.pairedDevices.find(
        d => d.deviceProperties.id === this.pendingConnectRequest_.deviceId);
    device.deviceProperties.connectionState =
        DeviceConnectionState.kNotConnected;

    if (success) {
      device.deviceProperties.connectionState =
          DeviceConnectionState.kConnected;
    }

    this.updatePairedDevice(device);
    this.pendingConnectRequest_.callback({success});
    this.pendingConnectRequest_ = null;
  }

  /**
   * Completes the pending disconnect() call.
   * @param {boolean} success The result of the operation.
   */
  completeDisconnect(success) {
    assertTrue(!!this.pendingDisconnectRequest_);
    if (success) {
      const device = this.systemProperties_.pairedDevices.find(
          d => d.deviceProperties.id ===
              this.pendingDisconnectRequest_.deviceId);
      device.deviceProperties.connectionState =
          DeviceConnectionState.kNotConnected;
      this.updatePairedDevice(device);
    }
    this.pendingDisconnectRequest_.callback({success});
    this.pendingDisconnectRequest_ = null;
  }

  /**
   * Completes the pending forget() call.
   * @param {boolean} success The result of the operation.
   */
  completeForget(success) {
    assertTrue(!!this.pendingForgetRequest_);
    if (success) {
      const device = this.systemProperties_.pairedDevices.find(
          d => d.deviceProperties.id === this.pendingForgetRequest_.deviceId);
      if (device) {
        this.removePairedDevice(device);
        this.appendToDiscoveredDeviceList([device.deviceProperties]);
      }
    }
    this.pendingForgetRequest_.callback({success});
    this.pendingForgetRequest_ = null;
  }

  /**
   * @private
   * Notifies the observer list that systemProperties_ has changed.
   */
  notifyObserversPropertiesUpdated_() {
    const systemProperties =
        /**
         * @type {!BluetoothSystemProperties}
         */
        (Object.assign({}, this.systemProperties_));

    // Don't provide paired devices if the system state is unavailable.
    if (systemProperties.systemState === BluetoothSystemState.kUnavailable) {
      systemProperties.pairedDevices = [];
    }
    this.system_properties_observers_.forEach(
        o => o.onPropertiesUpdated(systemProperties));
  }

  /**
   * @private
   * Notifies the delegates list that discoveredDevices_ has changed.
   */
  notifyDelegatesPropertiesUpdated_() {
    if (!this.lastDiscoveryDelegate_) {
      return;
    }
    this.lastDiscoveryDelegate_.onDiscoveredDevicesListChanged(
        [...this.discoveredDevices_]);
  }

  /**
   * @private
   * Notifies the delegates list that device discovery has started.
   */
  notifyDiscoveryStarted_() {
    this.lastPairingHandler_ = new FakeDevicePairingHandler();
    const devicePairingHandlerReciever =
        new DevicePairingHandlerReceiver(this.lastPairingHandler_);
    this.lastDiscoveryDelegate_.onBluetoothDiscoveryStarted(
        devicePairingHandlerReciever.$.bindNewPipeAndPassRemote());
  }

  /**
   * @private
   * Notifies the last delegate that device discovery has stopped.
   */
  notifyDiscoveryStopped_() {
    this.lastDiscoveryDelegate_.onBluetoothDiscoveryStopped();
  }

  /**
   * @return {?FakeDevicePairingHandler}
   */
  getLastCreatedPairingHandler() {
    return this.lastPairingHandler_;
  }

  /**
   * @param {string} deviceId
   * @return {?PairedBluetoothDeviceProperties}
   */
  getPairedDeviceById(deviceId) {
    const device = this.systemProperties_.pairedDevices.find(
        d => d.deviceProperties.id === deviceId);
    return device ? device : null;
  }

  /**
   * @return {number}
   */
  getNumStartDiscoveryCalls() {
    return this.numStartDiscoveryCalls_;
  }
}
