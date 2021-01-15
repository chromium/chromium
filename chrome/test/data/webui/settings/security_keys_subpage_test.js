// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BioEnrollDialogPage, CredentialManagementDialogPage, Ctap2Status, ResetDialogPage, SampleStatus, SecurityKeysBioEnrollProxyImpl, SecurityKeysCredentialBrowserProxyImpl, SecurityKeysPINBrowserProxyImpl,SecurityKeysResetBrowserProxyImpl, SetPINDialogPage} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';
// clang-format on

const currentMinPinLength = 6;
const newMinPinLength = 8;

/**
 * A base class for all security key subpage test browser proxies to
 * inherit from. Provides a |promiseMap_| that proxies can be used to
 * simulation Promise resolution via |setResponseFor| and |handleMethod|.
 */
class TestSecurityKeysBrowserProxy extends TestBrowserProxy {
  constructor(methodNames) {
    super(methodNames);

    /**
     * A map from method names to a promise to return when that method is
     * called. (If no promise is installed, a never-resolved promise is
     * returned.)
     * @private {!Map<string, !Promise>}
     */
    this.promiseMap_ = new Map();
  }

  /**
   * @param {string} methodName
   * @param {!Promise} promise
   */
  setResponseFor(methodName, promise) {
    this.promiseMap_.set(methodName, promise);
  }

  /**
   * @param {string} methodName
   * @param {*} opt_arg
   * @return {!Promise}
   * @protected
   */
  handleMethod(methodName, opt_arg) {
    this.methodCalled(methodName, opt_arg);
    const promise = this.promiseMap_.get(methodName);
    if (promise !== undefined) {
      this.promiseMap_.delete(methodName);
      return promise;
    }

    // Return a Promise that never resolves.
    return new Promise(() => {});
  }
}

/** @implements {SecurityKeysPINBrowserProxy} */
class TestSecurityKeysPINBrowserProxy extends TestSecurityKeysBrowserProxy {
  constructor() {
    super([
      'startSetPIN',
      'setPIN',
      'close',
    ]);
  }

  /** @override */
  startSetPIN() {
    return this.handleMethod('startSetPIN');
  }

  /** @override */
  setPIN(oldPIN, newPIN) {
    return this.handleMethod('setPIN', {oldPIN, newPIN});
  }

  /** @override */
  close() {
    this.methodCalled('close');
  }
}

/** @implements {SecurityKeysResetBrowserProxy} */
class TestSecurityKeysResetBrowserProxy extends TestSecurityKeysBrowserProxy {
  constructor() {
    super([
      'reset',
      'completeReset',
      'close',
    ]);
  }

  /** @override */
  reset() {
    return this.handleMethod('reset');
  }

  /** @override */
  completeReset() {
    return this.handleMethod('completeReset');
  }

  /** @override */
  close() {
    this.methodCalled('close');
  }
}

/** @implements {SecurityKeysCredentialBrowserProxy} */
class TestSecurityKeysCredentialBrowserProxy extends
    TestSecurityKeysBrowserProxy {
  constructor() {
    super([
      'startCredentialManagement',
      'providePIN',
      'enumerateCredentials',
      'deleteCredentials',
      'close',
    ]);
  }

  /** @override */
  startCredentialManagement() {
    return this.handleMethod('startCredentialManagement');
  }

  /** @override */
  providePIN(pin) {
    return this.handleMethod('providePIN', pin);
  }

  /** @override */
  enumerateCredentials() {
    return this.handleMethod('enumerateCredentials');
  }

  /** @override */
  deleteCredentials(ids) {
    return this.handleMethod('deleteCredentials', ids);
  }

  /** @override */
  close() {
    this.methodCalled('close');
  }
}

/** @implements {SecurityKeysBioEnrollProxy} */
class TestSecurityKeysBioEnrollProxy extends TestSecurityKeysBrowserProxy {
  constructor() {
    super([
      'startBioEnroll',
      'providePIN',
      'enumerateEnrollments',
      'startEnrolling',
      'cancelEnrollment',
      'deleteEnrollment',
      'renameEnrollment',
      'close',
    ]);
  }

  /** @override */
  startBioEnroll() {
    return this.handleMethod('startBioEnroll');
  }

