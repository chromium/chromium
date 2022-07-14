// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../../test_browser_proxy.js';

/**
 * @implements {InternetPageBrowserProxy}
 */
export class TestInternetPageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'showCarrierAccountDetail',
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
