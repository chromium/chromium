// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import type {BluetoothDeviceProperties, BluetoothDeviceStatusObserverInterface, BluetoothDiscoveryDelegateInterface, BluetoothSystemProperties, DiscoverySessionStatusObserverInterface, PairedBluetoothDeviceProperties, SystemPropertiesObserverInterface} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';
import {AudioOutputCapability, BluetoothModificationState, BluetoothSystemState, CrosBluetoothConfigInterface, DeviceConnectionState, DevicePairingHandlerReceiver, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

import {assertFalse, assertNotReached, assertTrue} from '../chai_assert.js';

import {FakeDevicePairingHandler} from './fake_device_pairing_handler.js';


export function createDefaultBluetoothDevice(
    id: string,
    publicName: string,
    connectionState: DeviceConnectionState,
    nickname?: string,
    audioCapability = AudioOutputCapability.kNotCapableOfAudioOutput,
    deviceType = DeviceType.kUnknown,
    isBlockedByPolicy = false,
    ): PairedBluetoothDeviceProperties {
  return {
    deviceProperties: {
      id: id,
      address: id,
      publicName: stringToMojoString16(publicName),
      deviceType: deviceType,
      audioCapability: audioCapability,
      connectionState: connectionState,
      isBlockedByPolicy: isBlockedByPolicy,
      batteryInfo: undefined,
      imageInfo: undefined,
    },
    nickname: nickname,
    fastPairableDevicePairingState: undefined,
  };
}

interface Callback {
  deviceId: string;
  callback: Function;
}

interface Response {
  success: boolean;
}

/**
 * @fileoverview Fake implementation of CrosBluetoothConfig for testing.
 */
export class FakeBluetoothConfig extends CrosBluetoothConfigInterface {
  systemProperties: BluetoothSystemProperties;
  discoveredDevices: BluetoothDeviceProperties[] = [];
  systemPropertiesObservers: SystemPropertiesObserverInterface[] = [];
  bluetoothDeviceStatusObservers: BluetoothDeviceStatusObserverInterface[] = [];
  lastDiscoveryDelegate: BluetoothDiscoveryDelegateInterface|null = null;
  /**
   * Object containing the device ID and callback for the current connect
   * request.
   */
  pendingConnectRequest: Callback|null = null;

  /**
   * Object containing the device ID and callback for the current disconnect
   * request.
   */
  pendingDisconnectRequest: Callback|null = null;
  /**
   * Object containing the device ID and callback for the current forget
   * request.
   */
  pendingForgetRequest: Callback|null = null;

  /**
   * The last pairing handler created. If defined, indicates discovery in is
   * progress. If null, indicates no discovery is occurring.
   */
  lastPairingHandler: FakeDevicePairingHandler|null = null;
  numStartDiscoveryCalls: number = 0;

  constructor() {
    super();
    this.systemProperties = {
      systemState: BluetoothSystemState.kDisabled,
      modificationState: BluetoothModificationState.kCannotModifyBluetooth,
      pairedDevices: [],
      fastPairableDevices: [],
    };
  }

  override observeSystemProperties(observer: SystemPropertiesObserverInterface):
      void {
    this.systemPropertiesObservers.push(observer);
    this.notifyObserversPropertiesUpdated_();
  }

  override observeDeviceStatusChanges(
      observer: BluetoothDeviceStatusObserverInterface): void {
    this.bluetoothDeviceStatusObservers.push(observer);
  }


  override observeDiscoverySessionStatusChanges(
      observer: DiscoverySessionStatusObserverInterface): void {
    // This method is left unimplemented since the observer is not used in JS.
    assertFalse(!!observer);
    assertNotReached();
  }

  override startDiscovery(delegate: BluetoothDiscoveryDelegateInterface): void {
    this.lastDiscoveryDelegate = delegate;
    this.notifyDiscoveryStarted_();
    this.notifyDelegatesPropertiesUpdated_();
    this.numStartDiscoveryCalls++;
  }

  /**
   * Begins the operation to enable/disable Bluetooth. If the systemState is
   * current disabled, transitions to enabling. If the systemState is
   * currently enabled, transitions to disabled. Does nothing if already in the
   * requested state. This method should be followed by a call to
   * completeSetBluetoothEnabledState() to complete the operation.
   */
  override setBluetoothEnabledState(enabled: boolean): void {
    const bluetoothSystemState = BluetoothSystemState;
    const systemState = this.systemProperties.systemState;
    if ((enabled && systemState === bluetoothSystemState.kEnabled) ||
        (!enabled && systemState === bluetoothSystemState.kDisabled)) {
      return;
    }

    this.setSystemState(
        enabled ? bluetoothSystemState.kEnabling :
                  bluetoothSystemState.kDisabling);
  }

  setBluetoothHidDetectionActive(): void {
    // This method is left unimplemented as it is only used in OOBE.
    assertNotReached();
  }

  override setBluetoothHidDetectionInactive(isUsingBluetooth: boolean): void {
    // This method is left unimplemented as it is only used in OOBE.
    assertTrue(isUsingBluetooth === undefined);
    assertNotReached();
  }

  /**
   * Initiates connecting to a device with id |deviceId|. To finish the
   * operation, call completeConnect().
   */
  override connect(deviceId: string): Promise<Response> {
    assertFalse(!!this.pendingConnectRequest);

    const device = this.systemProperties.pairedDevices.find(
        d => d.deviceProperties.id === deviceId);

    assertTrue(!!device);
    // device uses ! flag because the compilar currently fails when
    // running test locally.
    device!.deviceProperties.connectionState =
        DeviceConnectionState.kConnecting;
    this.updatePairedDevice(device!);

    return new Promise((resolve) => {
      this.pendingConnectRequest = {
        deviceId: deviceId,
        callback: resolve,
      };
    });
  }

  /**
   * Initiates disconnecting from a device with id |deviceId|. To finish the
   * operation, call completeDisconnect().
   */
  override disconnect(deviceId: string): Promise<Response> {
    assertFalse(!!this.pendingDisconnectRequest);
    return new Promise((resolve) => {
      this.pendingDisconnectRequest = {
        deviceId: deviceId,
        callback: resolve,
      };
    });
  }

  /**
   * Initiates forgetting a device with id |deviceId|. To finish the
   * operation, call completeForget().
   */
  override forget(deviceId: string): Promise<Response> {
    assertFalse(!!this.pendingForgetRequest);
    return new Promise((resolve) => {
      this.pendingForgetRequest = {
        deviceId: deviceId,
        callback: resolve,
      };
    });
  }

  override setDeviceNickname(deviceId: string, nickname: string): void {
    const device = this.systemProperties.pairedDevices.find(
        d => d.deviceProperties.id === deviceId);
    device!.nickname = nickname;
    this.updatePairedDevice(device!);
  }

  setSystemState(systemState: BluetoothSystemState): void {
    this.systemProperties.systemState = systemState;
    this.systemProperties = Object.assign({}, this.systemProperties);
    this.notifyObserversPropertiesUpdated_();

    // If discovery is in progress and Bluetooth is no longer enabled, stop
    // discovery and any pairing that may be occurring.
    if (!this.lastPairingHandler) {
      return;
    }

    if (systemState === BluetoothSystemState.kEnabled) {
      return;
    }

    this.lastPairingHandler.rejectPairDevice();
    this.lastPairingHandler = null;
    this.notifyDiscoveryStopped_();
  }

  /**
   * Completes the Bluetooth enable/disable operation. This method should be
   * called after setBluetoothEnabledState(). If the systemState is already
   * enabled or disabled, does nothing.
   */
  completeSetBluetoothEnabledState(success: boolean): void {
    const bluetoothSystemState = BluetoothSystemState;
    const systemState = this.systemProperties.systemState;
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

  setModificationState(modificationState: BluetoothModificationState): void {
    this.systemProperties.modificationState = modificationState;
    this.systemProperties = Object.assign({}, this.systemProperties);
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * Adds a list of devices to the current list of paired devices in
   * |systemProperties|.
   */
  appendToPairedDeviceList(devices: PairedBluetoothDeviceProperties[]): void {
    if (devices.length === 0) {
      return;
    }

    this.systemProperties.pairedDevices =
        [...this.systemProperties.pairedDevices, ...devices];
    this.systemProperties = Object.assign({}, this.systemProperties);
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * Removes a |device| from the list of paired devices in |systemProperties|.
   */
  removePairedDevice(device: PairedBluetoothDeviceProperties): void {
    const pairedDevices = this.systemProperties.pairedDevices.filter(
        d => d.deviceProperties.id !== device.deviceProperties.id);
    this.systemProperties.pairedDevices = pairedDevices;
    this.systemProperties = Object.assign({}, this.systemProperties);
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * Adds a list of devices to the current list of discovered devices in
   * |discoveredDevices|.
   */
  appendToDiscoveredDeviceList(devices: BluetoothDeviceProperties[]): void {
    if (!devices.length) {
      return;
    }

    this.discoveredDevices = [...this.discoveredDevices, ...devices];
    this.notifyDelegatesPropertiesUpdated_();
  }

  /**
   * Resets discovered devices list to an empty list.
   */
  resetDiscoveredDeviceList(): void {
    this.discoveredDevices = [];
    this.notifyDelegatesPropertiesUpdated_();
  }

  /**
   * Replaces device found in |systemProperties| with |device|.
   */
  updatePairedDevice(device: PairedBluetoothDeviceProperties): void {
    const pairedDevices = this.systemProperties.pairedDevices.filter(
        d => d.deviceProperties.id !== device.deviceProperties.id);
    this.systemProperties.pairedDevices =
        [...pairedDevices, Object.assign({}, device)];
    this.systemProperties = Object.assign({}, this.systemProperties);
    this.notifyObserversPropertiesUpdated_();
  }

  /**
   * Completes the pending connect() call.
   */
  completeConnect(success: boolean): void {
    assertTrue(!!this.pendingConnectRequest);
    const device = this.systemProperties.pairedDevices.find(
        d => d.deviceProperties.id === this.pendingConnectRequest!.deviceId);
    device!.deviceProperties!.connectionState =
        DeviceConnectionState.kNotConnected;

    if (success) {
      device!.deviceProperties.connectionState =
          DeviceConnectionState.kConnected;
    }

    this.updatePairedDevice(device!);
    this.pendingConnectRequest!.callback({success});
    this.pendingConnectRequest = null;
  }

  /**
   * Completes the pending disconnect() call.
   */
  completeDisconnect(success: boolean): void {
    assertTrue(!!this.pendingDisconnectRequest);
    if (success) {
      const device = this.systemProperties.pairedDevices.find(
          d => d.deviceProperties.id ===
              this.pendingDisconnectRequest!.deviceId);
      device!.deviceProperties.connectionState =
          DeviceConnectionState.kNotConnected;
      this.updatePairedDevice(device!);
    }
    this.pendingDisconnectRequest!.callback({success});
    this.pendingDisconnectRequest = null;
  }

  /**
   * Completes the pending forget() call.
   */
  completeForget(success: boolean): void {
    assertTrue(!!this.pendingForgetRequest);
    if (success) {
      const device = this.systemProperties.pairedDevices.find(
          d => d.deviceProperties.id === this.pendingForgetRequest!.deviceId);
      if (device) {
        this.removePairedDevice(device);
        this.appendToDiscoveredDeviceList([device.deviceProperties]);
      }
    }
    this.pendingForgetRequest!.callback({success});
    this.pendingForgetRequest = null;
  }

  /**
   * Notifies the observer list that systemProperties has changed.
   */
  private notifyObserversPropertiesUpdated_(): void {
    const systemProperties = Object.assign({}, this.systemProperties);

    // Don't provide paired devices if the system state is unavailable.
    if (systemProperties.systemState === BluetoothSystemState.kUnavailable) {
      systemProperties.pairedDevices = [];
    }
    this.systemPropertiesObservers.forEach(
        o => o.onPropertiesUpdated(systemProperties));
  }

  /**
   * Notifies the delegates list that discoveredDevices has changed.
   */
  private notifyDelegatesPropertiesUpdated_(): void {
    if (!this.lastDiscoveryDelegate) {
      return;
    }

    // lastDiscoveryDelegate uses ! flag because the compilar currently fails
    // when running test locally.
    this.lastDiscoveryDelegate!.onDiscoveredDevicesListChanged(
        [...this.discoveredDevices]);
  }

  /**
   * Notifies the delegates list that device discovery has started.
   */
  private notifyDiscoveryStarted_() {
    this.lastPairingHandler = new FakeDevicePairingHandler();
    const devicePairingHandlerReciever =
        new DevicePairingHandlerReceiver(this.lastPairingHandler);
    this.lastDiscoveryDelegate!.onBluetoothDiscoveryStarted(
        devicePairingHandlerReciever.$.bindNewPipeAndPassRemote());
  }

  /**
   * Notifies the last delegate that device discovery has stopped.
   */
  private notifyDiscoveryStopped_() {
    this.lastDiscoveryDelegate!.onBluetoothDiscoveryStopped();
  }

  getLastCreatedPairingHandler(): FakeDevicePairingHandler|null {
    return this.lastPairingHandler;
  }

  getPairedDeviceById(deviceId: string): PairedBluetoothDeviceProperties|null {
    const device = this.systemProperties.pairedDevices.find(
        d => d.deviceProperties.id === deviceId);
    return device ? device : null;
  }

  getNumStartDiscoveryCalls(): number {
    return this.numStartDiscoveryCalls;
  }
}
