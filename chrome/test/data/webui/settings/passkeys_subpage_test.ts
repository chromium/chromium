// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the passkeys subpage.
 */

import type {Passkey, PasskeysBrowserProxy, SettingsPasskeysSubpageElement, SettingsSimpleConfirmationDialogElement} from 'chrome://settings/lazy_load.js';
import {PasskeysBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

suite('PasskeysSubpage', function() {
  let browserProxy: TestPasskeysBrowserProxy;
  let page: SettingsPasskeysSubpageElement;
  const testPasskeys: [Passkey] = [
    {
      credentialId: '1',
      relyingPartyId: 'rpid_x',
      userName: 'user',
      userDisplayName: 'displayName',
    },
  ];

  setup(async function() {
    browserProxy = new TestPasskeysBrowserProxy();
    PasskeysBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-passkeys-subpage');
  });

  test('NoSupport', async function() {
    browserProxy.setNextPasskeys(null);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);

    const shown = getUsernamesFromList(page);
    assertEquals(shown.length, 0, 'No passkeys shown');

    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#error') !== null,
        'Error message shown');
  });

  test('Credentials', async function() {
    const passkeys = [
      {
        credentialId: '1',
        relyingPartyId: 'rpid1',
        userName: 'user1',
        userDisplayName: 'displayName1',
      },
      {
        credentialId: '2',
        relyingPartyId: 'rpid2',
        userName: 'user2',
        userDisplayName: 'displayName2',
      },
    ];
    browserProxy.setNextPasskeys(passkeys);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);

    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#error') === null,
        'Error message not shown');

    assertDeepEquals(
        getUsernamesFromList(page), passkeys.map(cred => cred.userName));
  });

  test('Delete', async function() {
    browserProxy.setNextPasskeys(testPasskeys);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);

    assertDeepEquals(getUsernamesFromList(page), [testPasskeys[0].userName]);
    let confirmationDialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#deleteConfirmDialog');
    assertTrue(
        confirmationDialog === null, 'Confirmation dialog should not exist');

    clickDots(page, 0);

    browserProxy.whenCalled('delete').then((name: string) => {
      assertEquals(name, testPasskeys[0].credentialId);
    });
    clickButton(page, 'delete');
    await flushTasks();

    assertEquals(
        browserProxy.getCallCount('delete'), 0,
        'Delete should not have been called yet');
    confirmationDialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#deleteConfirmDialog');
    assertTrue(confirmationDialog !== null, 'Cannot find confirmation dialog');
    assertTrue(
        confirmationDialog.$.dialog.open,
        'Confirmation dialog should be showing');

    browserProxy.setNextPasskeys([]);
    confirmationDialog.$.confirm.click();
    const deletedCredentialId = await browserProxy.whenCalled('delete');
    assertEquals(deletedCredentialId, testPasskeys[0].credentialId);
    await flushTasks();

    assertDeepEquals(getUsernamesFromList(page), []);
  });

  test('DeleteCancel', async function() {
    browserProxy.setNextPasskeys(testPasskeys);
    document.body.appendChild(page);
    await flushTasks();

    clickDots(page, 0);
    clickButton(page, 'delete');
    await flushTasks();

    const confirmationDialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#deleteConfirmDialog');
    assertTrue(confirmationDialog !== null, 'Cannot find confirmation dialog');

    confirmationDialog.$.cancel.click();
    await flushTasks();

    assertEquals(
        browserProxy.getCallCount('delete'), 0,
        'Delete should not have been called');
    assertDeepEquals(getUsernamesFromList(page), [testPasskeys[0].userName]);
  });

  test('DeleteError', async function() {
    browserProxy.setNextPasskeys(testPasskeys);
    document.body.appendChild(page);
    await flushTasks();
    assertEquals(browserProxy.getCallCount('enumerate'), 1);

    const lazyDialog = page.$.deleteErrorDialog;
    assertTrue(lazyDialog !== null, 'Dialog not found');
    assertTrue(
        lazyDialog.getIfExists() === null, 'Dialog should not be showing');

    clickDots(page, 0);
    clickButton(page, 'delete');
    await flushTasks();

    const confirmationDialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            '#deleteConfirmDialog');
    assertTrue(confirmationDialog !== null, 'Cannot find confirmation dialog');

    browserProxy.setNextPasskeys(testPasskeys);
    confirmationDialog.$.confirm.click();
    const deletedCredentialId = await browserProxy.whenCalled('delete');
    assertEquals(deletedCredentialId, testPasskeys[0].credentialId);
    await flushTasks();

    assertTrue(lazyDialog.get().open, 'Error dialog should be showing');
    assertDeepEquals(getUsernamesFromList(page), [testPasskeys[0].userName]);
  });
});
