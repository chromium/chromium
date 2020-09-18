// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.PluginVmBrowserProxy} */
class TestPluginVmBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getPluginVmSharedPathsDisplayText',
      'removePluginVmSharedPath',
      'wouldPermissionChangeRequireRelaunch',
      'setPluginVmPermission',
      'relaunchPluginVm',
    ]);
    this.removeSharedPathResult = true;
    this.pluginVmRunning = false;
    this.permissions = [true, true];  // [0]Camera, [1]Microphone
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
  wouldPermissionChangeRequireRelaunch(permissionSetting) {
    this.methodCalled(
        'wouldPermissionChangeRequireRelaunch', permissionSetting);
    return Promise.resolve(
        permissionSetting.proposedValue !==
            this.permissions[permissionSetting.permissionType] &&
        this.pluginVmRunning);
  }

  /** @override */
  setPluginVmPermission(permissionSetting) {
    this.methodCalled('setPluginVmPermission', permissionSetting);
    this.permissions[permissionSetting.permissionType] =
        permissionSetting.proposedValue;
  }

  /** @override */
  relaunchPluginVm() {
    this.methodCalled('relaunchPluginVm');
  }
}
