// Copyright 2020 The Chromium Authors. All rights reserved.
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
 * Suite of tests for multidevice-notification-access-setup-dialog element.
 */
suite('Multidevice', () => {
  /** @type {?TestMultideviceBrowserProxy} */
  let browserProxy;

  /** @type {?MultideviceNotificationsOptInDialog} */
  let notificationAccessSetupDialog = null;

  /** @type {?HTMLElement} */
  let buttonContainer = null;

  /**
   * @param {NotificationAccessSetupOperationStatus} status
   */
  function simulateStatusChanged(status) {
    cr.webUIListenerCallback('settings.onNotificationAccessSetupStatusChanged',
        status);
    Polymer.dom.flush();
  }

  /** @return {boolean} */
  function isSetupInstructionsShownSeparately() {
    return notificationAccessSetupDialog.shouldShowSetupInstructionsSeparately_;
  }

  setup(() => {
    PolymerTest.clearBody();
    browserProxy = new multidevice.TestMultideviceBrowserProxy();
    settings.MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    notificationAccessSetupDialog =
        document.createElement(
            'settings-multidevice-notification-access-setup-dialog');
    document.body.appendChild(notificationAccessSetupDialog);
    Polymer.dom.flush();
    buttonContainer =
        assert(notificationAccessSetupDialog.$$('#buttonContainer'));
  });

  test('Test success flow', async () => {
    assertTrue(isSetupInstructionsShownSeparately());
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);

    simulateStatusChanged(
      NotificationAccessSetupOperationStatus.CONNECTION_REQUESTED);
    assertTrue(isSetupInstructionsShownSeparately());
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateStatusChanged(NotificationAccessSetupOperationStatus.CONNECTING);
    assertTrue(isSetupInstructionsShownSeparately());
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateStatusChanged(NotificationAccessSetupOperationStatus.
        SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertTrue(isSetupInstructionsShownSeparately());
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);
    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(isSetupInstructionsShownSeparately());
    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The feature becomes enabled when the status becomes
    // NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);

    assertTrue(notificationAccessSetupDialog.$$('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(notificationAccessSetupDialog.$$('#dialog').open);
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

    assertFalse(notificationAccessSetupDialog.$$('#dialog').open);
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

    assertFalse(notificationAccessSetupDialog.$$('#dialog').open);
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

    assertFalse(notificationAccessSetupDialog.$$('#dialog').open);
  });
});
