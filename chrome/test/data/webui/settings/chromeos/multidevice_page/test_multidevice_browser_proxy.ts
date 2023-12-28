// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceBrowserProxy, MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, PhoneHubFeatureAccessStatus, PhoneHubPermissionsSetupAction, PhoneHubPermissionsSetupFeatureCombination, PhoneHubPermissionsSetupFlowScreens} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Default Host device for PageContentData.
 */
export const HOST_DEVICE = 'Pixel XL';

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

  constructor() {
    super([
      'showMultiDeviceSetupDialog',
      'getPageContentData',
      'setFeatureEnabledState',
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

  setNotificationAccessStatusForTesting(status: PhoneHubFeatureAccessStatus):
      void {
    this.data_.notificationAccessStatus = status;
  }

  setIsPhoneHubPermissionsDialogSupportedForTesting(isSupported: boolean):
      void {
    this.data_.isPhoneHubPermissionsDialogSupported = isSupported;
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
    return Promise.resolve(true);
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
