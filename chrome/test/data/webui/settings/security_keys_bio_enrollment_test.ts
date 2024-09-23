// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SecurityKeysBioEnrollProxy, SettingsSecurityKeysBioEnrollDialogElement} from 'chrome://settings/lazy_load.js';
import {BioEnrollDialogPage, Ctap2Status, SampleStatus, SecurityKeysBioEnrollProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertShown} from './security_keys_test_util.js';
import {TestSecurityKeysBrowserProxy} from './test_security_keys_browser_proxy.js';

const currentMinPinLength = 6;

class TestSecurityKeysBioEnrollProxy extends TestSecurityKeysBrowserProxy
    implements SecurityKeysBioEnrollProxy {
  constructor() {
    super([
      'startBioEnroll',
      'providePin',
      'getSensorInfo',
      'enumerateEnrollments',
      'startEnrolling',
      'cancelEnrollment',
      'deleteEnrollment',
      'renameEnrollment',
      'close',
    ]);
  }

  startBioEnroll() {
    return this.handleMethod('startBioEnroll');
  }

  providePin(pin: string) {
    return this.handleMethod('providePin', pin);
  }

  getSensorInfo() {
    return this.handleMethod('getSensorInfo');
  }

  enumerateEnrollments() {
    return this.handleMethod('enumerateEnrollments');
  }

  startEnrolling() {
    return this.handleMethod('startEnrolling');
  }

  cancelEnrollment() {
    this.methodCalled('cancelEnrollment');
  }

  deleteEnrollment(id: string) {
    return this.handleMethod('deleteEnrollment', id);
  }

  renameEnrollment(id: string, name: string) {
    return this.handleMethod('renameEnrollment', [id, name]);
  }

  close() {
    this.methodCalled('close');
  }
}

