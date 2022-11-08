// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * @implements {InternetPageBrowserProxy}
 */
export class TestInternetPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'showCarrierAccountDetail',
      'showPortalSignin',
      'showCellularSetupUI',
      'configureThirdPartyVpn',
      'addThirdPartyVpn',
      'requestGmsCoreNotificationsDisabledDeviceNames',
      'setGmsCoreNotificationsDisabledDeviceNamesCallback',
    ]);
  }

  /** @override */
  showCarrierAccountDetail(guid) {
    this.methodCalled('showCarrierAccountDetail');
  }

  /** @override */
  showPortalSignin(guid) {
    this.methodCalled('showPortalSignin', guid);
  }

  /** @override */
  showCellularSetupUI(guid) {
    this.methodCalled('showCellularSetupUI');
  }

  /** @override */
  configureThirdPartyVpn(guid) {
    this.methodCalled('configureThirdPartyVpn');
  }

  /** @override */
  addThirdPartyVpn(appId) {
    this.methodCalled('addThirdPartyVpn');
  }

  /** @override */
  requestGmsCoreNotificationsDisabledDeviceNames() {
    this.methodCalled('requestGmsCoreNotificationsDisabledDeviceNames');
  }

  /** @override */
  setGmsCoreNotificationsDisabledDeviceNamesCallback(callback) {
    this.methodCalled('setGmsCoreNotificationsDisabledDeviceNamesCallback');
  }
}
