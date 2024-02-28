// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {EditPasskeyDialogElement} from 'chrome://password-manager/password_manager.js';
import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

suite('EditPasskeyDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let passkey: chrome.passwordsPrivate.PasswordUiEntry;
  let dialog: EditPasskeyDialogElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    await flushTasks();

    passkey = createPasswordEntry({
      id: 0,
      username: 'pikari',
      displayName: 'Hikari Kohinata',
      isPasskey: true,
    });
    passkey.affiliatedDomains = [createAffiliatedDomain('test.com')];
    dialog = document.createElement('edit-passkey-dialog');
    dialog.passkey = passkey;
    document.body.appendChild(dialog);
    await flushTasks();
  });

  test('passkey displayed correctly', async function() {
    assertEquals(dialog.$.usernameInput.value, passkey.username);
    assertEquals(
        dialog.$.usernameInput.placeholder,
        loadTimeData.getString('usernamePlaceholder'));
    assertEquals(dialog.$.displayNameInput.value, passkey.displayName);
    assertEquals(
        dialog.$.displayNameInput.placeholder,
        loadTimeData.getString('displayNamePlaceholder'));

    const listItemElements =
        dialog.shadowRoot!.querySelectorAll<HTMLAnchorElement>('a.site-link');
    assertEquals(listItemElements.length, 1);
    assertEquals(listItemElements[0]!.textContent!.trim(), 'test.com');
    assertEquals(listItemElements[0]!.href, passkey.affiliatedDomains![0]!.url);
  });

  test('passkey is updated', async function() {
    dialog.$.usernameInput.value = 'teko';
    dialog.$.displayNameInput.value = 'Futaba Ooki';
    await Promise.all([
      dialog.$.usernameInput.updateComplete,
      dialog.$.displayNameInput.updateComplete,
    ]);

    assertFalse(dialog.$.saveButton.disabled);
    dialog.$.saveButton.click();

    const updatedCredential =
        await passwordManager.whenCalled('changeCredential');

    assertEquals(updatedCredential.id, passkey.id);
    assertEquals(updatedCredential.username, dialog.$.usernameInput.value);
    assertEquals(
        updatedCredential.displayName, dialog.$.displayNameInput.value);
    assertFalse(dialog.$.dialog.open);
  });
});
