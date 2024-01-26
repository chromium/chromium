// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the passkeys subpage.
 */

import type {CrInputElement, Passkey, PasskeysBrowserProxy, SettingsPasskeysSubpageElement} from 'chrome://settings/lazy_load.js';
import {PasskeysBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestPasskeysBrowserProxy extends TestBrowserProxy implements
    PasskeysBrowserProxy {
  constructor() {
    super([
      'enumerate',
      'delete',
      'edit',
    ]);
  }

  // nextPasskeys_ is the next result to return from a call to `enumerate`
  // or `delete`.
  private nextPasskeys_: Passkey[]|null = null;

  setNextPasskeys(passkeys: Passkey[]|null) {
    this.nextPasskeys_ = passkeys;
    this.resetResolver('enumerate');
    this.resetResolver('delete');
    this.resetResolver('edit');
  }

  hasPasskeys(): Promise<boolean> {
    return Promise.resolve(true);
  }

  enumerate(): Promise<Passkey[]|null> {
    this.methodCalled('enumerate');
    return this.consumeNext_();
  }

  delete(credentialId: string): Promise<Passkey[]|null> {
    this.methodCalled('delete', credentialId);
    return this.consumeNext_();
  }
  edit(credentialId: string, newUsername: string): Promise<Passkey[]|null> {
    this.methodCalled('edit', credentialId, newUsername);
    return this.consumeNext_();
  }
  private consumeNext_(): Promise<Passkey[]|null> {
    const result = this.nextPasskeys_;
    this.nextPasskeys_ = null;
    return Promise.resolve(result);
  }
}

/**
 * Gets the usernames of the passkeys currently displayed.
 */
function getUsernamesFromList(list: HTMLElement): string[] {
  const inputs = Array.from(list.shadowRoot!.querySelectorAll<HTMLElement>(
      '.list-item .username-column'));
  return inputs.slice(1).map(input => input.textContent!.trim());
}

/**
 * Clicks the `num`th drop-down icon in the list of passkeys.
 */
function clickDots(page: HTMLElement, num: number) {
  const icon = page.shadowRoot!.querySelectorAll<HTMLElement>(
      '.list-item .icon-more-vert')[num];
  assertTrue(icon !== undefined);
  icon.click();
}

/**
 * Clicks the button named `name` in the drop-down.
 */
function clickButton(page: HTMLElement, name: string) {
  const menu = page.shadowRoot!.querySelector<HTMLElement>('#menu')!;
  const button = menu.querySelector<HTMLElement>('#' + name);

  assertTrue(button !== null, name + ' button missing');
  if (button === null) {
    return;
  }

  button.click();
}

/**
 * Clicks the buttons `save` or `cancel` in the edit dialog.
 */
function clickDialogButton(dialog: HTMLElement, name: string) {
  const menu = dialog.shadowRoot!.querySelector<HTMLElement>('#dialog')!;
  const button = menu.querySelector<HTMLElement>('#' + name);

  assertTrue(button !== null, name + ' button missing');
  if (button === null) {
    return;
  }

  button.click();
}

/**
 * Sets the username to an input string in the edit dialog.
 */
function setInputField(dialog: HTMLElement, input: string) {
  const menu = dialog.shadowRoot!.querySelector<HTMLElement>('#dialog')!;
  const inputField = menu.querySelector<HTMLInputElement>('#usernameInput');
  assertTrue(!!inputField);
  inputField!.value = input;
}

/**
 * Gets error message for username input in edit dialog.
 */
function getErrorMessage(dialog: HTMLElement) {
  const menu = dialog.shadowRoot!.querySelector<HTMLElement>('#dialog')!;
  const inputField = menu.querySelector<CrInputElement>('#usernameInput');
  assertTrue(!!inputField);
  return inputField.$.error.textContent;
}

/**
 * Returns true if error due to no passkey management support is shown
 */
function isShowingError(page: HTMLElement): boolean {
  return !!page.shadowRoot!.querySelector<HTMLElement>('#error');
}

suite('PasskeysSubpage', function() {
  let browserProxy: TestPasskeysBrowserProxy;
  let page: SettingsPasskeysSubpageElement;

  setup(async function() {
    browserProxy = new TestPasskeysBrowserProxy();
    PasskeysBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-passkeys-subpage');
  });

  test('Delete', async function() {
    const passkeys: [Passkey] = [
      {
        credentialId: '1',
        relyingPartyId: 'rpid.com',
        userName: 'user',
        userDisplayName: 'displayName',
      },
    ];
    browserProxy.setNextPasskeys(passkeys);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);


    assertDeepEquals(getUsernamesFromList(page), [passkeys[0].userName]);

    clickDots(page, 0);

    browserProxy.whenCalled('delete').then((name: string) => {
      assertEquals(name, passkeys[0].credentialId);
    });
    browserProxy.setNextPasskeys([]);
    clickButton(page, 'delete');
    await flushTasks();
    assertEquals(browserProxy.getCallCount('delete'), 1);

    assertDeepEquals(getUsernamesFromList(page), []);
  });

  test('cancelClickedEditDialog', async function() {
    const passkeys: [Passkey] = [
      {
        credentialId: '1',
        relyingPartyId: 'rpid.com',
        userName: 'user',
        userDisplayName: 'displayName',
      },
    ];
    browserProxy.setNextPasskeys(passkeys);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);

    assertFalse(isShowingError(page));
    assertDeepEquals(getUsernamesFromList(page), [passkeys[0].userName]);

    browserProxy.whenCalled('edit').then((args) => {
      assertEquals(args[0], passkeys[0].credentialId);
      assertEquals(args[1], passkeys[0].userName);
    });

    clickButton(page, 'edit');
    await flushTasks();

    const dialog = page.shadowRoot!.querySelector('passkey-edit-dialog');
    assertTrue(!!dialog);

    clickDialogButton(dialog, 'cancel');
    await flushTasks();

    assertEquals(browserProxy.getCallCount('edit'), 0);

    assertDeepEquals(
        getUsernamesFromList(page), passkeys.map(cred => cred.userName));
  });

  test('saveClickedAndUsernameValidEditDialog', async function() {
    const passkeys: [Passkey] = [
      {
        credentialId: '1',
        relyingPartyId: 'rpid.com',
        userName: 'user',
        userDisplayName: 'displayName',
      },
    ];
    const editedPasskeys: [Passkey] = [
      {
        credentialId: '1',
        relyingPartyId: 'rpid.com',
        userName: 'new-username',
        userDisplayName: 'displayName',
      },
    ];
    browserProxy.setNextPasskeys(passkeys);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);

    assertFalse(isShowingError(page));
    assertDeepEquals(getUsernamesFromList(page), [passkeys[0].userName]);

    clickDots(page, 0);

    browserProxy.whenCalled('edit').then((args) => {
      assertEquals(args[0], passkeys[0].credentialId);
      assertEquals(args[0], editedPasskeys[0].userName);
    });

    clickButton(page, 'edit');
    await flushTasks();
    const dialog = page.shadowRoot!.querySelector('passkey-edit-dialog');
    assertTrue(!!dialog);

    await flushTasks();
    browserProxy.setNextPasskeys(editedPasskeys);
    setInputField(dialog, 'new-username');
    await flushTasks();
    clickDialogButton(dialog, 'actionButton');
    await flushTasks();

    assertEquals(browserProxy.getCallCount('edit'), 1);

    assertDeepEquals(
        getUsernamesFromList(page), editedPasskeys.map(cred => cred.userName));
  });

  test('saveClickedAndUsernameInvalidEditDialog', async function() {
    const passkeys: [Passkey] = [
      {
        credentialId: '1',
        relyingPartyId: 'rpid.com',
        userName: 'user',
        userDisplayName: 'displayName',
      },
    ];
    browserProxy.setNextPasskeys(passkeys);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);

    assertFalse(isShowingError(page));
    assertDeepEquals(getUsernamesFromList(page), [passkeys[0].userName]);

    clickDots(page, 0);

    browserProxy.whenCalled('edit').then((args) => {
      assertEquals(args[0], passkeys[0].credentialId);
    });

    clickButton(page, 'edit');
    await flushTasks();

    const dialog = page.shadowRoot!.querySelector('passkey-edit-dialog');
    assertTrue(!!dialog);

    browserProxy.setNextPasskeys(passkeys);
    setInputField(dialog, '');
    clickDialogButton(dialog, 'actionButton');
    await flushTasks();

    assertEquals(getErrorMessage(dialog), 'Enter your username');

    assertEquals(browserProxy.getCallCount('edit'), 0);
    assertDeepEquals(
        getUsernamesFromList(page), passkeys.map(cred => cred.userName));
  });
});
