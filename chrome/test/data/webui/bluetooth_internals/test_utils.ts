// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AdapterReceiver, ConnectResult, GattServiceRemote} from 'chrome://bluetooth-internals/adapter.mojom-webui.js';
import type {AdapterInfo, AdapterObserver, GattServiceObserver} from 'chrome://bluetooth-internals/adapter.mojom-webui.js';
import {BluetoothInternalsHandlerReceiver} from 'chrome://bluetooth-internals/bluetooth_internals.mojom-webui.js';
import {DeviceCallbackRouter} from 'chrome://bluetooth-internals/device.mojom-webui.js';
import type {DeviceInfo, ServiceInfo} from 'chrome://bluetooth-internals/device.mojom-webui.js';
import type {UUID} from 'chrome://bluetooth-internals/uuid.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
// <if expr="is_chromeos">
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
// </if>

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * A BluetoothInternalsHandler for the chrome://bluetooth-internals
 * page. Provides a fake BluetoothInternalsHandler::GetAdapter
 * implementation and acts as a root of all Test* classes by containing an
 * adapter member.
 */
export class TestBluetoothInternalsHandler extends TestBrowserProxy {
  private receiver_: BluetoothInternalsHandlerReceiver;
  canRequestPermissions: boolean;
  needLocationPermission: boolean;
  needLocationServices: boolean;
  needNearbyDevicesPermission: boolean;
  adapter: TestAdapter = new TestAdapter(fakeAdapterInfo());

  // <if expr="is_chromeos">
  private pendingRestartSystemBluetoothResolver_: PromiseResolver<void>|null =
      null;
  // </if>

  constructor(handle: MojoHandle) {
    super([
      'checkSystemPermissions',
      // <if expr="is_chromeos">
      'completeRestartSystemBluetooth',
      // </if>
      'getAdapter',
      'getDebugLogsChangeHandler',
      'requestLocationServices',
      'requestSystemPermissions',
      // <if expr="is_chromeos">
      'restartSystemBluetooth',
      // </if>
      'startBtsnoop',
      'isBtsnoopFeatureEnabled',
    ]);

    this.receiver_ = new BluetoothInternalsHandlerReceiver(this);
    this.receiver_.$.bindHandle(handle);
    this.needLocationPermission = false;
    this.needNearbyDevicesPermission = false;
    this.needLocationServices = false;
    this.canRequestPermissions = false;
    // <if expr="is_chromeos">
    this.pendingRestartSystemBluetoothResolver_ = null;
    // </if>
  }

  getAdapter() {
    this.methodCalled('getAdapter');
    return Promise.resolve(
        {adapter: this.adapter.receiver.$.bindNewPipeAndPassRemote()});
  }

  getDebugLogsChangeHandler() {
    this.methodCalled('getDebugLogsChangeHandler');
    return Promise.resolve({handler: null, initialToggleValue: false});
  }

  checkSystemPermissions() {
    this.methodCalled('checkSystemPermissions');
    return Promise.resolve({
      needLocationPermission: this.needLocationPermission,
      needNearbyDevicesPermission: this.needNearbyDevicesPermission,
      needLocationServices: this.needLocationServices,
      canRequestPermissions: this.canRequestPermissions,
    });
  }

  requestSystemPermissions() {
    this.methodCalled('requestSystemPermissions');
    return Promise.resolve();
  }

  requestLocationServices() {
    this.methodCalled('requestLocationServices');
    return Promise.resolve();
  }

  startBtsnoop() {
    this.methodCalled('startBtsnoop');
    return Promise.resolve({btsnoop: null});
  }

  isBtsnoopFeatureEnabled() {
    this.methodCalled('isBtsnoopFeatureEnabled');
    return Promise.resolve({enabled: false});
  }

  // <if expr="is_chromeos">
  restartSystemBluetooth() {
    this.methodCalled('restartSystemBluetooth');
    this.pendingRestartSystemBluetoothResolver_ = new PromiseResolver();
    return this.pendingRestartSystemBluetoothResolver_.promise;
  }

