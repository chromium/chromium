// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AndroidSmsInfo, MultiDeviceBrowserProxy, MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, PhoneHubPermissionsSetupAction, PhoneHubPermissionsSetupFeatureCombination, PhoneHubPermissionsSetupFlowScreens} from 'chrome://os-settings/os_settings.js';
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
 * @param hostDeviceName Overrides default if |mode| corresponds
 *     to a set host.
 */
export function createFakePageContentData(
    mode: MultiDeviceSettingsMode,
    hostDeviceName?: string): MultiDevicePageContentData {
  const pageContentData = {mode: mode} as MultiDevicePageContentData;
  if ([
        MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_SERVER,
        MultiDeviceSettingsMode.HOST_SET_WAITING_FOR_VERIFICATION,
        MultiDeviceSettingsMode.HOST_SET_VERIFIED,
      ].includes(mode)) {
    pageContentData.hostDeviceName = hostDeviceName || HOST_DEVICE;
  }
  return pageContentData;
}

/**
 * Note: Only showMultiDeviceSetupDialog is used by the multidevice-page
 * element.
 */
export class TestMultideviceBrowserProxy extends TestBrowserProxy implements
    MultiDeviceBrowserProxy {
  private data_ =
      createFakePageContentData(MultiDeviceSettingsMode.NO_HOST_SET);
  private androidSmsInfo_:
      AndroidSmsInfo = {origin: TEST_ANDROID_SMS_ORIGIN, enabled: true};

  constructor() {
    super([
      'showMultiDeviceSetupDialog',
      'getPageContentData',
      'setFeatureEnabledState',
      'setUpAndroidSms',
      'getAndroidSmsInfo',
      'attemptNotificationSetup',
      'cancelNotificationSetup',
      'attemptAppsSetup',
      'cancelAppsSetup',
      'attemptCombinedFeatureSetup',
      'cancelCombinedFeatureSetup',
      'attemptFeatureSetupConnection',
      'cancelFeatureSetupConnection',
      'showBrowserSyncSettings',
      'logPhoneHubPermissionSetUpScreenAction',
      'logPhoneHubPermissionOnboardingSetupMode',
      'logPhoneHubPermissionOnboardingSetupResult',
      'getSmartLockSignInAllowed',
    ]);
  }

  getPageContentData(): Promise<MultiDevicePageContentData> {
    this.methodCalled('getPageContentData');
    return Promise.resolve(this.data_);
  }

  showMultiDeviceSetupDialog(): void {
    this.methodCalled('showMultiDeviceSetupDialog');
  }

  setFeatureEnabledState(
      feature: MultiDeviceFeature, enabled: boolean,
      authToken?: string): Promise<boolean> {
    this.methodCalled('setFeatureEnabledState', [feature, enabled, authToken]);
    if (feature === MultiDeviceFeature.MESSAGES) {
      this.androidSmsInfo_.enabled = enabled;
      webUIListenerCallback(
          'settings.onAndroidSmsInfoChange', this.androidSmsInfo_);
    }
    return Promise.resolve(true);
  }

  setUpAndroidSms(): void {
    this.methodCalled('setUpAndroidSms');
  }

  getAndroidSmsInfo(): Promise<AndroidSmsInfo> {
    this.methodCalled('getAndroidSmsInfo');
    return Promise.resolve(this.androidSmsInfo_);
  }

  attemptNotificationSetup(): void {
    this.methodCalled('attemptNotificationSetup');
  }

  cancelNotificationSetup(): void {
    this.methodCalled('cancelNotificationSetup');
  }

  attemptAppsSetup(): void {
    this.methodCalled('attemptAppsSetup');
  }

  cancelAppsSetup(): void {
    this.methodCalled('cancelAppsSetup');
  }

  attemptCombinedFeatureSetup(
      showCameraRoll: boolean, showNotifications: boolean): void {
    this.methodCalled(
        'attemptCombinedFeatureSetup', [showCameraRoll, showNotifications]);
  }

  cancelCombinedFeatureSetup(): void {
    this.methodCalled('cancelCombinedFeatureSetup');
  }

  attemptFeatureSetupConnection(): void {
    this.methodCalled('attemptFeatureSetupConnection');
  }

  cancelFeatureSetupConnection(): void {
    this.methodCalled('cancelFeatureSetupConnection');
  }

  setInstantTetheringStateForTest(state: MultiDeviceFeatureState): void {
    this.data_.instantTetheringState = state;
    webUIListenerCallback(
        'settings.updateMultidevicePageContentData',
        Object.assign({}, this.data_));
  }

  showBrowserSyncSettings() {
    this.methodCalled('showBrowserSyncSettings');
  }

  logPhoneHubPermissionSetUpScreenAction(
      screen: PhoneHubPermissionsSetupFlowScreens,
      action: PhoneHubPermissionsSetupAction): void {
    this.methodCalled(
        'logPhoneHubPermissionSetUpScreenAction', [screen, action]);
  }

  logPhoneHubPermissionSetUpButtonClicked(
      setupMode: PhoneHubPermissionsSetupFeatureCombination): void {
    chrome.send('logPhoneHubPermissionSetUpButtonClicked', [setupMode]);
  }

  logPhoneHubPermissionOnboardingSetupMode(
      setupMode: PhoneHubPermissionsSetupFeatureCombination): void {
    this.methodCalled('logPhoneHubPermissionOnboardingSetupMode', [setupMode]);
  }

  logPhoneHubPermissionOnboardingSetupResult(
      completedMode: PhoneHubPermissionsSetupFeatureCombination): void {
    this.methodCalled(
        'logPhoneHubPermissionOnboardingSetupResult', [completedMode]);
  }

  getSmartLockSignInAllowed(): Promise<boolean> {
    return Promise.resolve(true);
  }

  removeHostDevice(): void {}

  retryPendingHostSetup(): void {}
}
