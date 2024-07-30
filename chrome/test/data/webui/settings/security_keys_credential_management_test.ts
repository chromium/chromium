// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrIconButtonElement, SecurityKeysCredentialBrowserProxy, SettingsSecurityKeysCredentialManagementDialogElement} from 'chrome://settings/lazy_load.js';
import {CredentialManagementDialogPage, SecurityKeysCredentialBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertShown} from './security_keys_test_util.js';
import {TestSecurityKeysBrowserProxy} from './test_security_keys_browser_proxy.js';

const currentMinPinLength = 6;

class TestSecurityKeysCredentialBrowserProxy extends
    TestSecurityKeysBrowserProxy implements SecurityKeysCredentialBrowserProxy {
  constructor() {
    super([
      'startCredentialManagement',
      'providePin',
      'enumerateCredentials',
      'deleteCredentials',
      'updateUserInformation',
      'close',
    ]);
  }

  startCredentialManagement() {
    return this.handleMethod('startCredentialManagement');
  }

  providePin(pin: string) {
    return this.handleMethod('providePin', pin);
  }

  enumerateCredentials() {
    return this.handleMethod('enumerateCredentials');
  }

  deleteCredentials(ids: string[]) {
    return this.handleMethod('deleteCredentials', ids);
  }

  updateUserInformation(
      credentialId: string, userHandle: string, newUsername: string,
      newDisplayname: string) {
    return this.handleMethod(
        'updateUserInformation',
        {credentialId, userHandle, newUsername, newDisplayname});
  }

  close() {
    this.methodCalled('close');
  }
}

