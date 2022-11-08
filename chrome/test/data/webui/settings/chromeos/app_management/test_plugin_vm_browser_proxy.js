// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {PluginVmBrowserProxy} */
export class TestPluginVmBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'isRelaunchNeededForNewPermissions',
      'relaunchPluginVm',
    ]);
    this.pluginVmRunning = false;
  }

  /** @override */
  isRelaunchNeededForNewPermissions() {
    this.methodCalled('isRelaunchNeededForNewPermissions');
    return Promise.resolve(this.pluginVmRunning);
  }

  /** @override */
  relaunchPluginVm() {
    this.methodCalled('relaunchPluginVm');
  }
}
