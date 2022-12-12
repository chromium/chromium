// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {CrInputElement, SecurityKeysPinBrowserProxy, SecurityKeysPinBrowserProxyImpl, SetPinDialogPage, SettingsSecurityKeysSetPinDialogElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertShown} from './security_keys_test_util.js';
import {TestSecurityKeysBrowserProxy} from './test_security_keys_browser_proxy.js';

const currentMinPinLength = 6;
const newMinPinLength = 8;

class TestSecurityKeysPinBrowserProxy extends TestSecurityKeysBrowserProxy
    implements SecurityKeysPinBrowserProxy {
  constructor() {
    super([
      'startSetPin',
      'setPin',
      'close',
    ]);
  }

  startSetPin() {
    return this.handleMethod('startSetPin');
  }

  setPin(oldPIN: string, newPIN: string) {
    return this.handleMethod('setPin', {oldPIN, newPIN});
  }

  close() {
    this.methodCalled('close');
  }
}

suite('SecurityKeysSetPINDialog', function() {
  const tooShortCurrentPIN = 'abcd';
  const validCurrentPIN = 'abcdef';
  const tooShortNewPIN = '123456';
  const validNewPIN = '12345678';
  const anotherValidNewPIN = '87654321';

  let dialog: SettingsSecurityKeysSetPinDialogElement;
  let allDivs: string[];
  let browserProxy: TestSecurityKeysPinBrowserProxy;

  setup(function() {
    browserProxy = new TestSecurityKeysPinBrowserProxy();
    SecurityKeysPinBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-security-keys-set-pin-dialog');
    allDivs = Object.values(SetPinDialogPage);
  });

  function assertComplete() {
    assertEquals(dialog.$.closeButton.textContent!.trim(), 'OK');
    assertEquals(dialog.$.closeButton.className, 'action-button');
    assertEquals(dialog.$.pinSubmit.hidden, true);
  }

  function assertNotComplete() {
    assertEquals(dialog.$.closeButton.textContent!.trim(), 'Cancel');
    assertEquals(dialog.$.closeButton.className, 'cancel-button');
    assertEquals(dialog.$.pinSubmit.hidden, false);
  }

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startSetPin');
    assertShown(allDivs, dialog, 'initial');
    assertNotComplete();
  });

  // Test error codes that are returned immediately.
  for (const testCase of [
           [1 /* INVALID_COMMAND */, 'noPINSupport'],
           [52 /* temporary lock */, 'reinsert'], [50 /* locked */, 'locked'],
           [1000 /* invalid error */, 'error']]) {
    test('ImmediateError' + testCase[0]!.toString(), async function() {
      browserProxy.setResponseFor(
          'startSetPin', Promise.resolve({done: true, error: testCase[0]}));
      document.body.appendChild(dialog);

      await browserProxy.whenCalled('startSetPin');
      await browserProxy.whenCalled('close');
      assertComplete();
      assertShown(allDivs, dialog, (testCase[1] as string));
      if (testCase[1] === 'error') {
        // Unhandled error codes display the numeric code.
        assertTrue(dialog.$.error.textContent!.trim().includes(
            testCase[0]!.toString()));
      }
    });
  }

  test('ZeroRetries', async function() {
    // Authenticators can also signal that they are locked by indicating zero
    // attempts remaining.
    browserProxy.setResponseFor('startSetPin', Promise.resolve({
      done: false,
      error: null,
      currentMinPinLength,
      newMinPinLength,
      retries: 0,
    }));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('startSetPin');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown(allDivs, dialog, 'locked');
  });

  function setPINEntry(inputElement: CrInputElement, pinValue: string) {
    inputElement.value = pinValue;
    // Dispatch input events to trigger validation and UI updates.
    inputElement.dispatchEvent(
        new CustomEvent('input', {bubbles: true, cancelable: true}));
  }

  function setNewPINEntries(
      pinValue: string, confirmPINValue: string): Promise<void> {
    setPINEntry(dialog.$.newPIN, pinValue);
    setPINEntry(dialog.$.confirmPIN, confirmPINValue);
    const ret = eventToPromise('ui-ready', dialog);
    dialog.$.pinSubmit.click();
    return ret;
  }

  function setChangePINEntries(
      currentPINValue: string, pinValue: string,
      confirmPINValue: string): Promise<void> {
    setPINEntry(dialog.$.newPIN, pinValue);
    setPINEntry(dialog.$.confirmPIN, confirmPINValue);
    setPINEntry(dialog.$.currentPIN, currentPINValue);
    const ret = eventToPromise('ui-ready', dialog);
    dialog.$.pinSubmit.click();
    return ret;
  }

  test('SetPIN', async function() {
    const startSetPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('startSetPin', startSetPINResolver.promise);
    document.body.appendChild(dialog);
    const uiReady = eventToPromise('ui-ready', dialog);

    await browserProxy.whenCalled('startSetPin');
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
    browserProxy.setResponseFor('setPin', setPINResolver.promise);
    setNewPINEntries(validNewPIN, validNewPIN);
    const {oldPIN, newPIN} = await browserProxy.whenCalled('setPin');
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
    test('Error' + testCase[0]!.toString(), async function() {
      const startSetPINResolver = new PromiseResolver();
      browserProxy.setResponseFor('startSetPin', startSetPINResolver.promise);
      document.body.appendChild(dialog);
      const uiReady = eventToPromise('ui-ready', dialog);

      await browserProxy.whenCalled('startSetPin');
      startSetPINResolver.resolve({
        done: false,
        error: null,
        currentMinPinLength,
        newMinPinLength,
        retries: null,
      });
      await uiReady;

      browserProxy.setResponseFor(
          'setPin', Promise.resolve({done: true, error: testCase[0]}));
      setNewPINEntries(validNewPIN, validNewPIN);
      await browserProxy.whenCalled('setPin');
      await browserProxy.whenCalled('close');
      assertComplete();
      assertShown(allDivs, dialog, (testCase[1] as string));
      if (testCase[1] === 'error') {
        // Unhandled error codes display the numeric code.
        assertTrue(dialog.$.error.textContent!.trim().includes(
            testCase[0]!.toString()));
      }
    });
  }

  test('ChangePIN', async function() {
    const startSetPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('startSetPin', startSetPINResolver.promise);
    document.body.appendChild(dialog);
    let uiReady = eventToPromise('ui-ready', dialog);

    await browserProxy.whenCalled('startSetPin');
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
    browserProxy.setResponseFor('setPin', setPINResolver.promise);
    setPINEntry(dialog.$.currentPIN, validCurrentPIN);
    setPINEntry(dialog.$.newPIN, validNewPIN);
    setPINEntry(dialog.$.confirmPIN, validNewPIN);
    dialog.$.pinSubmit.click();
    let {oldPIN, newPIN} = await browserProxy.whenCalled('setPin');
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

    browserProxy.resetResolver('setPin');
    setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPin', setPINResolver.promise);
    dialog.$.pinSubmit.click();
    ({oldPIN, newPIN} = await browserProxy.whenCalled('setPin'));
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, anotherValidNewPIN);
    assertEquals(newPIN, validNewPIN);

    setPINResolver.resolve({done: true, error: 0});
    await browserProxy.whenCalled('close');
    assertShown(allDivs, dialog, 'success');
    assertComplete();
  });
});
