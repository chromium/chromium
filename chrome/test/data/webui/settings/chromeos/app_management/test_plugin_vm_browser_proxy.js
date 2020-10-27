// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.PluginVmBrowserProxy} */
class TestPluginVmBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getPluginVmSharedPathsDisplayText',
      'removePluginVmSharedPath',
      'isRelaunchNeededForNewPermissions',
      'setPluginVmPermission',
      'relaunchPluginVm',
    ]);
    this.removeSharedPathResult = true;
    this.pluginVmRunning = false;
  }

  /** @override */
  getPluginVmSharedPathsDisplayText(paths) {
    this.methodCalled('getPluginVmSharedPathsDisplayText', paths);
    return Promise.resolve(true);
  }

  /** @override */
  removePluginVmSharedPath(vmName, path) {
    this.methodCalled('removePluginVmSharedPath', vmName, path);
    return Promise.resolve(this.removeSharedPathResult);
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
