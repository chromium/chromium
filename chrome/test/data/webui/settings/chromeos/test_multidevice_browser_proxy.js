// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceFeature, MultiDeviceSettingsMode} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Default Host device for PageContentData.
 */
export const HOST_DEVICE = 'Pixel XL';

/**
 * Test value for messages for web permissions origin.
 */
export const TEST_ANDROID_SMS_ORIGIN = 'http://foo.com';

/**
 * Builds fake pageContentData for the specified mode. If it is a mode
 * corresponding to a set host, it will set the hostDeviceName to the provided
 * name or else default to HOST_DEVICE.
 * @param {MultiDeviceSettingsMode} mode
 * @param {string=} opt_hostDeviceName Overrides default if |mode| corresponds
 *     to a set host.
 * @return {!MultiDevicePageContentData}
 */
export function createFakePageContentData(mode, opt_hostDeviceName) {
  const pageContentData = {mode: mode};
  if ([
        MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
        MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
        MultiDeviceSettingsMode.HOST_SET_VERIFIED,
      ].includes(mode)) {
    pageContentData.hostDeviceName = opt_hostDeviceName || HOST_DEVICE;
  }
  return pageContentData;
}

/**
 * @implements {MultideviceBrowserProxy}
 * Note: Only showMultiDeviceSetupDialog is used by the multidevice-page
 * element.
 */
export class TestMultideviceBrowserProxy extends TestBrowserProxy {
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
      'attemptAppsSetup',
      'cancelAppsSetup',
      'attemptCombinedFeatureSetup',
      'cancelCombinedFeatureSetup',
      'attemptFeatureSetupConnection',
      'cancelFeatureSetupConnection',
      'logPhoneHubPermissionSetUpScreenAction',
      'logPhoneHubPermissionOnboardingSetupMode',
      'logPhoneHubPermissionOnboardingSetupResult',
    ]);
    this.data = createFakePageContentData(MultiDeviceSettingsMode.NO_HOST_SET);
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
    if (feature === MultiDeviceFeature.MESSAGES) {
      this.androidSmsInfo.enabled = enabled;
      webUIListenerCallback(
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
    webUIListenerCallback('smart-lock-signin-enabled-changed', enabled);
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

  /** @override */
  attemptAppsSetup() {
    this.methodCalled('attemptAppsSetup');
  }

  /** @override */
  cancelAppsSetup() {
    this.methodCalled('cancelAppsSetup');
  }

  /** @override */
  attemptCombinedFeatureSetup(cameraRoll, notifications) {
    this.methodCalled(
        'attemptCombinedFeatureSetup', [cameraRoll, notifications]);
  }

  /** @override */
  cancelCombinedFeatureSetup() {
    this.methodCalled('cancelCombinedFeatureSetup');
  }

  /** @override */
  attemptFeatureSetupConnection() {
    this.methodCalled('attemptFeatureSetupConnection');
  }

  /** @override */
  cancelFeatureSetupConnection() {
    this.methodCalled('cancelFeatureSetupConnection');
  }

  /**
   * @param {MultiDeviceFeature} state
   */
  setInstantTetheringStateForTest(state) {
    this.data.instantTetheringState = state;
    webUIListenerCallback(
        'settings.updateMultidevicePageContentData',
        Object.assign({}, this.data));
  }

  /** @override */
  logPhoneHubPermissionSetUpScreenAction(screen, action) {
    this.methodCalled(
        'logPhoneHubPermissionSetUpScreenAction', [screen, action]);
  }

  /** @override */
  logPhoneHubPermissionOnboardingSetupMode(setup_mode) {
    this.methodCalled('logPhoneHubPermissionOnboardingSetupMode', [setup_mode]);
  }

  /** @override */
  logPhoneHubPermissionOnboardingSetupResult(completed_mode) {
    this.methodCalled(
        'logPhoneHubPermissionOnboardingSetupResult', [completed_mode]);
  }
}
