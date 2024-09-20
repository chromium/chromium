// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for page-specific behaviors of
 * SetupSucceededPage.
 */

import 'chrome://multidevice-setup/strings.m.js';
import 'chrome://resources/ash/common/multidevice_setup/setup_succeeded_page.js';

import {BrowserProxyImpl} from 'chrome://resources/ash/common/multidevice_setup/multidevice_setup_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * @implements {BrowserProxy}
 */
export class TestMultideviceSetupBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['getProfileInfo', 'openMultiDeviceSettings']);
  }

  /** @override */
  openMultiDeviceSettings() {
    this.methodCalled('openMultiDeviceSettings');
  }

  /** @override */
  getProfileInfo() {
    this.methodCalled('getProfileInfo');
    return Promise.resolve({});
  }
}

suite('MultiDeviceSetup', () => {
  /**
   * SetupSucceededPage created before each test. Defined in setUp.
   * @type {?SetupSucceededPage}
   */
  let setupSucceededPageElement = null;
  /** @type {?TestMultideviceSetupBrowserProxy} */
  let browserProxy = null;

  setup(async () => {
    browserProxy = new TestMultideviceSetupBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    setupSucceededPageElement = document.createElement('setup-succeeded-page');
    document.body.appendChild(setupSucceededPageElement);
  });

  test('Settings link opens settings page', () => {
    setupSucceededPageElement.$$('#settings-link').click();
    return browserProxy.whenCalled('openMultiDeviceSettings');
  });
});
