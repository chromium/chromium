// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl, SyncBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

suite('AddPasswordDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let syncProxy: TestSyncBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);
    return flushTasks();
  });

  test('content correctly displayed', async function() {
    const password =
        createPasswordEntry({id: 0, username: 'user1', password: 'sTr0nGp@@s'});
    password.affiliatedDomains = [
      createAffiliatedDomain('test.com'),
      createAffiliatedDomain('m.test.com'),
    ];
    passwordManager.setRequestCredentialsDetailsResponse([password]);

    syncProxy.accountInfo = {
      email: 'test@gmail.com',
      avatarImage: 'chrome://image-url/',
    };

    const dialog = document.createElement('move-passwords-dialog');
    dialog.passwords = [password];
    document.body.appendChild(dialog);
    await flushTasks();

    assertTrue(dialog.$.dialog.open);
    assertEquals(
        syncProxy.accountInfo.email, dialog.$.accountEmail.textContent!.trim());
    assertEquals(syncProxy.accountInfo.avatarImage, dialog.$.avatar.src);

    const passwordItems =
        dialog.shadowRoot!.querySelectorAll('password-preview-item');
    assertEquals(1, passwordItems.length);

    const passwordItem = passwordItems[0];
    assertTrue(!!passwordItem);
    // Checked by default.
    assertTrue(passwordItem.$.checkbox.checked);
    assertTrue(passwordItem.checked);
    assertEquals(password.id, passwordItem.passwordId);
    assertEquals(
        password.affiliatedDomains[0]!.name,
        passwordItem.$.website.textContent!.trim());
    assertEquals(
        password.username, passwordItem.$.username.textContent!.trim());
    // Password hidden by default.
    assertEquals('password', passwordItem.$.password.type);

    passwordItem.$.showPasswordButton.click();
    assertEquals('text', passwordItem.$.password.type);
    assertEquals(password.password, passwordItem.$.password.value);
  });

  test('single password content correctly displayed', async function() {
    const password =
        createPasswordEntry({id: 0, username: 'user1', password: 'sTr0nGp@@s'});
    password.affiliatedDomains = [
      createAffiliatedDomain('test.com'),
      createAffiliatedDomain('m.test.com'),
    ];
    passwordManager.setRequestCredentialsDetailsResponse([password]);

    syncProxy.accountInfo = {
      email: 'test@gmail.com',
      avatarImage: 'chrome://image-url/',
    };

    const dialog = document.createElement('move-single-password-dialog');
    dialog.password = password;
    document.body.appendChild(dialog);
    await flushTasks();
    assertTrue(dialog.$.dialog.open);
    assertEquals(
        syncProxy.accountInfo.email, dialog.$.accountEmail.textContent!.trim());
    assertEquals(syncProxy.accountInfo.avatarImage, dialog.$.avatar.src);
    assertEquals(
        loadTimeData.getString('moveSinglePasswordTitle'),
        dialog.$.title.textContent!.trim());
    assertEquals(
        loadTimeData.getString('moveSinglePasswordDescription'),
        dialog.$.description.textContent!.trim());
    assertEquals(
        loadTimeData.getString('moveSinglePasswordButton'),
        dialog.$.move.textContent!.trim());
  });

  test('Move passwords', async function() {
    const passwords = [
      createPasswordEntry({id: 0, username: 'user1', password: 'sTr0nGp@@s'}),
      createPasswordEntry({id: 1, username: 'user2', password: 'sTr0nGp@@s'}),
      createPasswordEntry({id: 2, username: 'user1', password: 'sTr0nGp@@s'}),
    ];
    passwords.forEach(
        item => item.affiliatedDomains = [createAffiliatedDomain('test.com')]);
    passwordManager.setRequestCredentialsDetailsResponse(passwords);
    passwordManager.data.isAccountStorageEnabled = true;

    syncProxy.accountInfo = {
      email: 'test@gmail.com',
      avatarImage: 'chrome://image-url/',
    };
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const dialog = document.createElement('move-passwords-dialog');
    dialog.passwords = passwords;
    document.body.appendChild(dialog);
    await flushTasks();

    dialog.$.move.click();

    const ids = await passwordManager.whenCalled('movePasswordsToAccount');
    assertArrayEquals([0, 1, 2], ids);
  });

  test('Move single password', async function() {
    const password = createPasswordEntry({
      id: 1234,
      username: 'user1',
      password: 'sTr0nGp@@s',
      affiliatedDomains: [createAffiliatedDomain('test.com')],
    });

    passwordManager.setRequestCredentialsDetailsResponse([password]);
    passwordManager.data.isAccountStorageEnabled = true;

    syncProxy.accountInfo = {
      email: 'test@gmail.com',
      avatarImage: 'chrome://image-url/',
    };
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const dialog = document.createElement('move-single-password-dialog');
    dialog.password = password;
    document.body.appendChild(dialog);
    await flushTasks();

    dialog.$.move.click();

    const ids = await passwordManager.whenCalled('movePasswordsToAccount');
    assertArrayEquals([1234], ids);
  });

  test('Move only selected passwords', async function() {
    const passwords = [
      createPasswordEntry({id: 0, username: 'user1', password: 'sTr0nGp@@s'}),
      createPasswordEntry({id: 1, username: 'user2', password: 'sTr0nGp@@s'}),
      createPasswordEntry({id: 2, username: 'user1', password: 'sTr0nGp@@s'}),
    ];
    passwords.forEach(
        item => item.affiliatedDomains = [createAffiliatedDomain('test.com')]);
    passwordManager.setRequestCredentialsDetailsResponse(passwords);
    passwordManager.data.isAccountStorageEnabled = true;

    syncProxy.accountInfo = {
      email: 'test@gmail.com',
      avatarImage: 'chrome://image-url/',
    };
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const dialog = document.createElement('move-passwords-dialog');
    dialog.passwords = passwords;
    document.body.appendChild(dialog);
    await flushTasks();

    const passwordItems =
        dialog.shadowRoot!.querySelectorAll('password-preview-item');
    assertEquals(3, passwordItems.length);

    // Deselect 2nd item.
    passwordItems[1]!.$.checkbox.click();
    await passwordItems[1]!.$.checkbox.updateComplete;

    dialog.$.move.click();

    const ids = await passwordManager.whenCalled('movePasswordsToAccount');
    assertArrayEquals([0, 2], ids);
  });

  test('Move dialog not shown when auth fails', async function() {
    const password = createPasswordEntry({id: 0, username: 'user1'});
    password.affiliatedDomains = [createAffiliatedDomain('test.com')];

    const dialog = document.createElement('move-passwords-dialog');
    dialog.passwords = [password];
    document.body.appendChild(dialog);
    await flushTasks();

    assertFalse(dialog.$.dialog.open);
  });

  test('Move button disabled when nothing to move', async function() {
    const passwords = [
      createPasswordEntry({id: 0, username: 'user1', password: 'sTr0nGp@@s'}),
      createPasswordEntry({id: 1, username: 'user2', password: 'sTr0nGp@@s'}),
      createPasswordEntry({id: 2, username: 'user1', password: 'sTr0nGp@@s'}),
    ];
    passwords.forEach(
        item => item.affiliatedDomains = [createAffiliatedDomain('test.com')]);
    passwordManager.setRequestCredentialsDetailsResponse(passwords);
    passwordManager.data.isAccountStorageEnabled = true;

    syncProxy.accountInfo = {
      email: 'test@gmail.com',
      avatarImage: 'chrome://image-url/',
    };
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const dialog = document.createElement('move-passwords-dialog');
    dialog.passwords = passwords;
    document.body.appendChild(dialog);
    await flushTasks();

    const passwordItems =
        dialog.shadowRoot!.querySelectorAll('password-preview-item');
    assertEquals(3, passwordItems.length);

    for (const item of passwordItems) {
      item.$.checkbox.click();
      await item.$.checkbox.updateComplete;
    }

    assertTrue(dialog.$.move.disabled);
  });
});
