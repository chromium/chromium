// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.CrostiniBrowserProxy} */
class TestCrostiniBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestCrostiniInstallerView',
      'requestRemoveCrostini',
      'getCrostiniSharedPathsDisplayText',
      'getCrostiniSharedUsbDevices',
      'setCrostiniUsbDeviceShared',
      'removeCrostiniSharedPath',
      'exportCrostiniContainer',
      'importCrostiniContainer',
    ]);
    this.sharedUsbDevices = [];
  }

  /** @override */
  requestCrostiniInstallerView() {
    this.methodCalled('requestCrostiniInstallerView');
  }

  /** override */
  requestRemoveCrostini() {
    this.methodCalled('requestRemoveCrostini');
  }

  /** override */
  getCrostiniSharedPathsDisplayText(paths) {
    this.methodCalled('getCrostiniSharedPathsDisplayText');
    return Promise.resolve(paths.map(path => path + '-displayText'));
  }

  /** @override */
  getCrostiniSharedUsbDevices() {
    this.methodCalled('getCrostiniSharedUsbDevices');
    return Promise.resolve(this.sharedUsbDevices);
  }

  /** @override */
  setCrostiniUsbDeviceShared(guid, shared) {
    this.methodCalled('setCrostiniUsbDeviceShared', [guid, shared]);
  }

  /** override */
  removeCrostiniSharedPath(vmName, path) {
    this.methodCalled('removeCrostiniSharedPath', [vmName, path]);
  }

  /** @override */
  requestCrostiniInstallerStatus() {
    cr.webUIListenerCallback('crostini-installer-status-changed', false);
  }

  /** @override */
  requestCrostiniExportImportOperationStatus() {
    cr.webUIListenerCallback(
        'crostini-export-import-operation-status-changed', false);
  }

  /** override */
  exportCrostiniContainer() {
    this.methodCalled('exportCrostiniContainer');
  }

  /** override */
  importCrostiniContainer() {
    this.methodCalled('importCrostiniContainer');
  }
}
