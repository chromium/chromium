// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {DeletePasskeyDialogElement} from 'chrome://password-manager/password_manager.js';
import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

suite('DeletePasskeyDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let passkey: chrome.passwordsPrivate.PasswordUiEntry;
  let dialog: DeletePasskeyDialogElement;

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
    dialog = document.createElement('delete-passkey-dialog');
    dialog.passkey = passkey;
    document.body.appendChild(dialog);
    await flushTasks();
  });

  test('displays a warning with links to the website', async function() {
    const link =
        dialog.shadowRoot!.querySelector<HTMLAnchorElement>('#link a')!;
    assertTrue(!!link);
    assertEquals(link.textContent!.trim(), 'test.com');
    assertEquals(link.href!.trim(), passkey.affiliatedDomains![0]!.url);
  });

  test('clicking cancel closes the dialog', async function() {
    assertTrue(dialog.$.dialog.open);
    dialog.$.cancelButton.click();
    assertFalse(dialog.$.dialog.open);
  });

  test('clicking delete deletes the passkey', async function() {
    const deleteEvent = eventToPromise('passkey-removed', dialog);
    dialog.$.deleteButton.click();

    const {id} = await passwordManager.whenCalled('removeCredential');

    await deleteEvent;
    assertEquals(id, passkey.id);
    assertFalse(dialog.$.dialog.open);
  });
});
