// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FingerprintBrowserProxyImpl, FingerprintResultType, FingerprintSetupStep, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {FingerprintBrowserProxy} */
class TestFingerprintBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getFingerprintsList',
      'getNumFingerprints',
      'startEnroll',
      'cancelCurrentEnroll',
      'getEnrollmentLabel',
      'removeEnrollment',
      'changeEnrollmentLabel',
    ]);

    /** @private {!Array<string>} */
    this.fingerprintsList_ = [];
  }

  /** @param {!Array<string>} fingerprints */
  setFingerprints(fingerprints) {
    this.fingerprintsList_ = fingerprints.slice();
  }

  /**
   * @param {FingerprintResultType} result
   * @param {boolean} complete
   * @param {number} percent
   */
  scanReceived(result, complete, percent) {
    if (complete) {
      this.fingerprintsList_.push('New Label');
    }

    webUIListenerCallback(
        'on-fingerprint-scan-received',
        {result: result, isComplete: complete, percentComplete: percent});
  }

  /** @override */
  getFingerprintsList() {
    this.methodCalled('getFingerprintsList');
    /** @type {FingerprintInfo} */
    const fingerprintInfo = {
      fingerprintsList: this.fingerprintsList_.slice(),
      isMaxed: this.fingerprintsList_.length >= 3,
    };
    return Promise.resolve(fingerprintInfo);
  }

  /** @override */
  getNumFingerprints() {
    this.methodCalled('getNumFingerprints');
    return Promise.resolve(fingerprintsList_.length);
  }

  /** @override */
  startEnroll() {
    this.methodCalled('startEnroll');
  }

  /** @override */
  cancelCurrentEnroll() {
    this.methodCalled('cancelCurrentEnroll');
  }

  /** @override */
  getEnrollmentLabel(index) {
    this.methodCalled('getEnrollmentLabel');
    return Promise.resolve(this.fingerprintsList_[index]);
  }

  /** @override */
  removeEnrollment(index) {
    this.fingerprintsList_.splice(index, 1);
    this.methodCalled('removeEnrollment', index);
    return Promise.resolve(true);
  }

  /** @override */
  changeEnrollmentLabel(index, newLabel) {
    this.fingerprintsList_[index] = newLabel;
    this.methodCalled('changeEnrollmentLabel', index, newLabel);
    return Promise.resolve(true);
  }
}

