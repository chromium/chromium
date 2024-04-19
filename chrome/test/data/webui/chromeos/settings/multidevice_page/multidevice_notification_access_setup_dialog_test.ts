// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {MultiDeviceBrowserProxyImpl, NotificationAccessSetupOperationStatus, SettingsMultideviceNotificationAccessSetupDialogElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.js';

suite('<settings-multidevice-notification-access-setup-dialog>', () => {
  let browserProxy: TestMultideviceBrowserProxy;
  let notificationAccessSetupDialog:
      SettingsMultideviceNotificationAccessSetupDialogElement;
  let buttonContainer: HTMLElement;

  function simulateStatusChanged(
      status: NotificationAccessSetupOperationStatus) {
    webUIListenerCallback(
        'settings.onNotificationAccessSetupStatusChanged', status);
    flush();
  }

  function isSetupInstructionsShownSeparately(): boolean {
    return notificationAccessSetupDialog.get(
        'shouldShowSetupInstructionsSeparately_');
  }

  function queryCancelButton(): HTMLButtonElement|null {
    return buttonContainer.querySelector<HTMLButtonElement>('#cancelButton');
  }

  function queryGetStartedButton(): HTMLButtonElement|null {
    return buttonContainer.querySelector<HTMLButtonElement>(
        '#getStartedButton');
  }

  function queryDoneButton(): HTMLButtonElement|null {
    return buttonContainer.querySelector<HTMLButtonElement>('#doneButton');
  }

  function queryTryAgainButton(): HTMLButtonElement|null {
    return buttonContainer.querySelector<HTMLButtonElement>('#tryAgainButton');
  }

  function queryCloseButton(): HTMLButtonElement|null {
    return buttonContainer.querySelector<HTMLButtonElement>('#closeButton');
  }

  setup(() => {
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);

    notificationAccessSetupDialog = document.createElement(
        'settings-multidevice-notification-access-setup-dialog');
    document.body.appendChild(notificationAccessSetupDialog);
    flush();
    const container =
        notificationAccessSetupDialog.shadowRoot!.querySelector<HTMLElement>(
            '#buttonContainer');
    assertTrue(!!container);
    buttonContainer = container;
  });

  test('Test success flow', async () => {
    assertTrue(isSetupInstructionsShownSeparately());
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertEquals(null, queryDoneButton());
    assertEquals(null, queryTryAgainButton());
    getStartedButton.click();
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));

    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.CONNECTION_REQUESTED);
    assertTrue(isSetupInstructionsShownSeparately());
    assertTrue(!!queryCancelButton());
    assertEquals(null, queryGetStartedButton());
    assertEquals(null, queryDoneButton());
    assertEquals(null, queryTryAgainButton());

    simulateStatusChanged(NotificationAccessSetupOperationStatus.CONNECTING);
    assertTrue(isSetupInstructionsShownSeparately());
    assertTrue(!!queryCancelButton());
    assertEquals(null, queryGetStartedButton());
    assertEquals(null, queryDoneButton());
    assertEquals(null, queryTryAgainButton());

    simulateStatusChanged(NotificationAccessSetupOperationStatus
                              .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertTrue(isSetupInstructionsShownSeparately());
    assertTrue(!!queryCancelButton());
    assertEquals(null, queryGetStartedButton());
    assertEquals(null, queryDoneButton());
    assertEquals(null, queryTryAgainButton());

    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));
    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(isSetupInstructionsShownSeparately());
    assertEquals(null, queryCancelButton());
    assertEquals(null, queryGetStartedButton());
    assertTrue(!!queryDoneButton());
    assertEquals(null, queryTryAgainButton());

    // The feature becomes enabled when the status becomes
    // NotificationAccessSetupOperationStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(1, browserProxy.getCallCount('setFeatureEnabledState'));

    const dialog = notificationAccessSetupDialog.shadowRoot!
                       .querySelector<HTMLDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    const doneButton = queryDoneButton();
    assertTrue(!!doneButton);
    doneButton.click();
    assertFalse(dialog.open);
  });

  test('Test cancel during connecting flow', async () => {
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertEquals(null, queryDoneButton());
    assertEquals(null, queryTryAgainButton());
    getStartedButton.click();
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));

    simulateStatusChanged(NotificationAccessSetupOperationStatus.CONNECTING);

    const cancelButton = queryCancelButton();
    assertTrue(!!cancelButton);
    assertEquals(null, queryGetStartedButton());
    assertEquals(null, queryDoneButton());
    assertEquals(null, queryTryAgainButton());

    cancelButton.click();
    assertEquals(1, browserProxy.getCallCount('cancelNotificationSetup'));

    const dialog = notificationAccessSetupDialog.shadowRoot!
                       .querySelector<HTMLDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });

  test('Test failure during connecting flow', async () => {
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertEquals(null, queryDoneButton());
    assertEquals(null, queryTryAgainButton());
    getStartedButton.click();
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));

    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.TIMED_OUT_CONNECTING);

    assertTrue(!!queryCancelButton());
    assertEquals(null, queryGetStartedButton());
    assertEquals(null, queryDoneButton());
    const tryAgainButton = queryTryAgainButton();
    assertTrue(!!tryAgainButton);

    tryAgainButton.click();
    assertEquals(2, browserProxy.getCallCount('attemptNotificationSetup'));

    flush();

    assertTrue(!!queryCancelButton());
    assertEquals(null, queryGetStartedButton());
    assertEquals(null, queryDoneButton());
    assertEquals(null, queryTryAgainButton());

    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.CONNECTION_DISCONNECTED);

    const cancelButton = queryCancelButton();
    assertTrue(!!cancelButton);
    assertEquals(null, queryGetStartedButton());
    assertEquals(null, queryDoneButton());
    assertTrue(!!queryTryAgainButton());

    cancelButton.click();
    assertEquals(1, browserProxy.getCallCount('cancelNotificationSetup'));

    const dialog = notificationAccessSetupDialog.shadowRoot!
                       .querySelector<HTMLDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });

  test('Test notification access prohibited', async () => {
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertEquals(null, queryDoneButton());
    assertEquals(null, queryTryAgainButton());
    assertEquals(null, queryCloseButton());
    getStartedButton.click();
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));

    simulateStatusChanged(
        NotificationAccessSetupOperationStatus.NOTIFICATION_ACCESS_PROHIBITED);

    assertEquals(null, queryCancelButton());
    assertEquals(null, queryGetStartedButton());
    assertEquals(null, queryDoneButton());
    assertEquals(null, queryTryAgainButton());
    const closeButton = queryCloseButton();
    assertTrue(!!closeButton);

    closeButton.click();

    const dialog = notificationAccessSetupDialog.shadowRoot!
                       .querySelector<HTMLDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });
});
