// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PluginVmBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPluginVmBrowserProxy extends TestBrowserProxy implements
    PluginVmBrowserProxy {
  private pluginVmRunning: boolean;
  constructor() {
    super([
      'isRelaunchNeededForNewPermissions',
      'relaunchPluginVm',
    ]);
    this.pluginVmRunning = false;
  }

  setPluginVmRunning(pluginVmRunning: boolean): void {
    this.pluginVmRunning = pluginVmRunning;
  }

  isRelaunchNeededForNewPermissions(): Promise<boolean> {
    this.methodCalled('isRelaunchNeededForNewPermissions');
    return Promise.resolve(this.pluginVmRunning);
  }

  relaunchPluginVm(): void {
    this.methodCalled('relaunchPluginVm');
  }
}
