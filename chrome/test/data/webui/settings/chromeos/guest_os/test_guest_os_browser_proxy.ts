// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GuestOsBrowserProxy, GuestOsSharedUsbDevice} from 'chrome://os-settings/lazy_load.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestGuestOsBrowserProxy extends TestBrowserProxy implements
    GuestOsBrowserProxy {
  sharedUsbDevices: GuestOsSharedUsbDevice[] = [];
  private removeSharedPathResult_: boolean = true;
  constructor() {
    super([
      'getGuestOsSharedPathsDisplayText',
      'notifyGuestOsSharedUsbDevicesPageReady',
      'setGuestOsUsbDeviceShared',
      'removeGuestOsSharedPath',
    ]);
  }

  getGuestOsSharedPathsDisplayText(paths: string[]): Promise<string[]> {
    this.methodCalled('getGuestOsSharedPathsDisplayText');
    return Promise.resolve(paths.map(path => path + '-displayText'));
  }

  notifyGuestOsSharedUsbDevicesPageReady(): void {
    this.methodCalled('notifyGuestOsSharedUsbDevicesPageReady');
    webUIListenerCallback(
        'guest-os-shared-usb-devices-changed', this.sharedUsbDevices);
  }

  setGuestOsUsbDeviceShared(
      vmName: string, containerName: string, guid: string,
      shared: boolean): void {
    this.methodCalled(
        'setGuestOsUsbDeviceShared', [vmName, containerName, guid, shared]);
  }

  removeGuestOsSharedPath(vmName: string, path: string): Promise<boolean> {
    this.methodCalled('removeGuestOsSharedPath', [vmName, path]);
    return Promise.resolve(this.removeSharedPathResult_);
  }

  stubRemoveSharedPathResult(pathRemoved: boolean): void {
    this.removeSharedPathResult_ = pathRemoved;
  }
}
