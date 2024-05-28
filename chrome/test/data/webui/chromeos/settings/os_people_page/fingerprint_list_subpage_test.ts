// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {FingerprintBrowserProxyImpl, FingerprintResultType, FingerprintSetupStep, SettingsFingerprintListSubpageElement, SettingsSetupFingerprintDialogElement} from 'chrome://os-settings/lazy_load.js';
import {CrDialogElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {DomRepeatEvent, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeQuickUnlockPrivate} from '../fake_quick_unlock_private.js';

import {TestFingerprintBrowserProxy} from './test_fingerprint_browser_proxy.js';

suite('<settings-fingerprint-list-subpage>', () => {
  let fingerprintList: SettingsFingerprintListSubpageElement;
  let dialog: SettingsSetupFingerprintDialogElement;
  let addAnotherButton: HTMLButtonElement;
  let browserProxy: TestFingerprintBrowserProxy;

  function createFakeEvent(index: number, label?: string) {
    return {model: {index: index, item: label || ''}} as DomRepeatEvent<string>;
  }

  function openDialog() {
    const actionButton =
        fingerprintList.shadowRoot!.querySelector<HTMLButtonElement>(
            '.action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    flush();
    const dialogElement = fingerprintList.shadowRoot!.querySelector(
        'settings-setup-fingerprint-dialog');
    assertTrue(!!dialogElement);
    dialog = dialogElement;
    const button = dialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '#addAnotherButton');
    assertTrue(!!button);
    addAnotherButton = button;
  }

  setup(async () => {
    browserProxy = new TestFingerprintBrowserProxy();
    FingerprintBrowserProxyImpl.setInstanceForTesting(browserProxy);

    fingerprintList =
        document.createElement('settings-fingerprint-list-subpage');
    document.body.appendChild(fingerprintList);
    flush();
    await browserProxy.whenCalled('getFingerprintsList');
    assertEquals(0, fingerprintList.get('fingerprints_').length);
    browserProxy.resetResolver('getFingerprintsList');
  });

  test('EnrollingFingerprintLottieAnimation', async () => {
    loadTimeData.overrideValues({fingerprintUnlockEnabled: true});
    openDialog();
    await browserProxy.whenCalled('startEnroll');
    const dialogButton =
        dialog.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialogButton);
    assertTrue(dialogButton.open);
    assertEquals(FingerprintSetupStep.LOCATE_SCANNER, dialog.get('step_'));
    const element =
        dialog.shadowRoot!.querySelector<HTMLElement>('#scannerLocationLottie');
    assertTrue(!!element);
    assertFalse(element.hidden);
  });

  // Verify running through the enroll session workflow
  // (settings-setup-fingerprint-dialog) works as expected.
  test('EnrollingFingerprint', async () => {
    loadTimeData.overrideValues({fingerprintUnlockEnabled: true});
    openDialog();
    await browserProxy.whenCalled('startEnroll');
    const dialogButton =
        dialog.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialogButton);
    assertTrue(dialogButton.open);
    assertEquals(0, dialog.get('percentComplete_'));
    assertEquals(FingerprintSetupStep.LOCATE_SCANNER, dialog.get('step_'));
    const element =
        dialog.shadowRoot!.querySelector<HTMLElement>('#scannerLocationLottie');
    assertTrue(!!element);
    assertFalse(element.hidden);
    const arcElement = dialog.shadowRoot!.querySelector<HTMLElement>('#arc');
    assertTrue(!!arcElement);
    assertTrue(arcElement.hidden);
    // Message should be shown for LOCATE_SCANNER step.
    const message = dialog.shadowRoot!.querySelector('#messageDiv');
    assertTrue(!!message);
    assertEquals('visible', window.getComputedStyle(message).visibility);

    // First tap on the sensor to start fingerprint enrollment.
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 20 /* percent */);
    assertEquals(20, dialog.get('percentComplete_'));
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.get('step_'));
    assertTrue(element.hidden);
    assertFalse(arcElement.hidden);

    // Verify that by sending a scan problem, the div that contains the
    // problem message should be visible.
    browserProxy.scanReceived(
        FingerprintResultType.TOO_FAST, false, 20 /* percent */);
    assertEquals(20, dialog.get('percentComplete_'));
    assertEquals('visible', window.getComputedStyle(message).visibility);
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 50 /* percent */);
    assertEquals('hidden', window.getComputedStyle(message).visibility);
    assertEquals(50, dialog.get('percentComplete_'));
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 70 /* percent */);
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, true, 100 /* percent */);
    assertEquals(FingerprintSetupStep.READY, dialog.get('step_'));
    // Message should be shown for READY step.
    assertEquals('visible', window.getComputedStyle(message).visibility);

    // Verify that by tapping the continue button we should exit the dialog
    // and the fingerprint list should have one fingerprint registered.
    const closeButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#closeButton');
    assertTrue(!!closeButton);
    closeButton.click();
    await flushTasks();
    await browserProxy.whenCalled('getFingerprintsList');
    assertEquals(1, fingerprintList.get('fingerprints_').length);
  });

  // Verify enrolling a fingerprint, then enrolling another without closing the
  // dialog works as intended.
  test('EnrollingAnotherFingerprint', async () => {
    loadTimeData.overrideValues({fingerprintUnlockEnabled: true});
    openDialog();
    await browserProxy.whenCalled('startEnroll');
    browserProxy.resetResolver('startEnroll');

    const dialogButton =
        dialog.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialogButton);
    assertTrue(dialogButton.open);
    assertEquals(0, dialog.get('percentComplete_'));
    assertFalse(isVisible(addAnotherButton));
    assertEquals(FingerprintSetupStep.LOCATE_SCANNER, dialog.get('step_'));

    // First tap on the sensor to start fingerprint enrollment.
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 20 /* percent */);
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.get('step_'));

    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, true, 100 /* percent */);
    assertEquals(FingerprintSetupStep.READY, dialog.get('step_'));

    assertTrue(dialogButton.open);
    assertTrue(isVisible(addAnotherButton));
    addAnotherButton.click();

    // Once the first fingerprint is enrolled, verify that enrolling the
    // second fingerprint without closing the dialog works as expected.
    await Promise.all([
      browserProxy.whenCalled('startEnroll'),
      browserProxy.whenCalled('getFingerprintsList'),
    ]);
    browserProxy.resetResolver('getFingerprintsList');

    assertTrue(dialogButton.open);
    assertFalse(isVisible(addAnotherButton));
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.get('step_'));
    const element =
        dialog.shadowRoot!.querySelector<HTMLElement>('#scannerLocationLottie');
    assertTrue(!!element);
    assertTrue(element.hidden);
    const arcElement = dialog.shadowRoot!.querySelector<HTMLElement>('#arc');
    assertTrue(!!arcElement);
    assertFalse(arcElement.hidden);

    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, true, 100 /* percent */);

    // Verify that by tapping the continue button we should exit the
    // dialog and the fingerprint list should have two fingerprints
    // registered.
    const closeButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#closeButton');
    assertTrue(!!closeButton);
    closeButton.click();
    await browserProxy.whenCalled('getFingerprintsList');
    assertEquals(2, fingerprintList.get('fingerprints_').length);
  });

  // Verify after third fingerprint is enrolled, add another button in the
  // setup dialog is hidden.
  test('EnrollingThirdFingerprint', async () => {
    browserProxy.setFingerprints(['1', '2']);
    fingerprintList['updateFingerprintsList_']();

    openDialog();
    await browserProxy.whenCalled('startEnroll');
    browserProxy.resetResolver('startEnroll');

    const dialogButton =
        dialog.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialogButton);
    assertTrue(dialogButton.open);
    assertEquals(0, dialog.get('percentComplete_'));
    assertFalse(isVisible(addAnotherButton));
    assertEquals(FingerprintSetupStep.LOCATE_SCANNER, dialog.get('step_'));

    // First tap on the sensor to start fingerprint enrollment.
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 20 /* percent */);
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.get('step_'));

    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, true, 100 /* percent */);
    assertEquals(FingerprintSetupStep.READY, dialog.get('step_'));
    await browserProxy.whenCalled('getFingerprintsList');
    browserProxy.resetResolver('getFingerprintsList');

    // Add another is hidden after third fingerprint is enrolled.
    assertTrue(dialogButton.open);
    assertFalse(isVisible(addAnotherButton));
    assertEquals(3, fingerprintList.get('fingerprints_').length);
  });

  test('CancelEnrollingFingerprint', async () => {
    openDialog();
    await browserProxy.whenCalled('startEnroll');
    const dialogButton =
        dialog.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialogButton);
    assertTrue(dialogButton.open);
    assertEquals(0, dialog.get('percentComplete_'));
    assertEquals(FingerprintSetupStep.LOCATE_SCANNER, dialog.get('step_'));
    // First tap on the sensor to start fingerprint enrollment.
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 20 /* percent */);
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.get('step_'));

    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 30 /* percent */);
    assertEquals(30, dialog.get('percentComplete_'));
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.get('step_'));

    // Verify that by tapping the exit button we should exit the dialog
    // and the fingerprint list should have zero fingerprints registered.
    const closeButton =
        dialog.shadowRoot!.querySelector<HTMLButtonElement>('#closeButton');
    assertTrue(!!closeButton);
    closeButton.click();
    await browserProxy.whenCalled('cancelCurrentEnroll');
    assertEquals(0, fingerprintList.get('fingerprints_').length);
  });

  test('RemoveFingerprint', async () => {
    const quickUnlockPrivateApi = new FakeQuickUnlockPrivate();
    fingerprintList.set('authToken', quickUnlockPrivateApi.getFakeToken());
    browserProxy.setFingerprints(['Label 1', 'Label 2']);
    fingerprintList['updateFingerprintsList_']();
    await browserProxy.whenCalled('getFingerprintsList');
    browserProxy.resetResolver('getFingerprintsList');
    assertEquals(2, fingerprintList.get('fingerprints_').length);
    fingerprintList['onFingerprintDeleteTapped_'](createFakeEvent(0));

    await Promise.all([
      browserProxy.whenCalled('removeEnrollment'),
      browserProxy.whenCalled('getFingerprintsList'),
    ]);
    assertEquals(1, fingerprintList.get('fingerprints_').length);
  });

  test('Deep link to add fingerprint', async () => {
    const quickUnlockPrivateApi = new FakeQuickUnlockPrivate();
    fingerprintList.set('authToken', quickUnlockPrivateApi.getFakeToken());
    // This is equivalent to the settings id.
    const settingId = '1111';
    browserProxy.setFingerprints(['Label 1', 'Label 2']);
    fingerprintList['updateFingerprintsList_']();
    await browserProxy.whenCalled('getFingerprintsList');

    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.FINGERPRINT, params);

    flush();

    const deepLinkElement =
        fingerprintList.shadowRoot!.querySelector<HTMLElement>(
            '#addFingerprint');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Add button should be focused for settingId=' + settingId);
  });

  test('Deep link to remove fingerprint', async () => {
    const settingId = '1112';

    browserProxy.setFingerprints(['Label 1', 'Label 2']);
    const quickUnlockPrivateApi = new FakeQuickUnlockPrivate();
    fingerprintList.set('authToken', quickUnlockPrivateApi.getFakeToken());
    fingerprintList['updateFingerprintsList_']();
    await browserProxy.whenCalled('getFingerprintsList');

    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.FINGERPRINT, params);

    flush();

    const deepLinkElement =
        fingerprintList.root!.querySelectorAll('cr-icon-button')[0];
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Trash can button should be focused for settingId=' + settingId);
  });

  test('ChangeFingerprintLabel', async () => {
    browserProxy.setFingerprints(['Label 1']);
    fingerprintList['updateFingerprintsList_']();

    await browserProxy.whenCalled('getFingerprintsList');
    assertEquals(1, fingerprintList.get('fingerprints_').length);
    assertEquals('Label 1', fingerprintList.get('fingerprints_')[0]);

    // Verify that by sending a fingerprint input change event, the new
    // label gets changed as expected.
    fingerprintList['onFingerprintLabelChanged_'](
        createFakeEvent(0, 'New Label 1'));

    await Promise.all([
      browserProxy.whenCalled('changeEnrollmentLabel'),
      browserProxy.whenCalled('getFingerprintsList'),
    ]);
    assertEquals('New Label 1', fingerprintList.get('fingerprints_')[0]);
  });

  test('AddingNewFingerprint', async () => {
    browserProxy.setFingerprints(['1', '2', '3']);
    fingerprintList['updateFingerprintsList_']();
    const quickUnlockPrivateApi = new FakeQuickUnlockPrivate();
    fingerprintList.set('authToken', quickUnlockPrivateApi.getFakeToken());
    // Verify that new fingerprints cannot be added when there are already three
    // registered fingerprints.
    await browserProxy.whenCalled('getFingerprintsList');
    browserProxy.resetResolver('getFingerprintsList');
    assertEquals(3, fingerprintList.get('fingerprints_').length);
    const actionButton =
        fingerprintList.shadowRoot!.querySelector<HTMLButtonElement>(
            '.action-button');
    assertTrue(!!actionButton);
    assertTrue(actionButton.disabled);
    fingerprintList['onFingerprintDeleteTapped_'](createFakeEvent(0));

    await Promise.all([
      browserProxy.whenCalled('removeEnrollment'),
      browserProxy.whenCalled('getFingerprintsList'),
    ]);
    assertEquals(2, fingerprintList.get('fingerprints_').length);
    assertFalse(actionButton.disabled);
  });
});
