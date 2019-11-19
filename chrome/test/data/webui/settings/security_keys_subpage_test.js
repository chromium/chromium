// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    if (promise != undefined) {
      this.promiseMap_.delete(methodName);
      return promise;
    }

    // Return a Promise that never resolves.
    return new Promise(() => {});
  }
}

/** @implements {settings.SecurityKeysPINBrowserProxy} */
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

/** @implements {settings.SecurityKeysResetBrowserProxy} */
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

/** @implements {settings.SecurityKeysCredentialBrowserProxy} */
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

/** @implements {settings.SecurityKeysBioEnrollProxy} */
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
      allDivs.filter(id => dialog.$[id].className == 'iron-selected');
  assertEquals(allShown.length, 1);
  assertEquals(allShown[0], expectedID);
}


suite('SecurityKeysResetDialog', function() {
  let dialog = null;
  let allDivs = null;

  setup(function() {
    browserProxy = new TestSecurityKeysResetBrowserProxy();
    settings.SecurityKeysResetBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-security-keys-reset-dialog');
    allDivs = Object.values(settings.ResetDialogPage);
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
  let dialog = null;
  let allDivs = null;

  setup(function() {
    browserProxy = new TestSecurityKeysPINBrowserProxy();
    settings.SecurityKeysPINBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-security-keys-set-pin-dialog');
    allDivs = Object.values(settings.SetPINDialogPage);
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
          'startSetPIN',
          Promise.resolve([1 /* operation complete */, testCase[0]]));
      document.body.appendChild(dialog);

      await browserProxy.whenCalled('startSetPIN');
      await browserProxy.whenCalled('close');
      assertComplete();
      assertShown(allDivs, dialog, testCase[1]);
      if (testCase[1] == 'error') {
        // Unhandled error codes display the numeric code.
        assertTrue(
            dialog.$.error.textContent.trim().includes(testCase[0].toString()));
      }
    });
  }

  test('ZeroRetries', async function() {
    // Authenticators can also signal that they are locked by indicating zero
    // attempts remaining.
    browserProxy.setResponseFor(
        'startSetPIN',
        Promise.resolve([0 /* not yet complete */, 0 /* no retries */]));
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
    const ret = test_util.eventToPromise('ui-ready', dialog);
    dialog.$.pinSubmit.click();
    return ret;
  }

  function setChangePINEntries(currentPINValue, pinValue, confirmPINValue) {
    setPINEntry(dialog.$.newPIN, pinValue);
    setPINEntry(dialog.$.confirmPIN, confirmPINValue);
    setPINEntry(dialog.$.currentPIN, currentPINValue);
    const ret = test_util.eventToPromise('ui-ready', dialog);
    dialog.$.pinSubmit.click();
    return ret;
  }

  test('SetPIN', async function() {
    const startSetPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('startSetPIN', startSetPINResolver.promise);
    document.body.appendChild(dialog);
    const uiReady = test_util.eventToPromise('ui-ready', dialog);

    await browserProxy.whenCalled('startSetPIN');
    startSetPINResolver.resolve(
        [0 /* not yet complete */, null /* no current PIN */]);
    await uiReady;
    assertNotComplete();
    assertShown(allDivs, dialog, 'pinPrompt');
    assertTrue(dialog.$.currentPINEntry.hidden);

    await setNewPINEntries('123', '');
    assertTrue(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    await setNewPINEntries('123', '123');
    assertTrue(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    await setNewPINEntries('1234', '123');
    assertFalse(dialog.$.newPIN.invalid);
    assertTrue(dialog.$.confirmPIN.invalid);

    const setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    setNewPINEntries('1234', '1234');
    ({oldPIN, newPIN} = await browserProxy.whenCalled('setPIN'));
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, '');
    assertEquals(newPIN, '1234');

    setPINResolver.resolve([1 /* complete */, 0 /* success */]);
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
      const uiReady = test_util.eventToPromise('ui-ready', dialog);

      await browserProxy.whenCalled('startSetPIN');
      startSetPINResolver.resolve(
          [0 /* not yet complete */, null /* no current PIN */]);
      await uiReady;

      browserProxy.setResponseFor(
          'setPIN', Promise.resolve([1 /* complete */, testCase[0]]));
      setNewPINEntries('1234', '1234');
      await browserProxy.whenCalled('setPIN');
      await browserProxy.whenCalled('close');
      assertComplete();
      assertShown(allDivs, dialog, testCase[1]);
      if (testCase[1] == 'error') {
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
    let uiReady = test_util.eventToPromise('ui-ready', dialog);

    await browserProxy.whenCalled('startSetPIN');
    startSetPINResolver.resolve(
        [0 /* not yet complete */, 2 /* two attempts */]);
    await uiReady;
    assertNotComplete();
    assertShown(allDivs, dialog, 'pinPrompt');
    assertFalse(dialog.$.currentPINEntry.hidden);

    setChangePINEntries('123', '', '');
    assertTrue(dialog.$.currentPIN.invalid);
    assertFalse(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    setChangePINEntries('123', '123', '');
    assertTrue(dialog.$.currentPIN.invalid);
    assertFalse(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    setChangePINEntries('1234', '123', '1234');
    assertFalse(dialog.$.currentPIN.invalid);
    assertTrue(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    setChangePINEntries('123', '1234', '1234');
    assertTrue(dialog.$.currentPIN.invalid);
    assertFalse(dialog.$.newPIN.invalid);
    assertFalse(dialog.$.confirmPIN.invalid);

    let setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    setPINEntry(dialog.$.currentPIN, '4321');
    setPINEntry(dialog.$.newPIN, '1234');
    setPINEntry(dialog.$.confirmPIN, '1234');
    dialog.$.pinSubmit.click();
    let {oldPIN, newPIN} = await browserProxy.whenCalled('setPIN');
    assertShown(allDivs, dialog, 'pinPrompt');
    assertNotComplete();
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, '4321');
    assertEquals(newPIN, '1234');

    // Simulate an incorrect PIN.
    uiReady = test_util.eventToPromise('ui-ready', dialog);
    setPINResolver.resolve([1 /* complete */, 49 /* incorrect PIN */]);
    await uiReady;
    assertTrue(dialog.$.currentPIN.invalid);
    // Text box for current PIN should not be cleared.
    assertEquals(dialog.$.currentPIN.value, '4321');

    setPINEntry(dialog.$.currentPIN, '43211');

    browserProxy.resetResolver('setPIN');
    setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    dialog.$.pinSubmit.click();
    ({oldPIN, newPIN} = await browserProxy.whenCalled('setPIN'));
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, '43211');
    assertEquals(newPIN, '1234');

    setPINResolver.resolve([1 /* complete */, 0 /* success */]);
    await browserProxy.whenCalled('close');
    assertShown(allDivs, dialog, 'success');
    assertComplete();
  });
});

suite('SecurityKeysCredentialManagement', function() {
  let dialog = null;
  let allDivs = null;

  setup(function() {
    browserProxy = new TestSecurityKeysCredentialBrowserProxy();
    settings.SecurityKeysCredentialBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement(
        'settings-security-keys-credential-management-dialog');
    allDivs = Object.values(settings.CredentialManagementDialogPage);
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
    startResolver.resolve();

    const errorString = 'foo bar baz';
    cr.webUIListenerCallback(
        'security-keys-credential-management-finished', errorString);
    assertShown(allDivs, dialog, 'error');
    assertTrue(dialog.$.error.textContent.trim().includes(errorString));
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
    let uiReady = test_util.eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    startCredentialManagementResolver.resolve();
    await uiReady;
    assertShown(allDivs, dialog, 'pinPrompt');
    dialog.$.pin.value = '0000';
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePIN');
    assertEquals(pin, '0000');

    // Show a list of three credentials.
    pinResolver.resolve();
    await browserProxy.whenCalled('enumerateCredentials');
    uiReady = test_util.eventToPromise(
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
    Polymer.flush();
    assertTrue(dialog.$.confirmButton.disabled);
    const checkboxes = Array.from(
        Polymer.dom(dialog.$.credentialList).querySelectorAll('cr-checkbox'));
    assertEquals(checkboxes.length, 3);
    assertEquals(checkboxes.filter(el => el.checked).length, 0);
    checkboxes[1].click();
    checkboxes[2].click();
    assertFalse(dialog.$.confirmButton.disabled);

    dialog.$.confirmButton.click();
    const credentialIds = await browserProxy.whenCalled('deleteCredentials');
    assertDeepEquals(credentialIds, ['bbbbbb', 'cccccc']);
    uiReady = test_util.eventToPromise(
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

  setup(function() {
    browserProxy = new TestSecurityKeysBioEnrollProxy();
    settings.SecurityKeysBioEnrollProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-security-keys-bio-enroll-dialog');
    allDivs = Object.values(settings.BioEnrollDialogPage);
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
    resolver.resolve();

    const errorString = 'foo bar baz';
    cr.webUIListenerCallback('security-keys-bio-enroll-error', errorString);
    assertShown(allDivs, dialog, 'error');
    assertTrue(dialog.$.error.textContent.trim().includes(errorString));
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
    let uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    startResolver.resolve();
    await uiReady;
    assertShown(allDivs, dialog, 'pinPrompt');
    dialog.$.pin.value = '0000';
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePIN');
    assertEquals(pin, '0000');

    // Show a list of three enrollments.
    pinResolver.resolve();
    await browserProxy.whenCalled('enumerateEnrollments');
    uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    const enrollments = [
      {
        name: 'Fingerprint00',
        id: '0000',
      },
      {
        name: 'FingerprintAF',
        id: '4321',
      },
      {
        name: 'FingerprintFA',
        id: '1234',
      },
    ];
    enumerateResolver.resolve(enrollments);
    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
    assertEquals(dialog.$.enrollmentList.items, enrollments);

    // Delete the second enrollments and refresh the list.
    Polymer.flush();
    Polymer.dom(dialog.$.enrollmentList)
        .querySelectorAll('cr-icon-button')[1]
        .click();
    const id = await browserProxy.whenCalled('deleteEnrollment');
    assertEquals(enrollments[1].id, id);
    enrollments.splice(1, 1);
    deleteResolver.resolve(enrollments);
    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
    assertEquals(dialog.$.enrollmentList.items, enrollments);
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
    let uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    startResolver.resolve();
    await uiReady;
    assertShown(allDivs, dialog, 'pinPrompt');
    dialog.$.pin.value = '0000';
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePIN');
    assertEquals(pin, '0000');

    // Ensure no enrollments exist.
    pinResolver.resolve();
    await browserProxy.whenCalled('enumerateEnrollments');
    uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    enumerateResolver.resolve([]);
    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
    assertEquals(dialog.$.enrollmentList.items.length, 0);

    // Simulate add enrollment.
    assertFalse(dialog.$.addButton.hidden);
    uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    dialog.$.addButton.click();
    await browserProxy.whenCalled('startEnrolling');
    await uiReady;

    assertShown(allDivs, dialog, 'enroll');
    uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    cr.webUIListenerCallback(
        'security-keys-bio-enroll-status', {status: 0, remaining: 1});
    await uiReady;
    assertFalse(dialog.$.arc.isComplete());
    assertFalse(dialog.$.cancelButton.hidden);
    assert(dialog.$.confirmButton.hidden);

    uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    const enrollmentId = 'someId';
    const enrollmentName = 'New Fingerprint';
    enrollingResolver.resolve({
      code: 0,
      remaining: 0,
      enrollment: {
        id: enrollmentId,
        name: enrollmentName,
      },
    });
    await uiReady;
    assert(dialog.$.arc.isComplete());
    assert(dialog.$.cancelButton.hidden);
    assertFalse(dialog.$.confirmButton.hidden);

    // Proceeding brings up rename dialog page.
    uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    dialog.$.confirmButton.click();
    await uiReady;

    assertShown(allDivs, dialog, 'chooseName');
    assertEquals(dialog.$.enrollmentName.value, enrollmentName);
    const newEnrollmentName = 'Even Newer Fingerprint';
    dialog.$.enrollmentName.value = newEnrollmentName;
    assertFalse(dialog.$.confirmButton.hidden);
    assertFalse(dialog.$.confirmButton.disabled);

    // Proceeding renames the enrollment and returns to the enrollment overview.
    uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
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
    let enrollResolver = new PromiseResolver;
    browserProxy.setResponseFor('startEnrolling', enrollResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startBioEnroll');

    dialog.dialogPage_ = 'enrollments';

    // Forcibly disable the cancel button to ensure showing the dialog page
    // re-enables it.
    dialog.cancelButtonDisabled_ = true;
    dialog.cancelButtonVisible_ = false;

    let uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    dialog.$.addButton.click();
    await browserProxy.whenCalled('startEnrolling');
    await uiReady;

    assertShown(allDivs, dialog, 'enroll');
    assert(dialog.$.cancelButton.disabled == false);
    assert(dialog.$.cancelButton.hidden == false);

    uiReady =
        test_util.eventToPromise('bio-enroll-dialog-ready-for-testing', dialog);
    dialog.$.cancelButton.click();
    await browserProxy.whenCalled('cancelEnrollment');
    enrollResolver.resolve({code: Ctap2Status.ERR_KEEPALIVE_CANCEL});
    await browserProxy.whenCalled('enumerateEnrollments');

    await uiReady;
    assertShown(allDivs, dialog, 'enrollments');
  });
});
