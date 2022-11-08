// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * A test version of LifetimeBrowserProxy.
 */
export class TestLifetimeBrowserProxy extends TestBrowserProxy {
  constructor() {
    const methodNames = ['restart', 'relaunch'];
    methodNames.push('signOutAndRestart', 'factoryReset');
    super(methodNames);
  }

  /** @override */
  restart() {
    this.methodCalled('restart');
  }

  /** @override */
  relaunch() {
    this.methodCalled('relaunch');
  }

  /** @override */
  signOutAndRestart() {
    this.methodCalled('signOutAndRestart');
  }

  /** @override */
  factoryReset(requestTpmFirmwareUpdate) {
    this.methodCalled('factoryReset', requestTpmFirmwareUpdate);
  }
}