suite('SecurityKeysBioEnrollment', function() {
  let dialog: SettingsSecurityKeysBioEnrollDialogElement;
  let allDivs: string[];
  let browserProxy: TestSecurityKeysBioEnrollProxy;

  setup(function() {
    browserProxy = new TestSecurityKeysBioEnrollProxy();
    SecurityKeysBioEnrollProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-security-keys-bio-enroll-dialog');
    allDivs = Object.values(BioEnrollDialogPage);
  });

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');
    assertShown(allDivs, dialog, 'initial');
    assertFalse(dialog.$.cancelButton.hidden);
  });

  test('Cancel', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');
    assertShown(allDivs, dialog, 'initial');
    dialog.$.cancelButton.click();
    await browserProxy.whenCalled('close');
    assertFalse(dialog.$.dialog.open);
  });

  test('Finished', async function() {
    const resolver = new PromiseResolver();
    browserProxy.setResponseFor('startBioEnroll', resolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');
    assertShown(allDivs, dialog, 'initial');
    resolver.resolve([currentMinPinLength]);
    await microtasksFinished();
    assertShown(allDivs, dialog, 'pinPrompt');

    const error = 'foo bar baz';
    webUIListenerCallback('security-keys-bio-enroll-error', error);
    await microtasksFinished();
    assertShown(allDivs, dialog, 'error');
    assertTrue(dialog.$.confirmButton.hidden);
    assertTrue(dialog.$.error.textContent!.trim().includes(error));
  });

  test('PINChangeError', async function() {
    const resolver = new PromiseResolver();
    browserProxy.setResponseFor('startBioEnroll', resolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');
    assertShown(allDivs, dialog, 'initial');
    resolver.resolve([currentMinPinLength]);
    await microtasksFinished();
    assertShown(allDivs, dialog, 'pinPrompt');

    const error = 'something about setting a new PIN';
    webUIListenerCallback(
        'security-keys-bio-enroll-error', error, true /* requiresPINChange */);
    await microtasksFinished();
    assertShown(allDivs, dialog, 'error');
    assertFalse(dialog.$.confirmButton.hidden);
    assertFalse(dialog.$.confirmButton.disabled);
    assertTrue(dialog.$.error.textContent!.trim().includes(error));

    const setPinEvent = eventToPromise('bio-enroll-set-pin', dialog);
    dialog.$.confirmButton.click();
    await setPinEvent;
  });

  test('Enrollments', async function() {
    const startResolver = new PromiseResolver();
    browserProxy.setResponseFor('startBioEnroll', startResolver.promise);
    const pinResolver = new PromiseResolver();
    browserProxy.setResponseFor('providePin', pinResolver.promise);
    const getSensorInfoResolver = new PromiseResolver();
    browserProxy.setResponseFor('getSensorInfo', getSensorInfoResolver.promise);
    const enumerateResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'enumerateEnrollments', enumerateResolver.promise);
    const deleteResolver = new PromiseResolver();
    browserProxy.setResponseFor('deleteEnrollment', deleteResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');
    assertShown(allDivs, dialog, 'initial');

    // Simulate PIN entry.
    let uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    startResolver.resolve([currentMinPinLength]);
    await uiReady;
    assertShown(allDivs, dialog, 'pinPrompt');
    assertEquals(currentMinPinLength, dialog.$.pin.minPinLength);
    dialog.$.pin.$.pin.value = '000000';
    await dialog.$.pin.$.pin.updateComplete;
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePin');
    assertEquals(pin, '000000');
    pinResolver.resolve(null);

    await browserProxy.whenCalled('getSensorInfo');
    getSensorInfoResolver.resolve({
      maxTemplateFriendlyName: 10,
    });

    // Show a list of three enrollments.
    await browserProxy.whenCalled('enumerateEnrollments');
    uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);

    const fingerprintA = {
      name: 'FingerprintA',
      id: '1234',
    };
    const fingerprintB = {
      name: 'FingerprintB',
      id: '4321',
    };
    const fingerprintC = {
      name: 'FingerprintC',
      id: '000000',
    };
    const enrollments = [fingerprintC, fingerprintB, fingerprintA];
    const sortedEnrollments = [fingerprintA, fingerprintB, fingerprintC];
    enumerateResolver.resolve(enrollments);
    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
    assertDeepEquals(dialog.$.enrollmentList.items, sortedEnrollments);

    // Delete the second enrollments and refresh the list.
    flush();
    dialog.$.enrollmentList.querySelectorAll('cr-icon-button')[1]!.click();
    const id = await browserProxy.whenCalled('deleteEnrollment');
    assertEquals(sortedEnrollments[1]!.id, id);
    sortedEnrollments.splice(1, 1);
    enrollments.splice(1, 1);
    deleteResolver.resolve(enrollments);
    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
    assertDeepEquals(dialog.$.enrollmentList.items, sortedEnrollments);
  });

  test('AddEnrollment', async function() {
    const startResolver = new PromiseResolver();
    browserProxy.setResponseFor('startBioEnroll', startResolver.promise);
    const pinResolver = new PromiseResolver();
    browserProxy.setResponseFor('providePin', pinResolver.promise);
    const getSensorInfoResolver = new PromiseResolver();
    browserProxy.setResponseFor('getSensorInfo', getSensorInfoResolver.promise);
    const enumerateResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'enumerateEnrollments', enumerateResolver.promise);
    const enrollingResolver = new PromiseResolver();
    browserProxy.setResponseFor('startEnrolling', enrollingResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');
    assertShown(allDivs, dialog, 'initial');

    // Simulate PIN entry.
    let uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    startResolver.resolve([currentMinPinLength]);
    await uiReady;
    assertShown(allDivs, dialog, 'pinPrompt');
    assertEquals(currentMinPinLength, dialog.$.pin.minPinLength);
    dialog.$.pin.$.pin.value = '000000';
    await dialog.$.pin.$.pin.updateComplete;
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePin');
    assertEquals(pin, '000000');
    pinResolver.resolve(null);

    await browserProxy.whenCalled('getSensorInfo');
    getSensorInfoResolver.resolve({
      maxTemplateFriendlyName: 20,
    });

    // Ensure no enrollments exist.
    await browserProxy.whenCalled('enumerateEnrollments');
    uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    enumerateResolver.resolve([]);
    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
    assertEquals(dialog.$.enrollmentList.items!.length, 0);

    // Simulate add enrollment.
    assertFalse(dialog.$.addButton.hidden);
    uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    dialog.$.addButton.click();
    await browserProxy.whenCalled('startEnrolling');
    await uiReady;

    assertShown(allDivs, dialog, 'enroll');
    webUIListenerCallback(
        'security-keys-bio-enroll-status',
        {status: SampleStatus.OK, remaining: 1});
    flush();
    assertFalse(dialog.$.arc.isComplete());
    assertFalse(dialog.$.cancelButton.hidden);
    assertTrue(dialog.$.confirmButton.hidden);

    uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    const enrollmentId = 'someId';
    const enrollmentName = 'New Fingerprint';
    enrollingResolver.resolve({
      code: 0,
      enrollment: {
        id: enrollmentId,
        name: enrollmentName,
      },
    });
    await uiReady;
    assertTrue(dialog.$.arc.isComplete());
    assertTrue(dialog.$.cancelButton.hidden);
    assertFalse(dialog.$.confirmButton.hidden);

    // Proceeding brings up rename dialog page.
    uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    dialog.$.confirmButton.click();
    await uiReady;

    // Try renaming with a name that's longer than |maxTemplateFriendlyName|.
    assertShown(allDivs, dialog, 'chooseName');
    assertEquals(dialog.$.enrollmentName.value, enrollmentName);
    const invalidNewEnrollmentName = '21 bytes long string!';
    dialog.$.enrollmentName.value = invalidNewEnrollmentName;
    await dialog.$.enrollmentName.updateComplete;
    assertFalse(dialog.$.confirmButton.hidden);
    assertFalse(dialog.$.confirmButton.disabled);
    assertFalse(dialog.$.enrollmentName.invalid);
    dialog.$.confirmButton.click();
    assertTrue(dialog.$.enrollmentName.invalid);
    assertEquals(browserProxy.getCallCount('renameEnrollment'), 0);

    // Try renaming to a valid name.
    assertShown(allDivs, dialog, 'chooseName');
    const newEnrollmentName = '20 bytes long string';
    dialog.$.enrollmentName.value = newEnrollmentName;
    await dialog.$.enrollmentName.updateComplete;
    assertFalse(dialog.$.confirmButton.hidden);
    assertFalse(dialog.$.confirmButton.disabled);

    // Proceeding renames the enrollment and returns to the enrollment overview.
    uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    const renameEnrollmentResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'renameEnrollment', renameEnrollmentResolver.promise);
    dialog.$.confirmButton.click();
    assertFalse(dialog.$.enrollmentName.invalid);

    const renameArgs = await browserProxy.whenCalled('renameEnrollment');
    assertDeepEquals(renameArgs, [enrollmentId, newEnrollmentName]);
    renameEnrollmentResolver.resolve([]);
    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
  });

  test('EnrollCancel', async function() {
    // Simulate starting an enrollment and then cancelling it.
    browserProxy.setResponseFor('enumerateEnrollments', Promise.resolve([]));
    const enrollResolver = new PromiseResolver();
    browserProxy.setResponseFor('startEnrolling', enrollResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');

    dialog.setDialogPageForTesting(BioEnrollDialogPage.ENROLLMENTS);

    // Forcibly disable the cancel button to ensure showing the dialog page
    // re-enables it.
    dialog.setCancelButtonDisabledForTesting(true);

    let uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    dialog.$.addButton.click();
    await browserProxy.whenCalled('startEnrolling');
    await uiReady;

    assertShown(allDivs, dialog, 'enroll');
    assertFalse(dialog.$.cancelButton.disabled);
    assertFalse(dialog.$.cancelButton.hidden);

    uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    dialog.$.cancelButton.click();
    await browserProxy.whenCalled('cancelEnrollment');
    enrollResolver.resolve({code: Ctap2Status.ERR_KEEPALIVE_CANCEL});
    await browserProxy.whenCalled('enumerateEnrollments');

    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
  });

  test('EnrollError', async function() {
    // Test that resolving the startEnrolling promise with a CTAP error brings
    // up the error page.
    const enrollResolver = new PromiseResolver();
    browserProxy.setResponseFor('startEnrolling', enrollResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');

    dialog.setDialogPageForTesting(BioEnrollDialogPage.ENROLLMENTS);

    let uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    dialog.$.addButton.click();
    await browserProxy.whenCalled('startEnrolling');
    await uiReady;

    uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);

    enrollResolver.resolve({code: Ctap2Status.ERR_INVALID_OPTION});

    await uiReady;
    assertShown(allDivs, dialog, 'error');
  });
});