suite('SecurityKeysCredentialManagement', function() {
  let dialog: SettingsSecurityKeysCredentialManagementDialogElement;
  let allDivs: string[];
  let browserProxy: TestSecurityKeysCredentialBrowserProxy;

  setup(function() {
    browserProxy = new TestSecurityKeysCredentialBrowserProxy();
    SecurityKeysCredentialBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
    startResolver.resolve({
      minPinLength: currentMinPinLength,
      supportsUpdateUserInformation: true,
    });
    await microtasksFinished();
    assertShown(allDivs, dialog, 'pinPrompt');

    const error = 'foo bar baz';
    webUIListenerCallback(
        'security-keys-credential-management-finished', error);
    await microtasksFinished();
    assertShown(allDivs, dialog, 'pinError');
    assertTrue(dialog.$.error.textContent!.trim().includes(error));
  });

  test('PINChangeError', async function() {
    const startResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'startCredentialManagement', startResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown(allDivs, dialog, 'initial');
    startResolver.resolve({
      minPinLength: currentMinPinLength,
      supportsUpdateUserInformation: true,
    });
    await microtasksFinished();
    assertShown(allDivs, dialog, 'pinPrompt');

    const error = 'foo bar baz';
    webUIListenerCallback(
        'security-keys-credential-management-finished', error,
        true /* requiresPINChange */);
    await microtasksFinished();
    assertShown(allDivs, dialog, 'pinError');
    assertFalse(dialog.$.confirmButton.hidden);
    assertFalse(dialog.$.confirmButton.disabled);
    assertTrue(dialog.$.pinError.textContent!.trim().includes(error));

    const setPinEvent = eventToPromise('credential-management-set-pin', dialog);
    dialog.$.confirmButton.click();
    await setPinEvent;
  });

  test('UpdateNotSupported', async function() {
    const startCredentialManagementResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'startCredentialManagement', startCredentialManagementResolver.promise);
    const pinResolver = new PromiseResolver();
    browserProxy.setResponseFor('providePin', pinResolver.promise);
    const enumerateResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'enumerateCredentials', enumerateResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown(allDivs, dialog, 'initial');

    // Simulate PIN entry.
    let uiReady = eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    startCredentialManagementResolver.resolve({
      minPinLength: currentMinPinLength,
      supportsUpdateUserInformation: false,
    });

    await uiReady;
    assertShown(allDivs, dialog, 'pinPrompt');
    assertEquals(currentMinPinLength, dialog.$.pin.minPinLength);
    dialog.$.pin.$.pin.value = '000000';
    await dialog.$.pin.$.pin.updateComplete;
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePin');
    assertEquals(pin, '000000');

    // Show a credential.
    pinResolver.resolve(null);
    await browserProxy.whenCalled('enumerateCredentials');
    uiReady = eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    const credentials = [
      {
        credentialId: 'aaaaaa',
        relyingPartyId: 'acme.com',
        userHandle: 'userausera',
        userName: 'userA@example.com',
        userDisplayName: 'User Aaa',
      },
    ];
    enumerateResolver.resolve(credentials);
    await uiReady;
    assertShown(allDivs, dialog, 'credentials');
    assertEquals(dialog.$.credentialList.items, credentials);

    // Check that the edit button is disabled.
    flush();
    const editButtons: CrIconButtonElement[] =
        Array.from(dialog.$.credentialList.querySelectorAll('.edit-button'));
    assertEquals(editButtons.length, 1);
    assertTrue(editButtons[0]!.hidden);
  });

  test('Credentials', async function() {
    const startCredentialManagementResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'startCredentialManagement', startCredentialManagementResolver.promise);
    const pinResolver = new PromiseResolver();
    browserProxy.setResponseFor('providePin', pinResolver.promise);
    const enumerateResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'enumerateCredentials', enumerateResolver.promise);
    const deleteResolver = new PromiseResolver();
    browserProxy.setResponseFor('deleteCredentials', deleteResolver.promise);
    const updateUserInformationResolver = new PromiseResolver();
    browserProxy.setResponseFor(
        'updateUserInformation', updateUserInformationResolver.promise);

    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    assertShown(allDivs, dialog, 'initial');

    // Simulate PIN entry.
    let uiReady = eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    startCredentialManagementResolver.resolve({
      minPinLength: currentMinPinLength,
      supportsUpdateUserInformation: true,
    });
    await uiReady;
    assertShown(allDivs, dialog, 'pinPrompt');
    assertEquals(currentMinPinLength, dialog.$.pin.minPinLength);
    dialog.$.pin.$.pin.value = '000000';
    await dialog.$.pin.$.pin.updateComplete;
    dialog.$.confirmButton.click();
    const pin = await browserProxy.whenCalled('providePin');
    assertEquals(pin, '000000');

    // Show a list of three credentials.
    pinResolver.resolve(null);
    await browserProxy.whenCalled('enumerateCredentials');
    uiReady = eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    const credentials = [
      {
        credentialId: 'aaaaaa',
        relyingPartyId: 'acme.com',
        userHandle: 'userausera',
        userName: 'userA@example.com',
        userDisplayName: 'User Aaa',
      },
      {
        credentialId: 'bbbbbb',
        relyingPartyId: 'acme.com',
        userHandle: 'userbuserb',
        userName: 'userB@example.com',
        userDisplayName: 'User Bbb',
      },
      {
        credentialId: 'cccccc',
        relyingPartyId: 'acme.com',
        userHandle: 'usercuserc',
        userName: 'userC@example.com',
        userDisplayName: 'User Ccc',
      },
    ];
    enumerateResolver.resolve(credentials);
    await uiReady;
    assertShown(allDivs, dialog, 'credentials');
    assertEquals(dialog.$.credentialList.items, credentials);

    // Update a credential
    flush();
    const editButtons: CrIconButtonElement[] =
        Array.from(dialog.$.credentialList.querySelectorAll('.edit-button'));
    assertEquals(editButtons.length, 3);
    editButtons.forEach(button => assertFalse(button.hidden));
    editButtons[0]!.click();
    await microtasksFinished();
    assertShown(allDivs, dialog, 'edit');
    dialog.$.displayNameInput.value = 'Bobby Example';
    dialog.$.userNameInput.value = 'bobby@example.com';
    await microtasksFinished();
    dialog.$.confirmButton.click();
    credentials[0]!.userDisplayName = 'Bobby Example';
    credentials[0]!.userName = 'bobby@example.com';
    updateUserInformationResolver.resolve({success: true, message: 'updated'});
    await microtasksFinished();
    assertShown(allDivs, dialog, 'credentials');
    assertDeepEquals(dialog.$.credentialList.items, credentials);

    // Delete a credential.
    flush();
    const deleteButtons: CrIconButtonElement[] =
        Array.from(dialog.$.credentialList.querySelectorAll('.delete-button'));
    assertEquals(deleteButtons.length, 3);
    deleteButtons[0]!.click();
    await microtasksFinished();
    assertShown(allDivs, dialog, 'confirm');
    dialog.$.confirmButton.click();
    const credentialIds = await browserProxy.whenCalled('deleteCredentials');
    assertDeepEquals(credentialIds, ['aaaaaa']);
    uiReady = eventToPromise(
        'credential-management-dialog-ready-for-testing', dialog);
    deleteResolver.resolve({success: true, message: 'foobar'});
    await uiReady;
    assertShown(allDivs, dialog, 'credentials');
  });
});
