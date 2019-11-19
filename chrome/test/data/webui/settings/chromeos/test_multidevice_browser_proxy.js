// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('multidevice', function() {
  /**
   * Default Host device for PageContentData.
   */
  const HOST_DEVICE = 'Pixel XL';

  /**
   * Test value for messages for web permissions origin.
   */
  const TEST_ANDROID_SMS_ORIGIN = 'http://foo.com';

  /**
   * Builds fake pageContentData for the specified mode. If it is a mode
   * corresponding to a set host, it will set the hostDeviceName to the provided
   * name or else default to HOST_DEVICE.
   * @param {settings.MultiDeviceSettingsMode} mode
   * @param {string=} opt_hostDeviceName Overrides default if |mode| corresponds
   *     to a set host.
   * @return {!MultiDevicePageContentData}
   */
  function createFakePageContentData(mode, opt_hostDeviceName) {
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
  class TestMultideviceBrowserProxy extends TestBrowserProxy {
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
  }

  return {
    TestMultideviceBrowserProxy: TestMultideviceBrowserProxy,
    createFakePageContentData: createFakePageContentData,
    TEST_ANDROID_SMS_ORIGIN: TEST_ANDROID_SMS_ORIGIN,
    HOST_DEVICE: HOST_DEVICE
  };
});