suite('settings-fingerprint-list', function() {
  /** @type {?SettingsFingerprintListElement} */
  let fingerprintList = null;

  /** @type {?SettingsSetupFingerprintDialogElement} */
  let dialog = null;
  /** @type {?HTMLButtonElement} */
  let addAnotherButton = null;
  /** @type {?settings.TestFingerprintBrowserProxy} */
  let browserProxy = null;

  /**
   * @param {number} index
   * @param {string=} opt_label
   */
  function createFakeEvent(index, opt_label) {
    return {model: {index: index, item: opt_label || ''}};
  }

  function openDialog() {
    fingerprintList.shadowRoot.querySelector('.action-button').click();
    flush();
    dialog = fingerprintList.shadowRoot.querySelector(
        'settings-setup-fingerprint-dialog');
    addAnotherButton = dialog.shadowRoot.querySelector('#addAnotherButton');
  }

  setup(async function() {
    browserProxy = new TestFingerprintBrowserProxy();
    FingerprintBrowserProxyImpl.setInstanceForTesting(browserProxy);

    PolymerTest.clearBody();
    fingerprintList = document.createElement('settings-fingerprint-list');
    document.body.appendChild(fingerprintList);
    flush();
    await browserProxy.whenCalled('getFingerprintsList');
    assertEquals(0, fingerprintList.fingerprints_.length);
    browserProxy.resetResolver('getFingerprintsList');
  });

  test('EnrollingFingerprintLottieAnimation', async function() {
    loadTimeData.overrideValues({fingerprintUnlockEnabled: true});
    openDialog();
    await browserProxy.whenCalled('startEnroll');
    assertTrue(dialog.shadowRoot.querySelector('#dialog').open);
    assertEquals(FingerprintSetupStep.LOCATE_SCANNER, dialog.step_);
    assertFalse(
        dialog.shadowRoot.querySelector('#scannerLocationLottie').hidden);
  });

  // Verify running through the enroll session workflow
  // (settings-setup-fingerprint-dialog) works as expected.
  test('EnrollingFingerprint', async function() {
    loadTimeData.overrideValues({fingerprintUnlockEnabled: true});
    openDialog();
    await browserProxy.whenCalled('startEnroll');
    assertTrue(dialog.shadowRoot.querySelector('#dialog').open);
    assertEquals(0, dialog.percentComplete_);
    assertEquals(FingerprintSetupStep.LOCATE_SCANNER, dialog.step_);
    assertFalse(
        dialog.shadowRoot.querySelector('#scannerLocationLottie').hidden);
    assertTrue(dialog.shadowRoot.querySelector('#arc').hidden);
    // Message should be shown for LOCATE_SCANNER step.
    assertEquals(
        'visible',
        window.getComputedStyle(dialog.shadowRoot.querySelector('#messageDiv'))
            .visibility);

    // First tap on the sensor to start fingerprint enrollment.
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 20 /* percent */);
    assertEquals(20, dialog.percentComplete_);
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.step_);
    assertTrue(
        dialog.shadowRoot.querySelector('#scannerLocationLottie').hidden);
    assertFalse(dialog.shadowRoot.querySelector('#arc').hidden);

    // Verify that by sending a scan problem, the div that contains the
    // problem message should be visible.
    browserProxy.scanReceived(
        FingerprintResultType.TOO_FAST, false, 20 /* percent */);
    assertEquals(20, dialog.percentComplete_);
    assertEquals(
        'visible',
        window.getComputedStyle(dialog.shadowRoot.querySelector('#messageDiv'))
            .visibility);
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 50 /* percent */);
    assertEquals(
        'hidden',
        window.getComputedStyle(dialog.shadowRoot.querySelector('#messageDiv'))
            .visibility);
    assertEquals(50, dialog.percentComplete_);
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 70 /* percent */);
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, true, 100 /* percent */);
    assertEquals(FingerprintSetupStep.READY, dialog.step_);
    // Message should be shown for READY step.
    assertEquals(
        'visible',
        window.getComputedStyle(dialog.shadowRoot.querySelector('#messageDiv'))
            .visibility);

    // Verify that by tapping the continue button we should exit the dialog
    // and the fingerprint list should have one fingerprint registered.
    dialog.shadowRoot.querySelector('#closeButton').click();
    await flushTasks();
    await browserProxy.whenCalled('getFingerprintsList');
    assertEquals(1, fingerprintList.fingerprints_.length);
  });

  // Verify enrolling a fingerprint, then enrolling another without closing the
  // dialog works as intended.
  test('EnrollingAnotherFingerprint', async function() {
    loadTimeData.overrideValues({fingerprintUnlockEnabled: true});
    openDialog();
    await browserProxy.whenCalled('startEnroll');
    browserProxy.resetResolver('startEnroll');

    assertTrue(dialog.shadowRoot.querySelector('#dialog').open);
    assertEquals(0, dialog.percentComplete_);
    assertFalse(isVisible(addAnotherButton));
    assertEquals(FingerprintSetupStep.LOCATE_SCANNER, dialog.step_);

    // First tap on the sensor to start fingerprint enrollment.
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 20 /* percent */);
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.step_);

    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, true, 100 /* percent */);
    assertEquals(FingerprintSetupStep.READY, dialog.step_);

    assertTrue(dialog.shadowRoot.querySelector('#dialog').open);
    assertTrue(isVisible(addAnotherButton));
    addAnotherButton.click();

    // Once the first fingerprint is enrolled, verify that enrolling the
    // second fingerprint without closing the dialog works as expected.
    await Promise.all([
      browserProxy.whenCalled('startEnroll'),
      browserProxy.whenCalled('getFingerprintsList'),
    ]);
    browserProxy.resetResolver('getFingerprintsList');

    assertTrue(dialog.shadowRoot.querySelector('#dialog').open);
    assertFalse(isVisible(addAnotherButton));
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.step_);
    assertTrue(
        dialog.shadowRoot.querySelector('#scannerLocationLottie').hidden);
    assertFalse(dialog.shadowRoot.querySelector('#arc').hidden);

    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, true, 100 /* percent */);

    // Verify that by tapping the continue button we should exit the
    // dialog and the fingerprint list should have two fingerprints
    // registered.
    dialog.shadowRoot.querySelector('#closeButton').click();
    await browserProxy.whenCalled('getFingerprintsList');
    assertEquals(2, fingerprintList.fingerprints_.length);
  });

  // Verify after third fingerprint is enrolled, add another button in the
  // setup dialog is hidden.
  test('EnrollingThirdFingerprint', async function() {
    browserProxy.setFingerprints(['1', '2']);
    fingerprintList.updateFingerprintsList_();

    openDialog();
    await browserProxy.whenCalled('startEnroll');
    browserProxy.resetResolver('startEnroll');

    assertTrue(dialog.shadowRoot.querySelector('#dialog').open);
    assertEquals(0, dialog.percentComplete_);
    assertFalse(isVisible(addAnotherButton));
    assertEquals(FingerprintSetupStep.LOCATE_SCANNER, dialog.step_);

    // First tap on the sensor to start fingerprint enrollment.
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 20 /* percent */);
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.step_);

    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, true, 100 /* percent */);
    assertEquals(FingerprintSetupStep.READY, dialog.step_);
    await browserProxy.whenCalled('getFingerprintsList');
    browserProxy.resetResolver('getFingerprintsList');

    // Add another is hidden after third fingerprint is enrolled.
    assertTrue(dialog.shadowRoot.querySelector('#dialog').open);
    assertFalse(isVisible(addAnotherButton));
    assertEquals(3, fingerprintList.fingerprints_.length);
  });

  test('CancelEnrollingFingerprint', async function() {
    openDialog();
    await browserProxy.whenCalled('startEnroll');
    assertTrue(dialog.shadowRoot.querySelector('#dialog').open);
    assertEquals(0, dialog.percentComplete_);
    assertEquals(FingerprintSetupStep.LOCATE_SCANNER, dialog.step_);
    // First tap on the sensor to start fingerprint enrollment.
    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 20 /* percent */);
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.step_);

    browserProxy.scanReceived(
        FingerprintResultType.SUCCESS, false, 30 /* percent */);
    assertEquals(30, dialog.percentComplete_);
    assertEquals(FingerprintSetupStep.MOVE_FINGER, dialog.step_);

    // Verify that by tapping the exit button we should exit the dialog
    // and the fingerprint list should have zero fingerprints registered.
    dialog.shadowRoot.querySelector('#closeButton').click();
    await browserProxy.whenCalled('cancelCurrentEnroll');
    assertEquals(0, fingerprintList.fingerprints_.length);
  });

  test('RemoveFingerprint', async function() {
    browserProxy.setFingerprints(['Label 1', 'Label 2']);
    fingerprintList.updateFingerprintsList_();

    await browserProxy.whenCalled('getFingerprintsList');
    browserProxy.resetResolver('getFingerprintsList');
    assertEquals(2, fingerprintList.fingerprints_.length);
    fingerprintList.onFingerprintDeleteTapped_(createFakeEvent(0));

    await Promise.all([
      browserProxy.whenCalled('removeEnrollment'),
      browserProxy.whenCalled('getFingerprintsList'),
    ]);
    assertEquals(1, fingerprintList.fingerprints_.length);
  });

  test('Deep link to add fingerprint', async () => {
    const settingId = '1111';

    browserProxy.setFingerprints(['Label 1', 'Label 2']);
    fingerprintList.updateFingerprintsList_();
    await browserProxy.whenCalled('getFingerprintsList');

    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.FINGERPRINT, params);

    flush();

    const deepLinkElement =
        fingerprintList.shadowRoot.querySelector('#addFingerprint');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Add button should be focused for settingId=' + settingId);
  });

  test('Deep link to remove fingerprint', async () => {
    const settingId = '1112';

    browserProxy.setFingerprints(['Label 1', 'Label 2']);
    fingerprintList.updateFingerprintsList_();
    await browserProxy.whenCalled('getFingerprintsList');

    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.FINGERPRINT, params);

    flush();

    const deepLinkElement =
        fingerprintList.root.querySelectorAll('cr-icon-button')[0];
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Trash can button should be focused for settingId=' + settingId);
  });

  test('ChangeFingerprintLabel', async function() {
    browserProxy.setFingerprints(['Label 1']);
    fingerprintList.updateFingerprintsList_();

    await browserProxy.whenCalled('getFingerprintsList');
    assertEquals(1, fingerprintList.fingerprints_.length);
    assertEquals('Label 1', fingerprintList.fingerprints_[0]);

    // Verify that by sending a fingerprint input change event, the new
    // label gets changed as expected.
    fingerprintList.onFingerprintLabelChanged_(
        createFakeEvent(0, 'New Label 1'));

    await Promise.all([
      browserProxy.whenCalled('changeEnrollmentLabel'),
      browserProxy.whenCalled('getFingerprintsList'),
    ]);
    assertEquals('New Label 1', fingerprintList.fingerprints_[0]);
  });

  test('AddingNewFingerprint', async function() {
    browserProxy.setFingerprints(['1', '2', '3']);
    fingerprintList.updateFingerprintsList_();

    // Verify that new fingerprints cannot be added when there are already three
    // registered fingerprints.
    await browserProxy.whenCalled('getFingerprintsList');
    browserProxy.resetResolver('getFingerprintsList');
    assertEquals(3, fingerprintList.fingerprints_.length);
    assertTrue(
        fingerprintList.shadowRoot.querySelector('.action-button').disabled);
    fingerprintList.onFingerprintDeleteTapped_(createFakeEvent(0));

    await Promise.all([
      browserProxy.whenCalled('removeEnrollment'),
      browserProxy.whenCalled('getFingerprintsList'),
    ]);
    assertEquals(2, fingerprintList.fingerprints_.length);
    assertFalse(
        fingerprintList.shadowRoot.querySelector('.action-button').disabled);
  });
});
