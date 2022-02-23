// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.m.js';
// #import {MultiDeviceBrowserProxyImpl, PermissionsSetupStatus, PhoneHubPermissionsSetupMode} from 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

/**
 * @fileoverview
 * Suite of tests for multidevice-permissions-setup-dialog_tests element.
 */
suite('Multidevice', () => {
  /** @type {?TestMultideviceBrowserProxy} */
  let browserProxy;

  /** @type {?MultidevicePermissionsOptInDialog} */
  let permissionsSetupDialog = null;

  /** @type {?HTMLElement} */
  let buttonContainer = null;

  /**
   * @param {PermissionsSetupStatus} status
   */
  function simulateStatusChanged(status) {
    cr.webUIListenerCallback(
        'settings.onNotificationAccessSetupStatusChanged', status);
    Polymer.dom.flush();
  }

  /**
   * @param {PermissionsSetupStatus} status
   */
  function simulateAppsStatusChanged(status) {
    cr.webUIListenerCallback('settings.onAppsAccessSetupStatusChanged', status);
    Polymer.dom.flush();
  }


  /** @return {boolean} */
  function isSetupInstructionsShownSeparately() {
    return permissionsSetupDialog.shouldShowSetupInstructionsSeparately_;
  }

  /** @return {boolean} */
  function isNotificationItemShowen() {
    return permissionsSetupDialog.shouldShowNotificationItem_;
  }

  /** @return {boolean} */
  function isAppsItemShowen() {
    return permissionsSetupDialog.shouldShowAppsItem_;
  }

  setup(() => {
    PolymerTest.clearBody();
    browserProxy = new multidevice.TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    permissionsSetupDialog =
        document.createElement('settings-multidevice-permissions-setup-dialog');
    document.body.appendChild(permissionsSetupDialog);
    Polymer.dom.flush();
    buttonContainer = assert(permissionsSetupDialog.$$('#buttonContainer'));
  });

  test('Test notification setup success flow', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.NOTIFICATION_SETUP_MODE;
    assertTrue(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertFalse(isSetupInstructionsShownSeparately());
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(PermissionsSetupStatus.CONNECTION_REQUESTED);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateStatusChanged(PermissionsSetupStatus.CONNECTING);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);
    simulateStatusChanged(PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertFalse(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The feature becomes enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);

    assertTrue(permissionsSetupDialog.$$('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test notification setup cancel during connecting flow', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.NOTIFICATION_SETUP_MODE;
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(PermissionsSetupStatus.CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelNotificationSetup'), 1);

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test notificaiton setup failure during connecting flow', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.NOTIFICATION_SETUP_MODE;
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(PermissionsSetupStatus.TIMED_OUT_CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#tryAgainButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 2);

    Polymer.dom.flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateStatusChanged(PermissionsSetupStatus.CONNECTION_DISCONNECTED);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelNotificationSetup'), 1);

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test notification access prohibited', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.NOTIFICATION_SETUP_MODE;
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    assertFalse(!!buttonContainer.querySelector('#closeButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(
        PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED);

    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    assertTrue(!!buttonContainer.querySelector('#closeButton'));

    buttonContainer.querySelector('#closeButton').click();

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test apps setup success flow', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.APPS_SETUP_MODE;
    assertFalse(isNotificationItemShowen());
    assertTrue(isAppsItemShowen());
    assertFalse(isSetupInstructionsShownSeparately());
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTION_REQUESTED);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTING);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateAppsStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);
    simulateAppsStatusChanged(PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertFalse(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The feature becomes enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);

    assertTrue(permissionsSetupDialog.$$('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test apps setup cancel during connecting flow', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.APPS_SETUP_MODE;
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelAppsSetup'), 1);

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test apps setup failure during connecting flow', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.APPS_SETUP_MODE;
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);

    simulateAppsStatusChanged(PermissionsSetupStatus.TIMED_OUT_CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#tryAgainButton').click();
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 2);

    Polymer.dom.flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTION_DISCONNECTED);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelAppsSetup'), 1);

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test notification and apps setup success flow', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.ALL_PERMISSIONS_SETUP_MODE;
    assertTrue(isNotificationItemShowen());
    assertTrue(isAppsItemShowen());
    assertFalse(isSetupInstructionsShownSeparately());
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

    simulateStatusChanged(PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The phone hub notification feature becomes enabled when the status
    // becomes PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);

    simulateAppsStatusChanged(PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(isNotificationItemShowen());
    assertFalse(isAppsItemShowen());
    assertFalse(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The phone hub apps feature becomes enabled when the status
    // becomes PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 2);

    assertTrue(permissionsSetupDialog.$$('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });


  test('Test phone enabled but ChromeOS disabled screen lock', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.ALL_PERMISSIONS_SETUP_MODE;
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    loadTimeData.overrideValues({isPhoneScreenLockEnabled: true});
    loadTimeData.overrideValues({isChromeosScreenLockEnabled: false});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 0);
  });

  test('Test phone and ChromeOS enabled screen lock', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.ALL_PERMISSIONS_SETUP_MODE;
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    loadTimeData.overrideValues({isPhoneScreenLockEnabled: true});
    loadTimeData.overrideValues({isChromeosScreenLockEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });

  test('Test phone disabled but ChromeOS enabled screen lock', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.ALL_PERMISSIONS_SETUP_MODE;
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    loadTimeData.overrideValues({isPhoneScreenLockEnabled: false});
    loadTimeData.overrideValues({isChromeosScreenLockEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });

  test('Test phone and ChromeOS disabled screen lock', async () => {
    permissionsSetupDialog.phonePermissionSetupMode =
        PhoneHubPermissionsSetupMode.ALL_PERMISSIONS_SETUP_MODE;
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    loadTimeData.overrideValues({isPhoneScreenLockEnabled: false});
    loadTimeData.overrideValues({isChromeosScreenLockEnabled: false});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });
});
