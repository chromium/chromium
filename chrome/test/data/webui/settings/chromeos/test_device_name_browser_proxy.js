// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SetDeviceNameResult} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDeviceNameBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'notifyReadyForDeviceName',
      'attemptSetDeviceName',
    ]);

    /** @private {string} */
    this.deviceName_ = '';

    /** @private {!SetDeviceNameResult} */
    this.deviceNameResult_ = SetDeviceNameResult.UPDATE_SUCCESSFUL;
  }

  /** @param {!SetDeviceNameResult} deviceNameResult */
  setDeviceNameResult(deviceNameResult) {
    this.deviceNameResult_ = deviceNameResult;
  }

  /** @return {string} */
  getDeviceName() {
    return this.deviceName_;
  }

  /** @override */
  notifyReadyForDeviceName() {
    this.methodCalled('notifyReadyForDeviceName');
  }

  /** @override */
  attemptSetDeviceName(name) {
    if (this.deviceNameResult_ === SetDeviceNameResult.UPDATE_SUCCESSFUL) {
      this.deviceName_ = name;
    }

    this.methodCalled('attemptSetDeviceName');
    return Promise.resolve(this.deviceNameResult_);
  }
}
