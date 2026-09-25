// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {Credential, CredentialManagementResponse, CrIconButtonElement, SecurityKeysCredentialBrowserProxy, SettingsSecurityKeysCredentialManagementDialogElement, StartCredentialManagementResponse} from 'chrome://settings/lazy_load.js';
import {CredentialManagementDialogPage, SecurityKeysCredentialBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSecurityKeysBrowserProxy} from '../test_security_keys_browser_proxy.js';

import {assertShown} from './security_keys_test_util.js';

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

  startCredentialManagement(): Promise<StartCredentialManagementResponse> {
    return this.handleMethod<StartCredentialManagementResponse>(
        'startCredentialManagement');
  }

  providePin(pin: string): Promise<number|null> {
    return this.handleMethod<number|null>('providePin', pin);
  }

  enumerateCredentials(): Promise<Credential[]> {
    return this.handleMethod<Credential[]>('enumerateCredentials');
  }

  deleteCredentials(ids: string[]): Promise<CredentialManagementResponse> {
    return this.handleMethod<CredentialManagementResponse>(
        'deleteCredentials', ids);
  }

  updateUserInformation(
      credentialId: string, userHandle: string, newUsername: string,
      newDisplayname: string): Promise<CredentialManagementResponse> {
    return this.handleMethod<CredentialManagementResponse>(
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

  async function showCredentials(credentials: Credential[]) {
    browserProxy.setResponseFor('startCredentialManagement', Promise.resolve({
      minPinLength: currentMinPinLength,
      supportsUpdateUserInformation: true,
    }));
    browserProxy.setResponseFor('providePin', Promise.resolve(null));
    browserProxy.setResponseFor(
        'enumerateCredentials', Promise.resolve(credentials));
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startCredentialManagement');
    await microtasksFinished();
    dialog.$.pin.$.pin.value = '000000';
    await dialog.$.pin.$.pin.updateComplete;
    dialog.$.confirmButton.click();
    await browserProxy.whenCalled('enumerateCredentials');
    await microtasksFinished();
    flush();
    assertShown(allDivs, dialog, 'credentials');
  }

  function assertEntries(expected: Credential[]) {
    const entries = dialog.$.container.querySelectorAll('.list-item');
    assertEquals(expected.length, entries.length);
    for (let i = 0; i < expected.length; i++) {
      assertEquals(
          expected[i]!.relyingPartyId,
          entries[i]!.querySelector('.site')!.textContent.trim());
      assertEquals(
          expected[i]!.userDisplayName,
          entries[i]!.querySelector('.user-display-name')!.textContent.trim());
      assertEquals(
          expected[i]!.userName,
          entries[i]!.querySelector('.user-name')!.textContent.trim());
    }
  }

  function assertCredentialsVisible(visible: boolean) {
    assertEquals(
        visible, isVisible(dialog.shadowRoot!.querySelector('#header')));
    assertEquals(visible, isVisible(dialog.$.container));
    assertEquals(
        !visible,
        isVisible(dialog.shadowRoot!.querySelector('#noCredentials')));
  }

  const credential: Credential = {
    credentialId: 'aaaaaa',
    relyingPartyId: 'acme.com',
    userHandle: 'userausera',
    userName: 'userA@example.com',
    userDisplayName: 'User Aaa',
  };

  test('EmptyCredentials', async function() {
    await showCredentials([]);

    assertCredentialsVisible(false);
    const emptyMessage = dialog.shadowRoot!.querySelector('#noCredentials');
    assertEquals(
        dialog.i18n('securityKeysCredentialManagementNoCredentials'),
        emptyMessage!.textContent.trim());
    assertTrue(isVisible(dialog.$.confirmButton));
    assertFalse(dialog.$.confirmButton.disabled);
    dialog.$.confirmButton.click();
    await browserProxy.whenCalled('close');
    assertFalse(dialog.$.dialog.open);
  });

  test('DeleteLastCredential', async function() {
    await showCredentials([
      {...credential},
      {...credential, credentialId: 'bbbbbb'},
    ]);

    for (const credentialId of ['aaaaaa', 'bbbbbb']) {
      assertCredentialsVisible(true);
      const deleteButton =
          dialog.$.container.querySelector<CrIconButtonElement>(
              `.delete-button[data-credentialid="${credentialId}"]`)!;
      deleteButton.click();
      await microtasksFinished();
      assertShown(allDivs, dialog, 'confirm');
      browserProxy.setResponseFor(
          'deleteCredentials', Promise.resolve({success: true, message: ''}));
      dialog.$.confirmButton.click();
      assertDeepEquals(
          [credentialId], await browserProxy.whenCalled('deleteCredentials'));
      browserProxy.resetResolver('deleteCredentials');
      await microtasksFinished();
      flush();
      assertShown(allDivs, dialog, 'credentials');
    }

    assertEntries([]);
    assertCredentialsVisible(false);
    dialog.$.confirmButton.click();
    await browserProxy.whenCalled('close');
    assertFalse(dialog.$.dialog.open);
  });

  test('DeleteLastCredentialFails', async function() {
    await showCredentials([{...credential}]);
    dialog.$.container.querySelector<CrIconButtonElement>(
                          '.delete-button')!.click();
    await microtasksFinished();
    browserProxy.setResponseFor(
        'deleteCredentials',
        Promise.resolve({success: false, message: 'Could not delete'}));
    dialog.$.confirmButton.click();
    await browserProxy.whenCalled('deleteCredentials');
    await microtasksFinished();
    assertShown(allDivs, dialog, 'error');
    assertEquals('Could not delete', dialog.$.error.textContent.trim());

    dialog.$.confirmButton.click();
    await microtasksFinished();
    assertShown(allDivs, dialog, 'credentials');
    assertEntries([{...credential}]);
    assertCredentialsVisible(true);
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
    assertTrue(dialog.$.error.textContent.trim().includes(error));
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
    assertTrue(dialog.$.pinError.textContent.trim().includes(error));

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
    assertEntries(credentials);

    // Check that the edit button is disabled.
    flush();
    const editButtons: CrIconButtonElement[] =
        Array.from(dialog.$.container.querySelectorAll('.edit-button'));
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
    assertEntries(credentials);

    // Update a credential
    flush();
    const editButtons: CrIconButtonElement[] =
        Array.from(dialog.$.container.querySelectorAll('.edit-button'));
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
    assertEntries(credentials);

    // Delete a credential.
    flush();
    const deleteButtons: CrIconButtonElement[] =
        Array.from(dialog.$.container.querySelectorAll('.delete-button'));
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
