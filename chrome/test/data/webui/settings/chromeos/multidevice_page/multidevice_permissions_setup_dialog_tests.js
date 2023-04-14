// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceBrowserProxyImpl, PermissionsSetupStatus, SetupFlowStatus} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestMultideviceBrowserProxy} from './test_multidevice_browser_proxy.js';

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
  let dialogBody = null;

  /** @type {?HTMLElement} */
  let buttonContainer = null;

  /**
   * @param {PermissionsSetupStatus} status
   */
  function simulateNotificationStatusChanged(status) {
    webUIListenerCallback(
        'settings.onNotificationAccessSetupStatusChanged', status);
    flush();
  }

  /**
   * @param {PermissionsSetupStatus} status
   */
  function simulateAppsStatusChanged(status) {
    webUIListenerCallback('settings.onAppsAccessSetupStatusChanged', status);
    flush();
  }

  function simulateCombinedStatusChanged(status) {
    webUIListenerCallback(
        'settings.onCombinedAccessSetupStatusChanged', status);
    flush();
  }

  function simulateFeatureSetupConnectionStatusChanged(status) {
    webUIListenerCallback(
        'settings.onFeatureSetupConnectionStatusChanged', status);
    flush();
  }

  /**
   * @param {SetupFlowStatus} status
   */
  function isExpectedFlowState(setupState) {
    return permissionsSetupDialog.flowState_ === setupState;
  }

  setup(() => {
    PolymerTest.clearBody();
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.setInstanceForTesting(browserProxy);

    permissionsSetupDialog =
        document.createElement('settings-multidevice-permissions-setup-dialog');
    document.body.appendChild(permissionsSetupDialog);
    flush();
    dialogBody =
        assert(permissionsSetupDialog.shadowRoot.querySelector('#dialogBody'));
    buttonContainer = assert(
        permissionsSetupDialog.shadowRoot.querySelector('#buttonContainer'));
  });

  test('Test cancel during connection', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test('Test failure during connection', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.TIMED_OUT_CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#tryAgainButton').click();
    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 2);

    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_DISCONNECTED);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test('Test notification setup success flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.CONNECTION_REQUESTED);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateNotificationStatusChanged(PermissionsSetupStatus.CONNECTING);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(buttonContainer.querySelector('#doneButton').disabled);
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();
    simulateNotificationStatusChanged(
        PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(buttonContainer.querySelector('#doneButton').disabled);
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The feature becomes enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);

    assertTrue(permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test('Test notification setup cancel during connecting flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(PermissionsSetupStatus.CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelNotificationSetup'), 1);

    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test('Test notification setup failure during connecting flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();

    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.TIMED_OUT_CONNECTING);
    assertTrue(isExpectedFlowState(SetupFlowStatus.FINISHED));

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#tryAgainButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 2);

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 2);
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 2);

    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.CONNECTION_DISCONNECTED);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();

    assertEquals(browserProxy.getCallCount('cancelNotificationSetup'), 0);
    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test('Test notification access prohibited', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    assertFalse(!!buttonContainer.querySelector('#closeButton'));
    buttonContainer.querySelector('#getStartedButton').click();

    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.NOTIFICATION_ACCESS_PROHIBITED);

    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    assertTrue(!!buttonContainer.querySelector('#closeButton'));

    buttonContainer.querySelector('#closeButton').click();

    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test('Test apps setup success flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();

    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTION_REQUESTED);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTING);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateAppsStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(buttonContainer.querySelector('#doneButton').disabled);
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: false,
    });
    flush();

    simulateAppsStatusChanged(PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(buttonContainer.querySelector('#doneButton').disabled);
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The feature becomes enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);

    assertTrue(permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test('Test apps setup cancel during connecting flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();

    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    simulateAppsStatusChanged(PermissionsSetupStatus.CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#cancelButton').click();
    assertEquals(browserProxy.getCallCount('cancelAppsSetup'), 1);

    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test('Test apps setup failure during connecting flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();

    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    simulateAppsStatusChanged(PermissionsSetupStatus.TIMED_OUT_CONNECTING);
    assertTrue(isExpectedFlowState(SetupFlowStatus.FINISHED));

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#tryAgainButton').click();
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);

    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 2);

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 2);
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 2);


    flush();

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
    assertEquals(browserProxy.getCallCount('cancelAppsSetup'), 0);

    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test('Test notification and apps setup success flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();

    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(buttonContainer.querySelector('#doneButton').disabled);
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: true,
    });
    flush();

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The phone hub notification feature becomes enabled when the status
    // becomes PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: true,
    });
    flush();

    simulateAppsStatusChanged(PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(buttonContainer.querySelector('#doneButton').disabled);
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The phone hub apps feature becomes enabled when the status
    // becomes PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 2);

    assertTrue(permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test('Test phone enabled but ChromeOS disabled screen lock', async () => {
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
    buttonContainer.querySelector('#getStartedButton').click();

    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 0);
    assertTrue(isExpectedFlowState(SetupFlowStatus.SET_LOCKSCREEN));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
  });

  test('Test screen lock without pin number with next button', async () => {
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
    buttonContainer.querySelector('#getStartedButton').click();

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertFalse(permissionsSetupDialog.showSetupPinDialog_);
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));
  });

  test('Test screen lock with pin number with next button', async () => {
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

    const screenLockSubpage = permissionsSetupDialog.shadowRoot.querySelector(
        'settings-multidevice-screen-lock-subpage');
    screenLockSubpage.dispatchEvent(new CustomEvent(
        'pin-number-selected', {detail: {isPinNumberSelected: true}}));
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertTrue(permissionsSetupDialog.showSetupPinDialog_);
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 0);
    assertTrue(isExpectedFlowState(SetupFlowStatus.SET_LOCKSCREEN));
  });

  test('Test screen lock with pin number done', async () => {
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
    buttonContainer.querySelector('#getStartedButton').click();

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertFalse(permissionsSetupDialog.showSetupPinDialog_);
    assertFalse(permissionsSetupDialog.isPasswordDialogShowing);
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));
  });

  test('Test phone and ChromeOS enabled screen lock', async () => {
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
    buttonContainer.querySelector('#getStartedButton').click();
    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });

  test('Test phone disabled but ChromeOS enabled screen lock', async () => {
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
    buttonContainer.querySelector('#getStartedButton').click();
    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });

  test('Test phone and ChromeOS disabled screen lock', async () => {
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
    buttonContainer.querySelector('#getStartedButton').click();
    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });

  test('Test screen lock UI when Eche is disabled', async () => {
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
    buttonContainer.querySelector('#getStartedButton').click();
    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);

    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));
  });

  test(
      'Test screen lock UI when handling NOTIFICATION_SETUP_MODE', async () => {
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
        buttonContainer.querySelector('#getStartedButton').click();
        simulateFeatureSetupConnectionStatusChanged(
            PermissionsSetupStatus.CONNECTION_ESTABLISHED);

        assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));
      });

  test('Test Camera Roll setup success flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: true,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();

    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertEquals(browserProxy.getCallCount('attemptCombinedFeatureSetup'), 1);
    assertArrayEquals(
        [true, false], browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

    simulateCombinedStatusChanged(PermissionsSetupStatus.CONNECTION_REQUESTED);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateCombinedStatusChanged(PermissionsSetupStatus.CONNECTING);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(buttonContainer.querySelector('#doneButton').disabled);
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: true,
    });
    flush();

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertFalse(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(buttonContainer.querySelector('#doneButton').disabled);
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

    // The feature becomes enabled when the status becomes
    // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);

    assertTrue(permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test(
      'Test Camera Roll and Notifications combined setup success flow',
      async () => {
        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        assertTrue(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        buttonContainer.querySelector('#getStartedButton').click();

        assertEquals(
            browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

        simulateFeatureSetupConnectionStatusChanged(
            PermissionsSetupStatus.CONNECTION_ESTABLISHED);
        assertEquals(
            browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

        assertEquals(
            browserProxy.getCallCount('attemptCombinedFeatureSetup'), 1);
        assertArrayEquals(
            [true, true],
            browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

        simulateCombinedStatusChanged(
            PermissionsSetupStatus.CONNECTION_REQUESTED);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        simulateCombinedStatusChanged(PermissionsSetupStatus.CONNECTING);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        simulateCombinedStatusChanged(
            PermissionsSetupStatus
                .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertTrue(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: false,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        simulateCombinedStatusChanged(
            PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertFalse(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        // The features become enabled when the status becomes
        // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 2);

        assertTrue(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
        buttonContainer.querySelector('#doneButton').click();
        assertFalse(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
      });

  test(
      'Test Camera Roll, Notifications setup flow, notifications rejected',
      async () => {
        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        assertTrue(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        buttonContainer.querySelector('#getStartedButton').click();

        assertEquals(
            browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

        simulateFeatureSetupConnectionStatusChanged(
            PermissionsSetupStatus.CONNECTION_ESTABLISHED);
        assertEquals(
            browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

        assertEquals(
            browserProxy.getCallCount('attemptCombinedFeatureSetup'), 1);
        assertArrayEquals(
            [true, true],
            browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

        simulateCombinedStatusChanged(
            PermissionsSetupStatus
                .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertTrue(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: true,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        simulateCombinedStatusChanged(
            PermissionsSetupStatus.CAMERA_ROLL_GRANTED_NOTIFICATION_REJECTED);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertFalse(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        // Only Camera Roll is enabled.
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);

        assertTrue(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
        buttonContainer.querySelector('#doneButton').click();
        assertFalse(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
      });

  test(
      'Test Camera Roll, Notifications and apps setup success flow',
      async () => {
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
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        buttonContainer.querySelector('#getStartedButton').click();

        assertEquals(
            browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

        simulateFeatureSetupConnectionStatusChanged(
            PermissionsSetupStatus.CONNECTION_ESTABLISHED);
        assertEquals(
            browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);


        assertEquals(
            browserProxy.getCallCount('attemptCombinedFeatureSetup'), 1);
        assertArrayEquals(
            [true, true],
            browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

        simulateCombinedStatusChanged(
            PermissionsSetupStatus
                .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertTrue(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: false,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        simulateCombinedStatusChanged(
            PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        // The camera roll and notifications features become enabled when the
        // status becomes PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 2);

        assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: false,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        simulateAppsStatusChanged(
            PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertFalse(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        // The apps feature become enabled when the status becomes
        // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 3);

        assertTrue(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
        buttonContainer.querySelector('#doneButton').click();
        assertFalse(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
      });



  test(
      'Test Camera Roll, Notifications and Apps setup flow, all user rejected.',
      async () => {
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
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        buttonContainer.querySelector('#getStartedButton').click();

        assertEquals(
            browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

        simulateFeatureSetupConnectionStatusChanged(
            PermissionsSetupStatus.CONNECTION_ESTABLISHED);
        assertEquals(
            browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

        assertEquals(
            browserProxy.getCallCount('attemptCombinedFeatureSetup'), 1);
        assertArrayEquals(
            [true, true],
            browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

        simulateCombinedStatusChanged(
            PermissionsSetupStatus
                .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertTrue(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

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
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        // Should not enabled phone hub camera roll and notification feature
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

        assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);
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
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

        // Should not enabled phone hub apps feature
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

        assertTrue(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
        buttonContainer.querySelector('#cancelButton').click();
        assertFalse(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
      });

  test(
      'Test all setup flow, user rejected but grant app permission',
      async () => {
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
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        buttonContainer.querySelector('#getStartedButton').click();

        assertEquals(
            browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

        simulateFeatureSetupConnectionStatusChanged(
            PermissionsSetupStatus.CONNECTION_ESTABLISHED);
        assertEquals(
            browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

        assertEquals(
            browserProxy.getCallCount('attemptCombinedFeatureSetup'), 1);
        assertArrayEquals(
            [true, true],
            browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

        simulateCombinedStatusChanged(
            PermissionsSetupStatus
                .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertTrue(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

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
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        // Should not enabled phone hub camera roll and notification feature
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

        assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        simulateAppsStatusChanged(
            PermissionsSetupStatus.COMPLETED_SUCCESSFULLY);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertFalse(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        // The apps feature become enabled when the status becomes
        // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);

        assertTrue(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
        buttonContainer.querySelector('#doneButton').click();
        assertFalse(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
      });

  test(
      'Test all setup flow user granted others rejected cameraRoll permission',
      async () => {
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
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        buttonContainer.querySelector('#getStartedButton').click();

        assertEquals(
            browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
        assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

        simulateFeatureSetupConnectionStatusChanged(
            PermissionsSetupStatus.CONNECTION_ESTABLISHED);
        assertEquals(
            browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

        assertEquals(
            browserProxy.getCallCount('attemptCombinedFeatureSetup'), 1);
        assertArrayEquals(
            [true, true],
            browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

        simulateCombinedStatusChanged(
            PermissionsSetupStatus
                .SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertTrue(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

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
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        // Should not enabled phone hub camera roll and notification feature
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 1);

        assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);
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
        assertFalse(!!dialogBody.querySelector('#start-setup-description'));
        assertFalse(!!buttonContainer.querySelector('#learnMore'));
        assertFalse(!!buttonContainer.querySelector('#cancelButton'));
        assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
        assertTrue(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(buttonContainer.querySelector('#doneButton').disabled);
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        // The apps feature become enabled when the status becomes
        // PermissionsSetupStatus.COMPLETED_SUCCESSFULLY.
        assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 2);

        assertTrue(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
        buttonContainer.querySelector('#doneButton').click();
        assertFalse(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
      });

  test('Test all setup flow, operation failed or cancelled', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: true,
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();

    assertEquals(browserProxy.getCallCount('attemptFeatureSetupConnection'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_CONNECTION));

    simulateFeatureSetupConnectionStatusChanged(
        PermissionsSetupStatus.CONNECTION_ESTABLISHED);
    assertEquals(browserProxy.getCallCount('cancelFeatureSetupConnection'), 1);

    assertEquals(browserProxy.getCallCount('attemptCombinedFeatureSetup'), 1);
    assertArrayEquals(
        [true, true], browserProxy.getArgs('attemptCombinedFeatureSetup')[0]);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_COMBINED));

    simulateCombinedStatusChanged(
        PermissionsSetupStatus.SENT_MESSAGE_TO_PHONE_AND_WAITING_FOR_RESPONSE);
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertTrue(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(buttonContainer.querySelector('#doneButton').disabled);
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

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
    assertFalse(!!dialogBody.querySelector('#start-setup-description'));
    assertFalse(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    // Should not enabled phone hub camera roll and notification feature
    assertEquals(browserProxy.getCallCount('setFeatureEnabledState'), 0);

    // Should not continue setup apps
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 0);

    assertTrue(permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
    buttonContainer.querySelector('#cancelButton').click();
    assertFalse(
        permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
  });

  test(
      'Test dailog is closed when all permissions are graned on phone.',
      async () => {
        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        assertTrue(!!dialogBody.querySelector('#start-setup-description'));
        assertTrue(!!buttonContainer.querySelector('#learnMore'));
        assertTrue(!!buttonContainer.querySelector('#cancelButton'));
        assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
        assertFalse(!!buttonContainer.querySelector('#doneButton'));
        assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: true,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        assertTrue(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: false,
          showAppStreaming: true,
          combinedSetupSupported: true,
        });
        flush();

        assertTrue(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);

        permissionsSetupDialog.setProperties({
          showCameraRoll: false,
          showNotifications: false,
          showAppStreaming: false,
          combinedSetupSupported: true,
        });
        flush();

        assertFalse(
            permissionsSetupDialog.shadowRoot.querySelector('#dialog').open);
      });
});
