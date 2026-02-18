// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PasswordManagerImpl, PluralStringProxyImpl, SyncBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import type {MovePasswordsDialogElement} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {createPasswordEntry} from './test_util.js';

suite('MovePasswordsDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let syncProxy: TestSyncBrowserProxy;
  let pluralStringProxy: TestPluralStringProxy;
  let metrics: MetricsTracker;
  let dialog: MovePasswordsDialogElement;

  setup(function() {
    loadTimeData.overrideValues({'passwordUploadUiUpdate': true});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    passwordManager.setAccountStorageEnabled(true);
    PasswordManagerImpl.setInstance(passwordManager);
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);
    pluralStringProxy = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralStringProxy);
    syncProxy.accountInfo = {
      email: 'test@gmail.com',
    };
    metrics = fakeMetricsPrivate();
    return flushTasks();
  });

  async function createDialog(
      passwords: chrome.passwordsPrivate.PasswordUiEntry[],
      hasOnlyOneDeviceCredential = false) {
    passwordManager.setRequestCredentialsDetailsResponse(passwords);
    dialog = document.createElement('move-passwords-dialog');
    dialog.passwords = passwords;
    dialog.hasOnlyOneDeviceCredential = hasOnlyOneDeviceCredential;
    dialog.isAccountStoreUser = true;
    document.body.appendChild(dialog);
    await flushTasks();

    dialog.$.dialog.showModal();

    await waitAfterNextRender(dialog);
    await flushTasks();
  }

  function checkPasswordsSectionForNumberOfPasswords(
      numberOfPasswords: number) {
    assertTrue(isChildVisible(dialog, '#passwordsSection'));
    assertTrue(isChildVisible(dialog, '#passwordsTitle'));

    const passwordItems =
        dialog.shadowRoot!.querySelectorAll('password-preview-item');
    assertEquals(numberOfPasswords, passwordItems.length);
  }

  test('Content displayed properly for only password', async function() {
    const password = createPasswordEntry({id: 0, username: 'user1'});
    await createDialog([password], /*hasOnlyOneDeviceCredential=*/ true);

    assertTrue(dialog.descriptionString.includes('Your password for'));
    assertEquals(
        syncProxy.accountInfo.email, dialog.$.accountEmail.textContent.trim());

    assertFalse(isChildVisible(dialog, '#passwordsSection'));
    assertFalse(isChildVisible(dialog, '#passwordsTitle'));
  });

  test(
      'Content displayed properly for single uploadable password',
      async function() {
        const password = createPasswordEntry({id: 0, username: 'user1'});
        await createDialog([password], /*hasOnlyOneDeviceCredential=*/ false);

        // Check that the correct version of the description and title would be
        // displayed.
        const pluralStringArgs =
            await pluralStringProxy.whenCalled('getPluralString');
        assertEquals(1, pluralStringArgs.itemCount);
        assertEquals(
            syncProxy.accountInfo.email,
            dialog.$.accountEmail.textContent.trim());

        checkPasswordsSectionForNumberOfPasswords(1);
      });

  test(
      'Content displayed properly for multiple uploadable passwords',
      async function() {
        const passwords = [
          createPasswordEntry({id: 0, username: 'user1'}),
          createPasswordEntry({id: 1, username: 'user2'}),
          createPasswordEntry({id: 2, username: 'user3'}),
        ];
        await createDialog(passwords);

        // Check that the correct version of the description and title would be
        // displayed.
        const pluralStringArgs =
            await pluralStringProxy.whenCalled('getPluralString');
        assertEquals(passwords.length, pluralStringArgs.itemCount);
        assertEquals(
            syncProxy.accountInfo.email,
            dialog.$.accountEmail.textContent.trim());

        checkPasswordsSectionForNumberOfPasswords(passwords.length);
      });

  test('Accepting moves only selected passwords', async function() {
    const passwords = [
      createPasswordEntry({id: 10, username: 'u1'}),
      createPasswordEntry({id: 20, username: 'u2'}),
    ];
    await createDialog(passwords);

    // Deselect the second one.
    const items = dialog.shadowRoot!.querySelectorAll('password-preview-item');
    assertTrue(!!items);
    assertEquals(2, items.length);
    items[1]!.checked = false;
    items[1]!.dispatchEvent(new CustomEvent('change'));
    await flushTasks();

    dialog.$.acceptButton.click();

    const movedIds = await passwordManager.whenCalled('movePasswordsToAccount');
    assertEquals(1, movedIds.length);
    assertEquals(10, movedIds[0]);
  });

  test('Button is disabled when no passwords are selected', async function() {
    const passwords =
        [createPasswordEntry({id: 0}), createPasswordEntry({id: 1})];
    await createDialog(passwords);

    assertFalse(dialog.$.acceptButton.disabled);

    // Simulate unchecking all items.
    const items = dialog.shadowRoot!.querySelectorAll('password-preview-item');
    items.forEach(item => {
      item.checked = false;
      item.dispatchEvent(new CustomEvent('change'));
    });
    await flushTasks();

    assertTrue(
        dialog.$.acceptButton.disabled,
        'Button should be disabled when nothing is selected');
  });

  test('Toggle password visibility', async function() {
    const passwordEntry = createPasswordEntry(
        {id: 0, username: 'user1', password: 'password123!'});
    await createDialog([passwordEntry]);

    const item =
        dialog.shadowRoot!.querySelector<HTMLElement>('password-preview-item');
    assertTrue(!!item);

    const passwordElement =
        item.shadowRoot!.querySelector<HTMLInputElement>('#password');
    assertTrue(!!passwordElement);
    assertNotEquals(passwordEntry.password, passwordElement.value);

    const showPasswordButton =
        item.shadowRoot!.querySelector<HTMLButtonElement>(
            '#showPasswordButton');
    assertTrue(!!showPasswordButton);
    showPasswordButton.click();
    await flushTasks();

    assertEquals(passwordEntry.password, passwordElement.value);
  });

  test('Dialog closes if passwords array becomes empty', async function() {
    await createDialog([createPasswordEntry()]);
    assertTrue(dialog.$.dialog.open);

    dialog.passwords = [];
    await flushTasks();

    assertFalse(dialog.$.dialog.open);
  });

  test('Metrics recorded', async function() {
    const password = createPasswordEntry({id: 0, username: 'user1'});
    await createDialog([password], /*hasOnlyOneDeviceCredential=*/ true);

    assertEquals(
        1,
        metrics.count(
            'PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered'));

    dialog.$.acceptButton.click();

    assertEquals(
        1,
        metrics.count(
            'PasswordManager.AccountStorage.MoveToAccountStoreFlowAccepted2'));
  });
});
