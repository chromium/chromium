// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// clang-format on

/** @implements {DeviceNameBrowserProxy} */
/* #export */ class TestDeviceNameBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getDeviceNameMetadata',
    ]);

    /** @private {String} */
    this.deviceName_ = '';
  }

  /** @param {String} deviceName */
  setDeviceName(deviceName) {
    this.deviceName_ = deviceName;
  }

  /** @override */
  getDeviceNameMetadata() {
    this.methodCalled('getDeviceNameMetadata');
    return Promise.resolve({deviceName: this.deviceName_});
  }
}