  /** @override */
  providePIN(pin) {
    return this.handleMethod('providePIN', pin);
  }

  /** @override */
  enumerateEnrollments() {
    return this.handleMethod('enumerateEnrollments');
  }

  /** @override */
  startEnrolling() {
    return this.handleMethod('startEnrolling');
  }

  /** @override */
  cancelEnrollment() {
    return this.methodCalled('cancelEnrollment');
  }

  /** @override */
  deleteEnrollment(id) {
    return this.handleMethod('deleteEnrollment', id);
  }

  /** @override */
  renameEnrollment(id, name) {
    return this.handleMethod('renameEnrollment', [id, name]);
  }

  /** @override */
  close() {
    this.methodCalled('close');
  }
}

function assertShown(allDivs, dialog, expectedID) {
  assertTrue(allDivs.includes(expectedID));

  const allShown =
      allDivs.filter(id => dialog.$[id].className === 'iron-selected');
  assertEquals(1, allShown.length);
  assertEquals(expectedID, allShown[0]);
}


suite('SecurityKeysResetDialog', function() {
  let dialog = null;
  let allDivs = null;
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestSecurityKeysResetBrowserProxy();
    SecurityKeysResetBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-security-keys-reset-dialog');
    allDivs = Object.values(ResetDialogPage);
  });

  function assertComplete() {
    assertEquals(dialog.$.button.textContent.trim(), 'OK');
    assertEquals(dialog.$.button.className, 'action-button');
  }

  function assertNotComplete() {
    assertEquals(dialog.$.button.textContent.trim(), 'Cancel');
    assertEquals(dialog.$.button.className, 'cancel-button');
  }

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('reset');
    assertShown(allDivs, dialog, 'initial');
    assertNotComplete();
  });

  test('Cancel', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('reset');
    assertShown(allDivs, dialog, 'initial');
    assertNotComplete();
    dialog.$.button.click();
    await browserProxy.whenCalled('close');
    assertFalse(dialog.$.dialog.open);
  });

  test('NotSupported', async function() {
    browserProxy.setResponseFor(
        'reset', Promise.resolve(1 /* INVALID_COMMAND */));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'noReset');
  });

  test('ImmediateUnknownError', async function() {
    const error = 1000 /* undefined error code */;
    browserProxy.setResponseFor('reset', Promise.resolve(error));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'resetFailed');
    assertTrue(
        dialog.$.resetFailed.textContent.trim().includes(error.toString()));
  });

  test('ImmediateUnknownError', async function() {
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    const promiseResolver = new PromiseResolver();
    browserProxy.setResponseFor('completeReset', promiseResolver.promise);
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    assertNotComplete();
    assertShown(allDivs, dialog, 'resetConfirm');
    promiseResolver.resolve(0 /* success */);
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'resetSuccess');
  });

  test('UnknownError', async function() {
    const error = 1000 /* undefined error code */;
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    browserProxy.setResponseFor('completeReset', Promise.resolve(error));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'resetFailed');
    assertTrue(
        dialog.$.resetFailed.textContent.trim().includes(error.toString()));
  });

  test('ResetRejected', async function() {
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    browserProxy.setResponseFor(
        'completeReset', Promise.resolve(48 /* NOT_ALLOWED */));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'resetNotAllowed');
  });
});

