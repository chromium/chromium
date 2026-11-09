// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DeviceTabVisibilityBrowserProxy, DeviceTabVisibilityEntry} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDeviceTabVisibilityBrowserProxy extends TestBrowserProxy
    implements DeviceTabVisibilityBrowserProxy {
  private devices_: DeviceTabVisibilityEntry[] = [];

  constructor() {
    super([
      'getDeviceTabVisibilityList',
      'setDeviceTabVisibility',
    ]);
  }

  setDevices(devices: DeviceTabVisibilityEntry[]) {
    this.devices_ = devices;
  }

  getDeviceTabVisibilityList(): Promise<DeviceTabVisibilityEntry[]> {
    this.methodCalled('getDeviceTabVisibilityList');
    return Promise.resolve(this.devices_.slice());
  }

  setDeviceTabVisibility(sessionTag: string, visible: boolean) {
    this.methodCalled('setDeviceTabVisibility', [sessionTag, visible]);
  }
}
