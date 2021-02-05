// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/**
 * A mojom.BluetoothInternalsHandler for the chrome://bluetooth-internals
 * page. Provides a fake BluetoothInternalsHandler::GetAdapter
 * implementation and acts as a root of all Test* classes by containing an
 * adapter member.
 */
export class TestBluetoothInternalsHandler extends TestBrowserProxy {
  /**
   * @param {!MojoHandle} handle
   */
  constructor(handle) {
    super([
      'getAdapter',
      'getDebugLogsChangeHandler',
    ]);

    this.receiver_ = new mojom.BluetoothInternalsHandlerReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  async getAdapter() {
    this.methodCalled('getAdapter');
    return {adapter: this.adapter.receiver.$.bindNewPipeAndPassRemote()};
  }

  async getDebugLogsChangeHandler() {
    this.methodCalled('getDebugLogsChangeHandler');
    return {handler: null, initialToggleValue: false};
  }

  setAdapterForTesting(adapter) {
    this.adapter = adapter;
  }

  reset() {
    super.reset();
    this.adapter.reset();
  }
}

/**
 * A bluetooth.mojom.Adapter implementation for the
 * chrome://bluetooth-internals page.
 */
export class TestAdapter extends TestBrowserProxy {
  constructor(adapterInfo) {
    super([
      'getInfo',
      'getDevices',
      'addObserver',
    ]);

    this.receiver = new bluetooth.mojom.AdapterReceiver(this);

    this.deviceImplMap = new Map();
    this.adapterInfo_ = adapterInfo;
    this.devices_ = [];
    this.connectResult_ = bluetooth.mojom.ConnectResult.SUCCESS;
  }

  reset() {
    super.reset();
    this.deviceImplMap.forEach(testDevice => testDevice.reset());
  }

  async connectToDevice(address) {
    assert(this.deviceImplMap.has(address), 'Device does not exist');
    return {
      result: this.connectResult_,
      device:
          this.deviceImplMap.get(address).router.$.bindNewPipeAndPassRemote(),
    };
  }

  async getInfo() {
    this.methodCalled('getInfo');
    return {info: this.adapterInfo_};
  }

  async getDevices() {
    this.methodCalled('getDevices');
    return {devices: this.devices_};
  }

  async addObserver(observer) {
    this.methodCalled('addObserver', observer);
  }

  async registerAdvertisement() {
    this.methodCalled('registerAdvertisement');
    return {advertisement: null};
  }

  async setDiscoverable() {
    this.methodCalled('setDiscoverable');
    return {success: true};
  }

  async setName() {
    this.methodCalled('setName');
    return {success: true};
  }

  async startDiscoverySession() {
    return {session: null};
  }

  async connectToServiceInsecurely(address, service_uuid) {
    return {result: null};
  }

  async createRfcommServiceInsecurely(service_name, service_uuid) {
    return {result: null};
  }

  setTestConnectResult(connectResult) {
    this.connectResult_ = connectResult;
  }

  setTestDevices(devices) {
    this.devices_ = devices;
    this.devices_.forEach(function(device) {
      this.deviceImplMap.set(device.address, new TestDevice(device));
    }, this);
  }

  setTestServicesForTestDevice(deviceInfo, services) {
    assert(this.deviceImplMap.has(deviceInfo.address), 'Device does not exist');
    this.deviceImplMap.get(deviceInfo.address).setTestServices(services);
  }
}

/**
 * A bluetooth.mojom.Device implementation for the
 * chrome://bluetooth-internals page. Remotes are returned by a
 * TestAdapter which provides the DeviceInfo.
 * @param {!device.DeviceInfo} info
 */
export class TestDevice extends TestBrowserProxy {
  constructor(info) {
    super([
      'getInfo',
      'getServices',
    ]);

    this.info_ = info;
    this.services_ = [];

    // NOTE: We use the generated CallbackRouter here because Device defines
    // lots of methods we don't care to mock here. DeviceCallbackRouter
    // callback silently discards messages that have no listeners.
    this.router = new bluetooth.mojom.DeviceCallbackRouter;
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

  setTestServices(services) {
    this.services_ = services;
  }
}

/**
 * Returns a copy of fake adapter info object.
 * @return {!Object}
 */
export function fakeAdapterInfo() {
  return {
    address: '02:1C:7E:6A:11:5A',
    discoverable: false,
    discovering: false,
    initialized: true,
    name: 'computer.example.com-0',
    systemName: 'Example Bluetooth Stack 1.0',
    powered: true,
    present: true,
  };
}

/**
 * Returns a copy of a fake device info object (variant 1).
 * @return {!Object}
 */
export function fakeDeviceInfo1() {
  return {
    address: 'AA:AA:84:96:92:84',
    name: 'AAA',
    nameForDisplay: 'AAA',
    rssi: {value: -40},
    isGattConnected: false,
    serviceDataMap: {},
    services: [],
  };
}

/**
 * Returns a copy of a fake device info object (variant 2).
 * @return {!Object}
 */
export function fakeDeviceInfo2() {
  return {
    address: 'BB:BB:84:96:92:84',
    name: 'BBB',
    nameForDisplay: 'BBB',
    rssi: null,
    isGattConnected: false,
    serviceDataMap: {},
    services: [],
  };
}

/**
 * Returns a copy of fake device info object. The returned device info lack
 * rssi and services properties.
 * @return {!Object}
 */
export function fakeDeviceInfo3() {
  return {
    address: 'CC:CC:84:96:92:84',
    name: 'CCC',
    nameForDisplay: 'CCC',
    serviceDataMap: {},
    isGattConnected: false,
  };
}

/**
 * Returns a copy of fake service info object (variant 1).
 * @return {!Object}
 */
export function fakeServiceInfo1() {
  return {
    id: 'service1',
    uuid: {uuid: '00002a05-0000-1000-8000-00805f9b34fb'},
    isPrimary: true,
  };
}

/**
 * Returns a copy of fake service info object (variant 2).
 * @return {!Object}
 */
export function fakeServiceInfo2() {
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
