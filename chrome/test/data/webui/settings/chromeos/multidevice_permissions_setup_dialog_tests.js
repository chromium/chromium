// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.m.js';
// #import {MultiDeviceBrowserProxyImpl, NotificationAccessSetupOperationStatus} from 'chrome://os-settings/chromeos/os_settings.js';
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
   * @param {NotificationAccessSetupOperationStatus} status
   */
  function simulateStatusChanged(status) {
    cr.webUIListenerCallback(
        'settings.onNotificationAccessSetupStatusChanged', status);
    Polymer.dom.flush();
  }

  /** @return {boolean} */
  function isSetupInstructionsShownSeparately() {
    return permissionsSetupDialog.shouldShowSetupInstructionsSeparately_;
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

  test('Test success flow', async () => {
    assertTrue(isSetupInstructionsShownSeparately());
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.CONNECTION_REQUESTED);
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateStatusChanged(NotificationAccessSetupOperationStatus.CONNECTING);
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateStatusChanged(NotificationAccessSetupOperationStatus
                              .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertTrue(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);
    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The feature becomes enabled when the status becomes
    // NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);

    assertTrue(permissionsSetupDialog.$$('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test cancel during connecting flow', async () => {
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(NotificationAccessSetupOperationStatus.CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelNotificationSetup'), 1);

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test failure during connecting flow', async () => {
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.TIMED_OUT_CONNECTING);

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

    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.CONNECTION_DISCONNECTED);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelNotificationSetup'), 1);

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test notification access prohibited', async () => {
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    assertFalse(!!buttonContainer.querySelector('#closeButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.NOTIFICATION_ACCESS_PROHIBITED);

    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    assertTrue(!!buttonContainer.querySelector('#closeButton'));

    buttonContainer.querySelector('#closeButton').click();

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test phone enabled but ChromeOS disabled screen lock', async () => {
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    loadTimeData.overrideValues({isPhoneScreenLockEnabled: true});
    loadTimeData.overrideValues({isChromeosScreenLockEnabled: false});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 0);
  });

  test('Test phone and ChromeOS enabled screen lock', async () => {
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    loadTimeData.overrideValues({isPhoneScreenLockEnabled: true});
    loadTimeData.overrideValues({isChromeosScreenLockEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });

  test('Test phone disabled but ChromeOS enabled screen lock', async () => {
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    loadTimeData.overrideValues({isPhoneScreenLockEnabled: false});
    loadTimeData.overrideValues({isChromeosScreenLockEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });

  test('Test phone and ChromeOS disabled screen lock', async () => {
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    loadTimeData.overrideValues({isPhoneScreenLockEnabled: false});
    loadTimeData.overrideValues({isChromeosScreenLockEnabled: false});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });
});
