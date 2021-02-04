// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// clang-format on

/** @implements {settings.GuestOsBrowserProxy} */
/* #export */ class TestGuestOsBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getGuestOsSharedPathsDisplayText',
      'notifyGuestOsSharedUsbDevicesPageReady',
      'setGuestOsUsbDeviceShared',
      'removeGuestOsSharedPath',
    ]);
    /** @type {!Array<!GuestOsSharedUsbDevice>} */
    this.sharedUsbDevices = [];
    this.removeSharedPathResult = true;
  }

  /** override */
  getGuestOsSharedPathsDisplayText(paths) {
    this.methodCalled('getGuestOsSharedPathsDisplayText');
    return Promise.resolve(paths.map(path => path + '-displayText'));
  }

  /** @override */
  notifyGuestOsSharedUsbDevicesPageReady() {
    this.methodCalled('notifyGuestOsSharedUsbDevicesPageReady');
    cr.webUIListenerCallback(
        'guest-os-shared-usb-devices-changed', this.sharedUsbDevices);
  }

  /** @override */
  setGuestOsUsbDeviceShared(vmName, guid, shared) {
    this.methodCalled('setGuestOsUsbDeviceShared', [vmName, guid, shared]);
  }

  /** override */
  removeGuestOsSharedPath(vmName, path) {
    this.methodCalled('removeGuestOsSharedPath', [vmName, path]);
    return Promise.resolve(this.removeSharedPathResult);
  }
}
