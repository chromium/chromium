// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * @implements {BluetoothPageBrowserProxy}
 */
export class TestBluetoothPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'isDeviceBlockedByPolicy',
    ]);
    /** @private {boolean} */
    this.isDeviceBlockedByPolicy_ = false;
  }

  /** @override */
  isDeviceBlockedByPolicy(address) {
    this.methodCalled('isDeviceBlockedByPolicy');
    return Promise.resolve(this.isDeviceBlockedByPolicy_);
  }

  /**
   * @param {boolean} isDeviceBlockedByPolicy
   */
  setIsDeviceBlockedByPolicyForTest(isDeviceBlockedByPolicy) {
    this.isDeviceBlockedByPolicy_ = isDeviceBlockedByPolicy;
  }
}
