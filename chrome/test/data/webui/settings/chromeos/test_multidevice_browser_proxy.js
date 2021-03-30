// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// #import {MultiDeviceSettingsMode, MultiDeviceFeature, MultiDevicePageContentData} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {PrintServerResult} from 'chrome://os-settings/chromeos/lazy_load.js';
// clang-format on

cr.define('multidevice', function() {
  /**
   * Default Host device for PageContentData.
   */
  /* #export */ const HOST_DEVICE = 'Pixel XL';

  /**
   * Test value for messages for web permissions origin.
   */
  /* #export */ const TEST_ANDROID_SMS_ORIGIN = 'http://foo.com';

  /**
   * Builds fake pageContentData for the specified mode. If it is a mode
   * corresponding to a set host, it will set the hostDeviceName to the provided
   * name or else default to HOST_DEVICE.
   * @param {settings.MultiDeviceSettingsMode} mode
   * @param {string=} opt_hostDeviceName Overrides default if |mode| corresponds
   *     to a set host.
   * @return {!MultiDevicePageContentData}
   */
  /* #export */ function createFakePageContentData(mode, opt_hostDeviceName) {
    const pageContentData = {mode: mode};
    if ([
          settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
          settings.MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
          settings.MultiDeviceSettingsMode.HOST_SET_VERIFIED,
        ].includes(mode)) {
      pageContentData.hostDeviceName = opt_hostDeviceName || HOST_DEVICE;
    }
    return pageContentData;
  }

  /**
   * @implements {settings.MultideviceBrowserProxy}
   * Note: Only showMultiDeviceSetupDialog is used by the multidevice-page
   * element.
   */
  /* #export */ class TestMultideviceBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'showMultiDeviceSetupDialog',
        'getPageContentData',
        'setFeatureEnabledState',
        'setUpAndroidSms',
        'getSmartLockSignInEnabled',
        'setSmartLockSignInEnabled',
        'getSmartLockSignInAllowed',
        'getAndroidSmsInfo',
        'attemptNotificationSetup',
        'cancelNotificationSetup',
      ]);
      this.data = createFakePageContentData(
          settings.MultiDeviceSettingsMode.NO_HOST_SET);
      this.androidSmsInfo = {origin: TEST_ANDROID_SMS_ORIGIN, enabled: true};
      this.smartLockSignInAllowed = true;
    }

    /** @override */
    getPageContentData() {
      this.methodCalled('getPageContentData');
      return Promise.resolve(this.data);
    }

    /** @override */
    showMultiDeviceSetupDialog() {
      this.methodCalled('showMultiDeviceSetupDialog');
    }

    /** @override */
    setFeatureEnabledState(feature, enabled, opt_authToken) {
      this.methodCalled(
          'setFeatureEnabledState', [feature, enabled, opt_authToken]);
      if (feature === settings.MultiDeviceFeature.MESSAGES) {
        this.androidSmsInfo.enabled = enabled;
        cr.webUIListenerCallback(
            'settings.onAndroidSmsInfoChange', this.androidSmsInfo);
      }
    }

    /** @override */
    setUpAndroidSms() {
      this.methodCalled('setUpAndroidSms');
    }

    /** @override */
    getSmartLockSignInEnabled() {
      this.methodCalled('getSmartLockSignInEnabled');
      return Promise.resolve(true);
    }

    /** @override */
    setSmartLockSignInEnabled(enabled, opt_authToken) {
      this.methodCalled('setSmartLockSignInEnabled', [enabled, opt_authToken]);
      cr.webUIListenerCallback('smart-lock-signin-enabled-changed', enabled);
    }

    /** @override */
    getSmartLockSignInAllowed() {
      this.methodCalled('getSmartLockSignInAllowed');
      return Promise.resolve(this.smartLockSignInAllowed);
    }

    /** @override */
    getAndroidSmsInfo() {
      this.methodCalled('getAndroidSmsInfo');
      return Promise.resolve(this.androidSmsInfo);
    }

    /** @override */
    attemptNotificationSetup() {
      this.methodCalled('attemptNotificationSetup');
    }

    /** @override */
    cancelNotificationSetup() {
      this.methodCalled('cancelNotificationSetup');
    }

    /**
     * @param {settings.MultiDeviceFeature} state
     */
    setInstantTetheringStateForTest(state) {
      this.data.instantTetheringState = state;
      cr.webUIListenerCallback(
          'settings.updateMultidevicePageContentData',
          Object.assign({}, this.data));
    }
  }

  // #cr_define_end
  return {
    TestMultideviceBrowserProxy: TestMultideviceBrowserProxy,
    createFakePageContentData: createFakePageContentData,
    TEST_ANDROID_SMS_ORIGIN: TEST_ANDROID_SMS_ORIGIN,
    HOST_DEVICE: HOST_DEVICE
  };
});