suite('SecurityKeysSetPINDialog', function() {
  const tooShortCurrentPIN = 'abcd';
  const validCurrentPIN = 'abcdef';
  const tooShortNewPIN = '123456';
  const validNewPIN = '12345678';
  const anotherValidNewPIN = '87654321';

  let dialog = null;
  let allDivs = null;
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestSecurityKeysPINBrowserProxy();
    SecurityKeysPINBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-security-keys-set-pin-dialog');
    allDivs = Object.values(SetPINDialogPage);
  });

  function assertComplete() {
    assertEquals(dialog.$.closeButton.textContent.trim(), 'OK');
    assertEquals(dialog.$.closeButton.className, 'action-button');
    assertEquals(dialog.$.pinSubmit.hidden, true);
  }

  function assertNotComplete() {
    assertEquals(dialog.$.closeButton.textContent.trim(), 'Cancel');
    assertEquals(dialog.$.closeButton.className, 'cancel-button');
    assertEquals(dialog.$.pinSubmit.hidden, false);
  }

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startSetPIN');
    assertShown(allDivs, dialog, 'initial');
    assertNotComplete();
  });

  // Test error codes that are returned immediately.
  for (const testCase of [
           [1 /* INVALID_COMMAND */, 'noPINSupport'],
           [52 /* temporary lock */, 'reinsert'], [50 /* locked */, 'locked'],
           [1000 /* invalid error */, 'error']]) {
    test('ImmediateError' + testCase[0].toString(), async function() {
      browserProxy.setResponseFor(
          'startSetPIN', Promise.resolve({done: true, error: testCase[0]}));
      document.body.appendChild(dialog);

      await browserProxy.whenCalled('startSetPIN');
      await browserProxy.whenCalled('close');
      assertComplete();
      assertShown(allDivs, dialog, testCase[1]);
      if (testCase[1] === 'error') {
        // Unhandled error codes display the numeric code.
        assertTrue(
            dialog.$.error.textContent.trim().includes(testCase[0].toString()));
      }
    });
  }

  test('ZeroRetries', async function() {
    // Authenticators can also signal that they are locked by indicating zero
    // attempts remaining.
    browserProxy.setResponseFor('startSetPIN', Promise.resolve({
      done: false,
      error: null,
      currentMinPinLength,
      newMinPinLength,
      retries: 0,
    }));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('startSetPIN');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'locked');
  });

  function setPINEntry(inputElement, pinValue) {
    inputElement.value = pinValue;
    // Dispatch input events to trigger validation and UI updates.
    inputElement.dispatchEvent(
        new CustomEvent('input', {bubbles: true, cancelable: true}));
  }

  function setNewPINEntry(pinValue) {
    setPINEntry(dialog.$.newPIN, pinValue);
  }

  function setNewPINEntries(pinValue, confirmPINValue) {
    setPINEntry(dialog.$.newPIN, pinValue);
    setPINEntry(dialog.$.confirmPIN, confirmPINValue);
    const ret = eventToPromise('ui-ready', dialog);
    dialog.$.pinSubmit.click();
    return ret;
  }

  function setChangePINEntries(currentPINValue, pinValue, confirmPINValue) {
    setPINEntry(dialog.$.newPIN, pinValue);
    setPINEntry(dialog.$.confirmPIN, confirmPINValue);
    setPINEntry(dialog.$.currentPIN, currentPINValue);
    const ret = eventToPromise('ui-ready', dialog);
    dialog.$.pinSubmit.click();
    return ret;
  }

  test('SetPIN', async function() {
    const startSetPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('startSetPIN', startSetPINResolver.promise);
    document.body.appendChild(dialog);
    const uiReady = eventToPromise('ui-ready', dialog);

    await browserProxy.whenCalled('startSetPIN');
    startSetPINResolver.resolve({
      done: false,
      error: null,
      currentMinPinLength,
      newMinPinLength,
      retries: null,
    });
    await uiReady;
    assertNotComplete();
    assertShown(allDivs, dialog, 'pinPrompt');
    assertTrue(dialog.$.currentPINEntry.hidden);

    await setNewPINEntries(tooShortNewPIN, '');
    assertTrue(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    await setNewPINEntries(tooShortNewPIN, tooShortNewPIN);
    assertTrue(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    await setNewPINEntries(validNewPIN, anotherValidNewPIN);
    assertFalse(dialog.$.newPIN.invalid);
    assertTrue(dialog.$.confirmPIN.invalid);

    const setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    setNewPINEntries(validNewPIN, validNewPIN);
    const {oldPIN, newPIN} = await browserProxy.whenCalled('setPIN');
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, '');
    assertEquals(newPIN, validNewPIN);

    setPINResolver.resolve({done: true, error: 0});
    await browserProxy.whenCalled('close');
    assertShown(allDivs, dialog, 'success');
    assertComplete();
  });

  // Test error codes that are only returned after attempting to set a PIN.
  for (const testCase of [
           [52 /* temporary lock */, 'reinsert'], [50 /* locked */, 'locked'],
           [1000 /* invalid error */, 'error']]) {
    test('Error' + testCase[0].toString(), async function() {
      const startSetPINResolver = new PromiseResolver();
      browserProxy.setResponseFor('startSetPIN', startSetPINResolver.promise);
      document.body.appendChild(dialog);
      const uiReady = eventToPromise('ui-ready', dialog);

      await browserProxy.whenCalled('startSetPIN');
      startSetPINResolver.resolve({
        done: false,
        error: null,
        currentMinPinLength,
        newMinPinLength,
        retries: null,
      });
      await uiReady;

      browserProxy.setResponseFor(
          'setPIN', Promise.resolve({done: true, error: testCase[0]}));
      setNewPINEntries(validNewPIN, validNewPIN);
      await browserProxy.whenCalled('setPIN');
      await browserProxy.whenCalled('close');
      assertComplete();
      assertShown(allDivs, dialog, testCase[1]);
      if (testCase[1] === 'error') {
        // Unhandled error codes display the numeric code.
        assertTrue(
            dialog.$.error.textContent.trim().includes(testCase[0].toString()));
      }
    });
  }

  test('ChangePIN', async function() {
    const startSetPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('startSetPIN', startSetPINResolver.promise);
    document.body.appendChild(dialog);
    let uiReady = eventToPromise('ui-ready', dialog);

    await browserProxy.whenCalled('startSetPIN');
    startSetPINResolver.resolve({
      done: false,
      error: null,
      currentMinPinLength,
      newMinPinLength,
      retries: 2,
    });
    await uiReady;
    assertNotComplete();
    assertShown(allDivs, dialog, 'pinPrompt');
    assertFalse(dialog.$.currentPINEntry.hidden);

    setChangePINEntries(tooShortCurrentPIN, '', '');
    assertTrue(dialog.$.currentPIN.invalid);
    assertFalse(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    setChangePINEntries(tooShortCurrentPIN, tooShortNewPIN, '');
    assertTrue(dialog.$.currentPIN.invalid);
    assertFalse(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    setChangePINEntries(validCurrentPIN, tooShortNewPIN, validNewPIN);
    assertFalse(dialog.$.currentPIN.invalid);
    assertTrue(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    setChangePINEntries(tooShortCurrentPIN, validNewPIN, validNewPIN);
    assertTrue(dialog.$.currentPIN.invalid);
    assertFalse(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    setChangePINEntries(validNewPIN, validNewPIN, validNewPIN);
    assertFalse(dialog.$.currentPIN.invalid);
    assertTrue(dialog.$.newPIN.invalid);
    assertEquals(
        dialog.$.newPIN.errorMessage,
        loadTimeData.getString('securityKeysSamePINAsCurrent'));
    assertFalse(dialog.$.confirmPIN.invalid);

    let setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    setPINEntry(dialog.$.currentPIN, validCurrentPIN);
    setPINEntry(dialog.$.newPIN, validNewPIN);
    setPINEntry(dialog.$.confirmPIN, validNewPIN);
    dialog.$.pinSubmit.click();
    let {oldPIN, newPIN} = await browserProxy.whenCalled('setPIN');
    assertShown(allDivs, dialog, 'pinPrompt');
    assertNotComplete();
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, validCurrentPIN);
    assertEquals(newPIN, validNewPIN);

    // Simulate an incorrect PIN.
    uiReady = eventToPromise('ui-ready', dialog);
    setPINResolver.resolve({done: true, error: 49});
    await uiReady;
    assertTrue(dialog.$.currentPIN.invalid);
    // Text box for current PIN should not be cleared.
    assertEquals(dialog.$.currentPIN.value, validCurrentPIN);

    setPINEntry(dialog.$.currentPIN, anotherValidNewPIN);

    browserProxy.resetResolver('setPIN');
    setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    dialog.$.pinSubmit.click();
    ({oldPIN, newPIN} = await browserProxy.whenCalled('setPIN'));
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, anotherValidNewPIN);
    assertEquals(newPIN, validNewPIN);

    setPINResolver.resolve({done: true, error: 0});
    await browserProxy.whenCalled('close');
    assertShown(allDivs, dialog, 'success');
    assertComplete();
  });
});

suite('SecurityKeysCredentialManagement', function() {
  let dialog = null;
  let allDivs = null;
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestSecurityKeysCredentialBrowserProxy();
    SecurityKeysCredentialBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement(
        'settings-security-keys-credential-management-dialog');
    allDivs = Object.values(CredentialManagementDialogPage);
  });

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown(allDivs, dialog, 'initial');
  });

  test('Cancel', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown(allDivs, dialog, 'initial');
    dialog.$.cancelButton.click();
    await browserProxy.whenCalled('close');
    assertFalse(dialog.$.dialog.open);
  });

  test('Finished', async function() {
    const startResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'startCredentialManagement', startResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown(allDivs, dialog, 'initial');
    startResolver.resolve([currentMinPinLength]);

    const error = 'foo bar baz';
    webUIListenerCallback(
        'security-keys-credential-management-finished', error);
    assertShown(allDivs, dialog, 'error');
    assertTrue(dialog.$.error.textContent.trim().includes(error));
  });

  test('PINChangeError', async function() {
    const startResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'startCredentialManagement', startResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown(allDivs, dialog, 'initial');
    startResolver.resolve([currentMinPinLength]);

    const error = 'foo bar baz';
    webUIListenerCallback(
        'security-keys-credential-management-finished', error,
        true /* requiresPINChange */);
    assertShown(allDivs, dialog, 'error');
    assertFalse(dialog.$.confirmButton.hidden);
    assertFalse(dialog.$.confirmButton.disabled);
    assertTrue(dialog.$.error.textContent.trim().includes(error));

    const setPinEvent = eventToPromise('credential-management-set-pin', dialog);
    dialog.$.confirmButton.click();
    await setPinEvent;
  });

  test('Credentials', async function() {
    const startCredentialManagementResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'startCredentialManagement', startCredentialManagementResolver.promise);
    const pinResolver = new PromiseResolver();
    browserProxy.setResponseFor('providePIN', pinResolver.promise);
    const enumerateResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'enumerateCredentials', enumerateResolver.promise);
    const deleteResolver = new PromiseResolver();
    browserProxy.setResponseFor('deleteCredentials', deleteResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown(allDivs, dialog, 'initial');

    // Simulate PIN entry.
    let uiReady = eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    startCredentialManagementResolver.resolve([currentMinPinLength]);
    await uiReady;
    assertShown(allDivs, dialog, 'pinPrompt');
    assertEquals(currentMinPinLength, dialog.$.pin.minPinLength);
    dialog.$.pin.shadowRoot.querySelector('cr-input').value = '000000';
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePIN');
    assertEquals(pin, '000000');

    // Show a list of three credentials.
    pinResolver.resolve();
    await browserProxy.whenCalled('enumerateCredentials');
    uiReady = eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    const credentials = [
      {
        id: 'aaaaaa',
        relyingPartyId: 'acme.com',
        userName: 'userA@example.com',
        userDisplayName: 'User Aaa',
      },
      {
        id: 'bbbbbb',
        relyingPartyId: 'acme.com',
        userName: 'userB@example.com',
        userDisplayName: 'User B',
      },
      {
        id: 'cccccc',
        relyingPartyId: 'acme.com',
        userName: 'userC@example.com',
        userDisplayName: 'User C',
      },
    ];
    enumerateResolver.resolve(credentials);
    await uiReady;
    assertShown(allDivs, dialog, 'credentials');
    assertEquals(dialog.$.credentialList.items, credentials);

    // Select two of the credentials and delete them.
    flush();
    assertTrue(dialog.$.confirmButton.disabled);
    const checkboxes =
        Array.from(dialog.$.credentialList.querySelectorAll('cr-checkbox'));
    assertEquals(checkboxes.length, 3);
    assertEquals(checkboxes.filter(el => el.checked).length, 0);
    checkboxes[1].click();
    checkboxes[2].click();
    assertFalse(dialog.$.confirmButton.disabled);

    dialog.$.confirmButton.click();
    const credentialIds = await browserProxy.whenCalled('deleteCredentials');
    assertDeepEquals(credentialIds, ['bbbbbb', 'cccccc']);
    uiReady = eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    deleteResolver.resolve('foobar' /* localized response message */);
    await uiReady;
    assertShown(allDivs, dialog, 'error');
    assertTrue(dialog.$.error.textContent.trim().includes('foobar'));
  });
});

