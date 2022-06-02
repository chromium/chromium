// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MultiDeviceBrowserProxyImpl, PermissionsSetupStatus, SetupFlowStatus} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

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

  /**
   * @param {SetupFlowStatus} status
   */
  function isExpectedFlowState(setupState) {
    return permissionsSetupDialog.flowState_ === setupState;
  }

  setup(() => {
    PolymerTest.clearBody();
    browserProxy = new TestMultideviceBrowserProxy();
    MultiDeviceBrowserProxyImpl.instance_ = browserProxy;

    permissionsSetupDialog =
        document.createElement('settings-multidevice-permissions-setup-dialog');
    document.body.appendChild(permissionsSetupDialog);
    flush();
    dialogBody = assert(permissionsSetupDialog.$$('#dialogBody'));
    buttonContainer = assert(permissionsSetupDialog.$$('#buttonContainer'));
  });

  test('Test notification setup success flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
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

    assertTrue(permissionsSetupDialog.$$('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test notification setup cancel during connecting flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false
    });
    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
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

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test notification setup failure during connecting flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false
    });
    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
    assertTrue(
        isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));

    simulateNotificationStatusChanged(
        PermissionsSetupStatus.TIMED_OUT_CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#tryAgainButton').click();
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
    assertEquals(browserProxy.getCallCount('cancelNotificationSetup'), 1);

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test notification access prohibited', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false
    });
    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    assertFalse(!!buttonContainer.querySelector('#closeButton'));
    buttonContainer.querySelector('#getStartedButton').click();
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

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test apps setup success flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: false
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
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

    assertTrue(permissionsSetupDialog.$$('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test apps setup cancel during connecting flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: false
    });
    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

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
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: false,
      showAppStreaming: true,
      combinedSetupSupported: false
    });
    flush();

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptAppsSetup'), 1);
    assertTrue(isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_APPS));

    simulateAppsStatusChanged(PermissionsSetupStatus.TIMED_OUT_CONNECTING);

    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertFalse(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertTrue(!!buttonContainer.querySelector('#tryAgainButton'));

    buttonContainer.querySelector('#tryAgainButton').click();
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
    assertEquals(browserProxy.getCallCount('cancelAppsSetup'), 1);

    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test notification and apps setup success flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
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

    assertTrue(permissionsSetupDialog.$$('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test('Test phone enabled but ChromeOS disabled screen lock', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: true,
      isChromeosScreenLockEnabled: false
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();

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
      isPinNumberSelected_: false
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();

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
      isPinNumberSelected_: true
    });
    flush();

    const screenLockSubpage =
        permissionsSetupDialog.$$('settings-multidevice-screen-lock-subpage');
    screenLockSubpage.dispatchEvent(new CustomEvent(
        'pin-number-selected', {detail: {isPinNumberSelected: true}}));
    loadTimeData.overrideValues({isEcheAppEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();

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
      isSetPinDone_: true,
      isPasswordDialogShowing: true
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();

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
      isChromeosScreenLockEnabled: true
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });

  test('Test phone disabled but ChromeOS enabled screen lock', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: false,
      isChromeosScreenLockEnabled: true
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });

  test('Test phone and ChromeOS disabled screen lock', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: true,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: false,
      isChromeosScreenLockEnabled: false
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: true});
    buttonContainer.querySelector('#getStartedButton').click();
    assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
  });

  test('Test screen lock UI when Eche is disabled', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: false,
      showNotifications: true,
      showAppStreaming: false,
      combinedSetupSupported: false,
      isPhoneScreenLockEnabled: true,
      isChromeosScreenLockEnabled: false
    });
    flush();

    loadTimeData.overrideValues({isEcheAppEnabled: false});
    buttonContainer.querySelector('#getStartedButton').click();

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
          isChromeosScreenLockEnabled: false
        });
        flush();

        loadTimeData.overrideValues({isEcheAppEnabled: true});
        buttonContainer.querySelector('#getStartedButton').click();

        assertEquals(browserProxy.getCallCount('attemptNotificationSetup'), 1);
        assertTrue(
            isExpectedFlowState(SetupFlowStatus.WAIT_FOR_PHONE_NOTIFICATION));
      });

  test('Test Camera Roll setup success flow', async () => {
    permissionsSetupDialog.setProperties({
      showCameraRoll: true,
      showNotifications: false,
      showAppStreaming: false,
      combinedSetupSupported: true
    });
    flush();

    assertTrue(!!dialogBody.querySelector('#start-setup-description'));
    assertTrue(!!buttonContainer.querySelector('#learnMore'));
    assertTrue(!!buttonContainer.querySelector('#cancelButton'));
    assertTrue(!!buttonContainer.querySelector('#getStartedButton'));
    assertFalse(!!buttonContainer.querySelector('#doneButton'));
    assertFalse(!!buttonContainer.querySelector('#tryAgainButton'));
    buttonContainer.querySelector('#getStartedButton').click();
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

    assertTrue(permissionsSetupDialog.$$('#dialog').open);
    buttonContainer.querySelector('#doneButton').click();
    assertFalse(permissionsSetupDialog.$$('#dialog').open);
  });

  test(
      'Test Camera Roll and Notifications combined setup success flow',
      async () => {
        permissionsSetupDialog.setProperties({
          showCameraRoll: true,
          showNotifications: true,
          showAppStreaming: false,
          combinedSetupSupported: true
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

        assertTrue(permissionsSetupDialog.$$('#dialog').open);
        buttonContainer.querySelector('#doneButton').click();
        assertFalse(permissionsSetupDialog.$$('#dialog').open);
      });
});