  completeRestartSystemBluetooth() {
    assert(!!this.pendingRestartSystemBluetoothResolver_);
    this.pendingRestartSystemBluetoothResolver_.resolve();
    this.pendingRestartSystemBluetoothResolver_ = null;
    this.methodCalled('completeRestartSystemBluetooth');
  }
  // </if>

  setAdapterForTesting(adapter: TestAdapter) {
    this.adapter = adapter;
  }

  setSystemPermission(
      needLocationPermission: boolean, needNearbyDevicesPermission: boolean,
      needLocationServices: boolean, canRequestPermissions: boolean) {
    this.needLocationPermission = needLocationPermission;
    this.needNearbyDevicesPermission = needNearbyDevicesPermission;
    this.needLocationServices = needLocationServices;
    this.canRequestPermissions = canRequestPermissions;
  }

  override reset() {
    super.reset();
    this.adapter.reset();
    this.needLocationPermission = false;
    this.needNearbyDevicesPermission = false;
    this.needLocationServices = false;
    this.canRequestPermissions = false;
  }
}

/**
 * A Adapter implementation for the
 * chrome://bluetooth-internals page.
 */
export class TestAdapter extends TestBrowserProxy {
  receiver: AdapterReceiver;
  deviceImplMap: Map<string, TestDevice>;
  private adapterInfo_: AdapterInfo;
  private devices_: DeviceInfo[];
  private connectResult_: ConnectResult;

  constructor(adapterInfo: AdapterInfo) {
    super([
      'getInfo',
      'getDevices',
      'addObserver',
    ]);

    this.receiver = new AdapterReceiver(this);

    this.deviceImplMap = new Map();
    this.adapterInfo_ = adapterInfo;
    this.devices_ = [];
    this.connectResult_ = ConnectResult.SUCCESS;
  }

  override reset() {
    super.reset();
    this.deviceImplMap.forEach(testDevice => testDevice.reset());
  }

  connectToDevice(address: string) {
    assert(this.deviceImplMap.has(address), 'Device does not exist');
    return Promise.resolve({
      result: this.connectResult_,
      device:
          this.deviceImplMap.get(address)!.router.$.bindNewPipeAndPassRemote(),
    });
  }

  getInfo() {
    this.methodCalled('getInfo');
    return Promise.resolve({info: this.adapterInfo_});
  }

  getDevices() {
    this.methodCalled('getDevices');
    return Promise.resolve({devices: this.devices_});
  }

  addObserver(observer: AdapterObserver) {
    this.methodCalled('addObserver', observer);
    return Promise.resolve();
  }

  registerAdvertisement() {
    this.methodCalled('registerAdvertisement');
    return Promise.resolve({advertisement: null});
  }

  setDiscoverable() {
    this.methodCalled('setDiscoverable');
    return Promise.resolve({success: true});
  }

  setName() {
    this.methodCalled('setName');
    return Promise.resolve({success: true});
  }

  startDiscoverySession() {
    return Promise.resolve({session: null});
  }

  connectToServiceInsecurely(
      _address: string, _serviceUuid: UUID, _shouldUnboundOnError: boolean) {
    return Promise.resolve({result: null});
  }

  createRfcommServiceInsecurely(_serviceName: string, _serviceUuid: UUID) {
    return Promise.resolve({serverSocket: null});
  }

  createLocalGattService(_serviceId: UUID, _observer: GattServiceObserver) {
    return Promise.resolve({gattService: new GattServiceRemote()});
  }

  isLeScatternetDualRoleSupported() {
    return Promise.resolve({isSupported: false});
  }

  setTestConnectResult(connectResult: ConnectResult) {
    this.connectResult_ = connectResult;
  }

  setTestDevices(devices: DeviceInfo[]) {
    this.devices_ = devices;
    this.devices_.forEach(device => {
      this.deviceImplMap.set(device.address, new TestDevice(device));
    });
  }

  setTestServicesForTestDevice(
      deviceInfo: DeviceInfo, services: ServiceInfo[]) {
    assert(this.deviceImplMap.has(deviceInfo.address), 'Device does not exist');
    this.deviceImplMap.get(deviceInfo.address)!.setTestServices(services);
  }
}