suite('SecurityKeysBioEnrollment', function() {
  let dialog = null;
  let allDivs = null;
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestSecurityKeysBioEnrollProxy();
    SecurityKeysBioEnrollProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
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

    const error = 'foo bar baz';
    webUIListenerCallback('security-keys-bio-enroll-error', error);
    assertShown(allDivs, dialog, 'error');
    assertTrue(dialog.$.confirmButton.hidden);
    assertTrue(dialog.$.error.textContent.trim().includes(error));
  });

  test('PINChangeError', async function() {
    const resolver = new PromiseResolver();
    browserProxy.setResponseFor('startBioEnroll', resolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');
    assertShown(allDivs, dialog, 'initial');
    resolver.resolve([currentMinPinLength]);

    const error = 'something about setting a new PIN';
    webUIListenerCallback(
        'security-keys-bio-enroll-error', error, true /* requiresPINChange */);
    assertShown(allDivs, dialog, 'error');
    assertFalse(dialog.$.confirmButton.hidden);
    assertFalse(dialog.$.confirmButton.disabled);
    assertTrue(dialog.$.error.textContent.trim().includes(error));

    const setPinEvent = eventToPromise('bio-enroll-set-pin', dialog);
    dialog.$.confirmButton.click();
    await setPinEvent;
  });

  test('Enrollments', async function() {
    const startResolver = new PromiseResolver();
    browserProxy.setResponseFor('startBioEnroll', startResolver.promise);
    const pinResolver = new PromiseResolver();
    browserProxy.setResponseFor('providePIN', pinResolver.promise);
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
    dialog.$.pin.shadowRoot.querySelector('cr-input').value = '000000';
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePIN');
    assertEquals(pin, '000000');

    // Show a list of three enrollments.
    pinResolver.resolve();
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
    dialog.$.enrollmentList.querySelectorAll('cr-icon-button')[1].click();
    const id = await browserProxy.whenCalled('deleteEnrollment');
    assertEquals(sortedEnrollments[1].id, id);
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
    browserProxy.setResponseFor('providePIN', pinResolver.promise);
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
    dialog.$.pin.shadowRoot.querySelector('cr-input').value = '000000';
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePIN');
    assertEquals(pin, '000000');

    // Ensure no enrollments exist.
    pinResolver.resolve();
    await browserProxy.whenCalled('enumerateEnrollments');
    uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    enumerateResolver.resolve([]);
    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
    assertEquals(dialog.$.enrollmentList.items.length, 0);

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

    assertShown(allDivs, dialog, 'chooseName');
    assertEquals(dialog.$.enrollmentName.value, enrollmentName);
    const newEnrollmentName = 'Even Newer Fingerprint';
    dialog.$.enrollmentName.value = newEnrollmentName;
    assertFalse(dialog.$.confirmButton.hidden);
    assertFalse(dialog.$.confirmButton.disabled);

    // Proceeding renames the enrollment and returns to the enrollment overview.
    uiReady = eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    const renameEnrollmentResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'renameEnrollment', renameEnrollmentResolver.promise);
    dialog.$.confirmButton.click();

    const renameArgs = await browserProxy.whenCalled('renameEnrollment');
    assertDeepEquals(renameArgs, [enrollmentId, newEnrollmentName]);
    renameEnrollmentResolver.resolve([]);
    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
  });

  test('EnrollCancel', async function() {
    // Simulate starting an enrollment and then cancelling it.
    browserProxy.setResponseFor('enumerateEnrollments', Promise.resolve([]));
    const enrollResolver = new PromiseResolver;
    browserProxy.setResponseFor('startEnrolling', enrollResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');

    dialog.dialogPage_ = 'enrollments';

    // Forcibly disable the cancel button to ensure showing the dialog page
    // re-enables it.
    dialog.cancelButtonDisabled_ = true;
    dialog.cancelButtonVisible_ = false;

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
    const enrollResolver = new PromiseResolver;
    browserProxy.setResponseFor('startEnrolling', enrollResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');

    dialog.dialogPage_ = 'enrollments';

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
