// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isChromeOS} from 'chrome://resources/js/cr.m.js';

import {TestBrowserProxy} from '../test_browser_proxy.js';

/**
 * A test version of LifetimeBrowserProxy.
 */
export class TestLifetimeBrowserProxy extends TestBrowserProxy {
  constructor() {
    const methodNames = ['restart', 'relaunch'];
    if (isChromeOS) {
      methodNames.push('signOutAndRestart', 'factoryReset');
    }

    super(methodNames);
  }

  restart() {
    this.methodCalled('restart');
  }

  relaunch() {
    this.methodCalled('relaunch');
  }
}

if (isChromeOS) {
  TestLifetimeBrowserProxy.prototype.signOutAndRestart = function() {
    this.methodCalled('signOutAndRestart');
  };

  TestLifetimeBrowserProxy.prototype.factoryReset = function(
      requestTpmFirmwareUpdate) {
    this.methodCalled('factoryReset', requestTpmFirmwareUpdate);
  };
}