/**
 * A Device implementation for the
 * chrome://bluetooth-internals page. Remotes are returned by a
 * TestAdapter which provides the DeviceInfo.
 */
export class TestDevice extends TestBrowserProxy {
  router: DeviceCallbackRouter;
  private info_: DeviceInfo;
  private services_: ServiceInfo[];

  constructor(info: DeviceInfo) {
    super([
      'getInfo',
      'getServices',
    ]);

    this.info_ = info;
    this.services_ = [];

    // NOTE: We use the generated CallbackRouter here because Device defines
    // lots of methods we don't care to mock here. DeviceCallbackRouter
    // callback silently discards messages that have no listeners.
    this.router = new DeviceCallbackRouter();
    this.router.disconnect.addListener(() => this.router.$.close());
    this.router.getInfo.addListener(() => this.getInfo());
    this.router.getServices.addListener(() => this.getServices());
  }

  getInfo() {
    this.methodCalled('getInfo');
    return {info: this.info_};
  }

  getServices() {
    this.methodCalled('getServices');
    return {services: this.services_};
  }

  setTestServices(services: ServiceInfo[]) {
    this.services_ = services;
  }
}

/**
 * Returns a copy of fake adapter info object.
 */
export function fakeAdapterInfo(): AdapterInfo {
  return {
    address: '02:1C:7E:6A:11:5A',
    discoverable: false,
    discovering: false,
    initialized: true,
    name: 'computer.example.com-0',
    systemName: 'Example Bluetooth Stack 1.0',
    powered: true,
    present: true,
    floss: false,
    extendedAdvertisementSupport: false,
  };
}

/**
 * Returns a copy of a fake device info object (variant 1).
 */
export function fakeDeviceInfo1(): DeviceInfo {
  return {
    address: 'AA:AA:84:96:92:84',
    name: 'AAA',
    nameForDisplay: 'AAA',
    rssi: -40,
    serviceUuids: [{uuid: '00002a05-0000-1000-8000-00805f9b34fb'}],
    isGattConnected: false,
    manufacturerDataMap: {'1': [1, 2], '2': [3, 4]},
    serviceDataMap: new Map(),
  };
}

/**
 * Returns a copy of a fake device info object (variant 2).
 */
export function fakeDeviceInfo2(): DeviceInfo {
  return {
    address: 'BB:BB:84:96:92:84',
    name: 'BBB',
    nameForDisplay: 'BBB',
    rssi: null,
    serviceUuids: [],
    isGattConnected: false,
    manufacturerDataMap: {},
    serviceDataMap: new Map(),
  };
}

/**
 * Returns a copy of fake device info object. The returned device info lack
 * rssi and serviceUuids properties.
 */
export function fakeDeviceInfo3(): DeviceInfo {
  return {
    address: 'CC:CC:84:96:92:84',
    name: 'CCC',
    nameForDisplay: 'CCC',
    rssi: null,
    serviceUuids: [],
    isGattConnected: false,
    manufacturerDataMap: {},
    serviceDataMap: new Map(),
  };
}

/**
 * Returns a copy of fake service info object (variant 1).
 */
export function fakeServiceInfo1(): ServiceInfo {
  return {
    id: 'service1',
    uuid: {uuid: '00002a05-0000-1000-8000-00805f9b34fb'},
    isPrimary: true,
  };
}

/**
 * Returns a copy of fake service info object (variant 2).
 */
export function fakeServiceInfo2(): ServiceInfo {
  return {
    id: 'service2',
    uuid: {uuid: '0000180d-0000-1000-8000-00805f9b34fb'},
    isPrimary: true,
  };
}

/**
 * Returns a copy of fake characteristic info object with all properties
 * and all permissions bits set.
 * @return {!Object}
 */
export function fakeCharacteristicInfo1() {
  return {
    id: 'characteristic1',
    uuid: '00002a19-0000-1000-8000-00805f9b34fb',
    properties: Number.MAX_SAFE_INTEGER,
    permissions: Number.MAX_SAFE_INTEGER,
    lastKnownValue: [],
  };
}
