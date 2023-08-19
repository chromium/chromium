// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {MultiDeviceBrowserProxyImpl, PermissionsSetupStatus, SettingsMultidevicePermissionsSetupDialogElement, SetupFlowStatus} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertArrayEquals, assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.js';

suite('<settings-multidevice-permissions-setup-dialog>', () => {
  let browserProxy: TestMultideviceBrowserProxy;
  let permissionsSetupDialog: SettingsMultidevicePermissionsSetupDialogElement;
  let dialogBody: HTMLElement;
  let buttonContainer: HTMLElement;

  function simulateNotificationStatusChanged(status: PermissionsSetupStatus) {
    webUIListenerCallback(
        'settings.onNotificationAccessSetupStatusChanged', status);
    flush();
  }

  function simulateAppsStatusChanged(status: PermissionsSetupStatus) {
    webUIListenerCallback('settings.onAppsAccessSetupStatusChanged', status);
    flush();
  }

  function simulateCombinedStatusChanged(status: PermissionsSetupStatus) {
    webUIListenerCallback(
        'settings.onCombinedAccessSetupStatusChanged', status);
    flush();
  }

  function simulateFeatureSetupConnectionStatusChanged(
      status: PermissionsSetupStatus) {
    webUIListenerCallback(
        'settings.onFeatureSetupConnectionStatusChanged', status);
    flush();
  }

  function isExpectedFlowState(setupState: SetupFlowStatus) {
    return permissionsSetupDialog['flowState_'] === setupState;
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

    permissionsSetupDialog =
        document.createElement('settings-multidevice-permissions-setup-dialog');
    document.body.appendChild(permissionsSetupDialog);
    flush();
    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLElement>(
            '#dialogBody');
    assertTrue(!!dialog);
    dialogBody = dialog;
    const container =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLElement>(
            '#buttonContainer');
    assertTrue(!!container);
    buttonContainer = container;
  });

  teardown(() => {
    permissionsSetupDialog.remove();
  });

  test('Test cancel during connection', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();
    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTING);

    const cancelButton = queryCancelButton();
    assertTrue(!!cancelButton);
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    cancelButton.click();
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });

  test('Test failure during connection', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();
    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTING);

    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.TIMED_OUT_CONNECTING);

    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    const tryAgainButton = queryTryAgainButton();
    assertTrue(!!tryAgainButton);

    tryAgainButton.click();
    assertEquals(2, browserProxy.getCallCount('attemptFeatureSetupConnection'));

    flush();

    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_DISCONNECTED);

    const cancelButton = queryCancelButton();
    assertTrue(!!cancelButton);
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertTrue(!!queryTryAgainButton());

    cancelButton.click();
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });

  test('Test notification setup success flow', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();
    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.CONNECTION_REQUESTED);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateNotificationStatusChanged(PermissionsSetupStatus.CONNECTING);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton = queryDoneButton();
    assertTrue(!!doneButton);
    assertTrue(doneButton.disabled);
    assertNull(queryTryAgainButton());
    const title =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLElement>('#title');
    assertTrue(!!title);
    assertTrue(!!title.hasAttribute('aria-live'));
    assertTrue(!!title.hasAttribute('aria-describedby'));

    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();
    simulateNotificationStatusChanged(
        PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertNull(queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton1 = queryDoneButton();
    assertTrue(!!doneButton1);
    assertFalse(doneButton1.disabled);
    assertNull(queryTryAgainButton());

    // The feature becomes enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(1, browserProxy.getCallCount('setFeatureEnabledState'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    doneButton1.click();
    const dialog1 =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog1);
    assertFalse(dialog1.open);
  });

  test('Test notification setup cancel during connecting flow', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();
    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(PermissionsSetupStatus.CONNECTING);

    const cancelButton = queryCancelButton();
    assertTrue(!!cancelButton);
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    cancelButton.click();
    assertEquals(1, browserProxy.getCallCount('cancelNotificationSetup'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });

  test('Test notification setup failure during connecting flow', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.TIMED_OUT_CONNECTING);
    assertTrue(isExpectedFlowState(SetupFlowStatus.FINISHED));

    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    const tryAgainButton = queryTryAgainButton();
    assertTrue(!!tryAgainButton);

    tryAgainButton.click();
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
    assertEquals(2, browserProxy.getCallCount('attemptFeatureSetupConnection'));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertEquals(2, browserProxy.getCallCount('cancelFeatureSetupConnection'));
    assertEquals(2, browserProxy.getCallCount('attemptNotificationSetup'));

    flush();

    const cancelButton = queryCancelButton();
    assertTrue(!!cancelButton);
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.CONNECTION_DISCONNECTED);

    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertTrue(!!queryTryAgainButton());

    cancelButton.click();

    assertEquals(0, browserProxy.getCallCount('cancelNotificationSetup'));
    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });

  test('Test notification access prohibited', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    assertNull(queryCloseButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED);

    assertNull(queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    const closeButton = queryCloseButton();
    assertTrue(!!closeButton);

    closeButton.click();

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });

  test('Test apps setup success flow', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertTrue(!!queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptAppsSetup'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTION_REQUESTED);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTING);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateAppsStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton = queryDoneButton();
    assertTrue(!!doneButton);
    assertTrue(doneButton.disabled);
    assertNull(queryTryAgainButton());

    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    simulateAppsStatusChanged(PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertNull(queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton1 = queryDoneButton();
    assertTrue(!!doneButton1);
    assertFalse(doneButton1.disabled);
    assertNull(queryTryAgainButton());

    // The feature becomes enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(1, browserProxy.getCallCount('setFeatureEnabledState'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    doneButton1.click();
    const dialog1 =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog1);
    assertFalse(dialog1.open);
  });

  test('Test apps setup cancel during connecting flow', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptAppsSetup'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTING);

    const cancelButton = queryCancelButton();
    assertTrue(!!cancelButton);
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    cancelButton.click();
    assertEquals(1, browserProxy.getCallCount('cancelAppsSetup'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });

  test('Test apps setup failure during connecting flow', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptAppsSetup'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    simulateAppsStatusChanged(PermissionsSetupStatus.TIMED_OUT_CONNECTING);
    assertTrue(isExpectedFlowState(SetupFlowStatus.FINISHED));

    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    const tryAgainButton = queryTryAgainButton();
    assertTrue(!!tryAgainButton);

    tryAgainButton.click();
    assertEquals(1, browserProxy.getCallCount('attemptAppsSetup'));

    assertEquals(2, browserProxy.getCallCount('attemptFeatureSetupConnection'));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(2, browserProxy.getCallCount('cancelFeatureSetupConnection'));
    assertEquals(2, browserProxy.getCallCount('attemptAppsSetup'));


    flush();

    const cancelButton = queryCancelButton();
    assertTrue(!!cancelButton);
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTION_DISCONNECTED);

    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertTrue(!!queryTryAgainButton());

    cancelButton.click();
    assertEquals(0, browserProxy.getCallCount('cancelAppsSetup'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertFalse(dialog.open);
  });

  test('Test notification and apps setup success flow', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton = queryDoneButton();
    assertTrue(!!doneButton);
    assertTrue(doneButton.disabled);
    assertNull(queryTryAgainButton());

    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: true,
    });
    flush();

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    // The phone hub notification feature becomes enabled when the status
    // becomes PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(1, browserProxy.getCallCount('setFeatureEnabledState'));
    assertEquals(1, browserProxy.getCallCount('attemptAppsSetup'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: true,
    });
    flush();

    simulateAppsStatusChanged(PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertNull(queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton1 = queryDoneButton();
    assertTrue(!!doneButton1);
    assertFalse(doneButton1.disabled);
    assertNull(queryTryAgainButton());

    // The phone hub apps feature becomes enabled when the status
    // becomes PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(2, browserProxy.getCallCount('setFeatureEnabledState'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    doneButton1.click();
    const dialog1 =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog1);
    assertFalse(dialog1.open);
  });

  test('Test phone enabled but ChromeOS disabled screen lock', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: true,
      isChromeosScreenLockEnabled: false,
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(0, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.SET_LOCKSCREEN));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertTrue(!!queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
  });

  test('Test screen lock without pin number with next button', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: true,
      isChromeosScreenLockEnabled: true,
      isScreenLockEnabled_: true,
      flowState_: SetupFlowStatus.SET_LOCKSCREEN,
      isPinNumberSelected_: false,
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    getStartedButton.click();

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertFalse(permissionsSetupDialog.get('showSetupPinDialog_'));
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));
  });

  test('Test screen lock with pin number with next button', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: true,
      isChromeosScreenLockEnabled: true,
      isScreenLockEnabled_: true,
      flowState_: SetupFlowStatus.SET_LOCKSCREEN,
      isPinNumberSelected_: true,
    });
    flush();

    const screenLockSubpage = permissionsSetupDialog.shadowRoot!.querySelector(
        'settings-multidevice-screen-lock-subpage');
    assertTrue(!!screenLockSubpage);
    screenLockSubpage.dispatchEvent(new CustomEvent(
        'pin-number-selected', {detail: {isPinNumberSelected: true}}));
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    getStartedButton.click();

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertTrue(permissionsSetupDialog.get('showSetupPinDialog_'));
    assertEquals(0, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.SET_LOCKSCREEN));
  });

  test('Test screen lock with pin number done', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: true,
      isChromeosScreenLockEnabled: true,
      isScreenLockEnabled_: true,
      flowState_: SetupFlowStatus.SET_LOCKSCREEN,
      isPinNumberSelected_: true,
      isPinSet_: true,
      isPasswordDialogShowing: true,
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    getStartedButton.click();

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertFalse(permissionsSetupDialog.get('showSetupPinDialog_'));
    assertFalse(permissionsSetupDialog.isPasswordDialogShowing);
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));
  });

  test('Test phone and ChromeOS enabled screen lock', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: true,
      isChromeosScreenLockEnabled: true,
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    getStartedButton.click();
    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
  });

  test('Test phone disabled but ChromeOS enabled screen lock', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: false,
      isChromeosScreenLockEnabled: true,
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    getStartedButton.click();
    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
  });

  test('Test phone and ChromeOS disabled screen lock', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: false,
      isChromeosScreenLockEnabled: false,
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    getStartedButton.click();
    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
  });

  test('Test screen lock UI when Eche is disabled', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: true,
      isChromeosScreenLockEnabled: false,
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: false});
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    getStartedButton.click();
    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));
  });

  test('Test screen lock UI when handling NOTIFICATION_SETUP_MODE', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: true,
      isChromeosScreenLockEnabled: false,
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    getStartedButton.click();
    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertEquals(1, browserProxy.getCallCount('attemptNotificationSetup'));
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));
  });

  test('Test Camera Roll setup success flow', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: true,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptCombinedFeatureSetup'));
    assertArrayEquals(
        [true, false], browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

    simulateCombinedStatusChanged(PermissionsSetupStatus.CONNECTION_REQUESTED);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateCombinedStatusChanged(PermissionsSetupStatus.CONNECTING);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton = queryDoneButton();
    assertTrue(!!doneButton);
    assertTrue(doneButton.disabled);
    assertNull(queryTryAgainButton());
    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: true,
    });
    flush();

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertNull(queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton1 = queryDoneButton();
    assertTrue(!!doneButton1);
    assertFalse(doneButton1.disabled);
    assertNull(queryTryAgainButton());

    // The feature becomes enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(1, browserProxy.getCallCount('setFeatureEnabledState'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    doneButton1.click();
    const dialog1 =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog1);
    assertFalse(dialog1.open);
  });

  test('Test Camera Roll and Notifications combined setup success flow', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: true,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptCombinedFeatureSetup'));
    assertArrayEquals(
        [true, true], browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

    simulateCombinedStatusChanged(PermissionsSetupStatus.CONNECTION_REQUESTED);
    assertEquals(null, dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateCombinedStatusChanged(PermissionsSetupStatus.CONNECTING);
    assertEquals(null, dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertEquals(null, dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton = queryDoneButton();
    assertTrue(!!doneButton);
    assertTrue(doneButton.disabled);
    assertNull(queryTryAgainButton());
    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: true,
    });
    flush();

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertEquals(null, dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertNull(queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton1 = queryDoneButton();
    assertTrue(!!doneButton1);
    assertFalse(doneButton1.disabled);
    assertNull(queryTryAgainButton());

    // The features become enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(2, browserProxy.getCallCount('setFeatureEnabledState'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    doneButton1.click();
    const dialog1 =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog1);
    assertFalse(dialog1.open);
  });

  test(
      'Test Camera Roll, Notifications setup flow, notifications rejected',
      () => {
        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        assertTrue(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!queryCancelButton());
        const getStartedButton = queryGetStartedButton();
        assertTrue(!!getStartedButton);
        assertNull(queryDoneButton());
        assertNull(queryTryAgainButton());
        getStartedButton.click();

        assertEquals(
            1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

        simulateFeatureSetupConnectionStatusChanged(
            PermissionsSetupStatus.CONNECTION_ESTABLISHED);
        assertEquals(
            1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

        assertEquals(
            1, browserProxy.getCallCount('attemptCombinedFeatureSetup'));
        assertArrayEquals(
            [true, true],
            browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

        simulateCombinedStatusChanged(
            PermissionsSetupStatus
                .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
        assertEquals(
            null, dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!queryCancelButton());
        assertNull(queryGetStartedButton());
        const doneButton = queryDoneButton();
        assertTrue(!!doneButton);
        assertTrue(doneButton.disabled);
        assertNull(queryTryAgainButton());
        assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: true,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        simulateCombinedStatusChanged(
            PermissionsSetupStatus.CAMERA_ROLL_GRANTED_NOTIFICATION_REJECTED);
        assertEquals(
            null, dialogBody.querySelector('#start-setup-description'));
        assertNull(buttonContainer.querySelector('#learnMore'));
        assertNull(queryCancelButton());
        assertNull(queryGetStartedButton());
        const doneButton1 = queryDoneButton();
        assertTrue(!!doneButton1);
        assertFalse(doneButton1.disabled);
        assertNull(queryTryAgainButton());

        // Only Camera Roll is enabled.
        assertEquals(1, browserProxy.getCallCount('setFeatureEnabledState'));

        const dialog =
            permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
                '#dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.open);
        doneButton1.click();
        const dialog1 =
            permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
                '#dialog');
        assertTrue(!!dialog1);
        assertFalse(dialog1.open);
      });

  test('Test Camera Roll, Notifications and apps setup success flow', () => {
    // Simulate all features are granted by the user
    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: true,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));


    assertEquals(1, browserProxy.getCallCount('attemptCombinedFeatureSetup'));
    assertArrayEquals(
        [true, true], browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertEquals(null, dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton = queryDoneButton();
    assertTrue(!!doneButton);
    assertTrue(doneButton.disabled);
    assertNull(queryTryAgainButton());
    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: true,
    });
    flush();

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertEquals(null, dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    // The camera roll and notifications features become enabled when the
    // status becomes PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(2, browserProxy.getCallCount('setFeatureEnabledState'));

    assertEquals(1, browserProxy.getCallCount('attemptAppsSetup'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: true,
    });
    flush();

    simulateAppsStatusChanged(PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertEquals(null, dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertNull(queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton1 = queryDoneButton();
    assertTrue(!!doneButton1);
    assertFalse(doneButton1.disabled);
    assertNull(queryTryAgainButton());

    // The apps feature become enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(3, browserProxy.getCallCount('setFeatureEnabledState'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    doneButton1.click();
    const dialog1 =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog1);
    assertFalse(dialog1.open);
  });



  test(
      'Test Camera Roll, Notifications and Apps setup flow, all user rejected.',
      () => {
        // Simulate all features are rejected by the user
        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        assertTrue(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!queryCancelButton());
        const getStartedButton = queryGetStartedButton();
        assertTrue(!!getStartedButton);
        assertNull(queryDoneButton());
        assertNull(queryTryAgainButton());
        getStartedButton.click();

        assertEquals(
            1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

        simulateFeatureSetupConnectionStatusChanged(
            PermissionsSetupStatus.CONNECTION_ESTABLISHED);
        assertEquals(
            1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

        assertEquals(
            1, browserProxy.getCallCount('attemptCombinedFeatureSetup'));
        assertArrayEquals(
            [true, true],
            browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

        simulateCombinedStatusChanged(
            PermissionsSetupStatus
                .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
        assertEquals(
            null, dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!queryCancelButton());
        assertNull(queryGetStartedButton());
        const doneButton = queryDoneButton();
        assertTrue(!!doneButton);
        assertTrue(doneButton.disabled);
        assertNull(queryTryAgainButton());
        assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

        // Because user rejected, Camera Roll and Notification permissions not
        // granted.
        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        simulateCombinedStatusChanged(
            PermissionsSetupStatus.COMPLETED_USER_REJECTED);
        assertEquals(
            null, dialogBody.querySelector('#start-setup-description'));
        assertNull(buttonContainer.querySelector('#learnMore'));
        assertTrue(!!queryCancelButton());
        assertNull(queryGetStartedButton());
        assertNull(queryDoneButton());
        assertNull(queryTryAgainButton());

        // Should not enabled phone hub camera roll and notification feature
        assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

        assertEquals(1, browserProxy.getCallCount('attemptAppsSetup'));
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        simulateAppsStatusChanged(
            PermissionsSetupStatus.COMPLETED_USER_REJECTED);
        assertEquals(
            null, dialogBody.querySelector('#start-setup-description'));
        assertNull(buttonContainer.querySelector('#learnMore'));
        const cancelButton = queryCancelButton();
        assertTrue(!!cancelButton);
        assertNull(queryGetStartedButton());
        assertNull(queryDoneButton());
        assertTrue(!!queryTryAgainButton());

        // Should not enabled phone hub apps feature
        assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

        const dialog =
            permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
                '#dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.open);
        cancelButton.click();
        const dialog1 =
            permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
                '#dialog');
        assertTrue(!!dialog1);
        assertFalse(dialog1.open);
      });

  test('Test all setup flow, user rejected but grant app permission', () => {
    // Simulate user grants app permission only
    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: true,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptCombinedFeatureSetup'));
    assertArrayEquals(
        [true, true], browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertEquals(null, dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton = queryDoneButton();
    assertTrue(!!doneButton);
    assertTrue(doneButton.disabled);
    assertNull(queryTryAgainButton());
    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

    // Because user rejected, Camera Roll and Notification permissions not
    // granted.
    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: true,
    });
    flush();

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.COMPLETED_USER_REJECTED);
    assertEquals(null, dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());

    // Should not enabled phone hub camera roll and notification feature
    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

    assertEquals(1, browserProxy.getCallCount('attemptAppsSetup'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: true,
    });
    flush();

    simulateAppsStatusChanged(PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertEquals(null, dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    assertNull(queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton1 = queryDoneButton();
    assertTrue(!!doneButton1);
    assertFalse(doneButton1.disabled);
    assertNull(queryTryAgainButton());

    // The apps feature become enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(1, browserProxy.getCallCount('setFeatureEnabledState'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    doneButton1.click();
    const dialog1 =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog1);
    assertFalse(dialog1.open);
  });

  test(
      'Test all setup flow user granted others rejected cameraRoll permission',
      () => {
        // Simulate user grants app permission only
        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        assertTrue(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!queryCancelButton());
        const getStartedButton = queryGetStartedButton();
        assertTrue(!!getStartedButton);
        assertNull(queryDoneButton());
        assertNull(queryTryAgainButton());
        getStartedButton.click();

        assertEquals(
            1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

        simulateFeatureSetupConnectionStatusChanged(
            PermissionsSetupStatus.CONNECTION_ESTABLISHED);
        assertEquals(
            1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

        assertEquals(
            1, browserProxy.getCallCount('attemptCombinedFeatureSetup'));
        assertArrayEquals(
            [true, true],
            browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

        simulateCombinedStatusChanged(
            PermissionsSetupStatus
                .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
        assertEquals(
            null, dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!queryCancelButton());
        assertNull(queryGetStartedButton());
        const doneButton = queryDoneButton();
        assertTrue(!!doneButton);
        assertTrue(doneButton.disabled);
        assertNull(queryTryAgainButton());
        assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

        // Because user rejected, Camera Roll and Notification permissions not
        // granted.
        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: false,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        simulateCombinedStatusChanged(
            PermissionsSetupStatus.CAMERA_ROLL_REJECTED_NOTIFICATION_GRANTED);
        assertEquals(
            null, dialogBody.querySelector('#start-setup-description'));
        assertNull(buttonContainer.querySelector('#learnMore'));
        assertTrue(!!queryCancelButton());
        assertNull(queryGetStartedButton());
        assertNull(queryDoneButton());
        assertNull(queryTryAgainButton());

        // Should not enabled phone hub camera roll and notification feature
        assertEquals(1, browserProxy.getCallCount('setFeatureEnabledState'));

        assertEquals(1, browserProxy.getCallCount('attemptAppsSetup'));
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: false,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        simulateAppsStatusChanged(
            PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
        assertEquals(
            null, dialogBody.querySelector('#start-setup-description'));
        assertNull(buttonContainer.querySelector('#learnMore'));
        assertNull(queryCancelButton());
        assertNull(queryGetStartedButton());
        const doneButton1 = queryDoneButton();
        assertTrue(!!doneButton1);
        assertFalse(doneButton1.disabled);
        assertNull(queryTryAgainButton());

        // The apps feature become enabled when the status becomes
        // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
        assertEquals(2, browserProxy.getCallCount('setFeatureEnabledState'));

        const dialog =
            permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
                '#dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.open);
        doneButton1.click();
        const dialog1 =
            permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
                '#dialog');
        assertTrue(!!dialog1);
        assertFalse(dialog1.open);
      });

  test('Test all setup flow, operation failed or cancelled', () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: true,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    const getStartedButton = queryGetStartedButton();
    assertTrue(!!getStartedButton);
    assertNull(queryDoneButton());
    assertNull(queryTryAgainButton());
    getStartedButton.click();

    assertEquals(1, browserProxy.getCallCount('attemptFeatureSetupConnection'));
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(1, browserProxy.getCallCount('cancelFeatureSetupConnection'));

    assertEquals(1, browserProxy.getCallCount('attemptCombinedFeatureSetup'));
    assertArrayEquals(
        [true, true], browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!queryCancelButton());
    assertNull(queryGetStartedButton());
    const doneButton = queryDoneButton();
    assertTrue(!!doneButton);
    assertTrue(doneButton.disabled);
    assertNull(queryTryAgainButton());
    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

    // Because user rejected, Camera Roll and Notification permissions not
    // granted.
    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: true,
    });
    flush();

    simulateCombinedStatusChanged(PermissionsSetupStatus.FAILED_OR_CANCELLED);
    assertNull(dialogBody.querySelector('#start-setup-description'));
    assertNull(buttonContainer.querySelector('#learnMore'));
    const cancelButton = queryCancelButton();
    assertTrue(!!cancelButton);
    assertNull(queryGetStartedButton());
    assertNull(queryDoneButton());
    assertTrue(!!queryTryAgainButton());

    // Should not enabled phone hub camera roll and notification feature
    assertEquals(0, browserProxy.getCallCount('setFeatureEnabledState'));

    // Should not continue setup apps
    assertEquals(0, browserProxy.getCallCount('attemptAppsSetup'));

    const dialog =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);
    cancelButton.click();
    const dialog1 =
        permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
            '#dialog');
    assertTrue(!!dialog1);
    assertFalse(dialog1.open);
  });

  test(
      'Test dailog is closed when all permissions are graned on phone.', () => {
        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        assertTrue(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!queryCancelButton());
        assertTrue(!!queryGetStartedButton());
        assertNull(queryDoneButton());
        assertNull(queryTryAgainButton());

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: true,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        const dialog =
            permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
                '#dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.open);

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: false,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        const dialog1 =
            permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
                '#dialog');
        assertTrue(!!dialog1);
        assertTrue(dialog1.open);

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: false,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        const dialog2 =
            permissionsSetupDialog.shadowRoot!.querySelector<HTMLDialogElement>(
                '#dialog');
        assertTrue(!!dialog2);
        assertFalse(dialog2.open);
      });
});
