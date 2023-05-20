// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InternetPageBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestInternetPageBrowserProxy extends TestBrowserProxy implements
    InternetPageBrowserProxy {
  constructor() {
    super([
      'showCarrierAccountDetail',
      'showPortalSignin',
      'showCellularSetupUi',
      'configureThirdPartyVpn',
      'addThirdPartyVpn',
      'requestGmsCoreNotificationsDisabledDeviceNames',
      'setGmsCoreNotificationsDisabledDeviceNamesCallback',
    ]);
  }

  showCarrierAccountDetail(guid: string): void {
    this.methodCalled('showCarrierAccountDetail', guid);
  }

  showPortalSignin(guid: string): void {
    this.methodCalled('showPortalSignin', guid);
  }

  showCellularSetupUi(guid: string): void {
    this.methodCalled('showCellularSetupUi', guid);
  }

  configureThirdPartyVpn(guid: string): void {
    this.methodCalled('configureThirdPartyVpn', guid);
  }

  addThirdPartyVpn(appId: string): void {
    this.methodCalled('addThirdPartyVpn', appId);
  }

  requestGmsCoreNotificationsDisabledDeviceNames(): void {
    this.methodCalled('requestGmsCoreNotificationsDisabledDeviceNames');
  }

  setGmsCoreNotificationsDisabledDeviceNamesCallback(
      callback: (deviceNames: string[]) => void): void {
    this.methodCalled(
        'setGmsCoreNotificationsDisabledDeviceNamesCallback', callback);
  }
}
