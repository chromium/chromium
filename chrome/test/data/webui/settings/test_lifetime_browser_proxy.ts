// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LifetimeBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * A test version of LifetimeBrowserProxy.
 */
export class TestLifetimeBrowserProxy extends TestBrowserProxy implements
    LifetimeBrowserProxy {
  constructor() {
    super([
      'restart', 'relaunch',

      // <if expr="chromeos">
      'signOutAndRestart', 'factoryReset',
      // </if>
    ]);
  }

  restart() {
    this.methodCalled('restart');
  }

  relaunch() {
    this.methodCalled('relaunch');
  }

  // <if expr="chromeos">
  signOutAndRestart() {
    this.methodCalled('signOutAndRestart');
  }

  factoryReset(requestTpmFirmwareUpdate: boolean) {
    this.methodCalled('signOutAndRestart', requestTpmFirmwareUpdate);
  }
  //  </if>
}
