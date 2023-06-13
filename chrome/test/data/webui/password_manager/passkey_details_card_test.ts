// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {EditPasskeyDialogElement, Page, PasswordManagerImpl, PasswordViewPageInteractions, Router} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

suite('PasskeyDetailsCardTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    Router.getInstance().navigateTo(Page.PASSWORDS);
    return flushTasks();
  });

  test('Content displayed properly', async function() {
    const passkey = createPasswordEntry({
      url: 'test.com',
      username: 'reimu',
      isPasskey: true,
      displayName: 'Reimu Hakurei',
      note: 'Remember the milk',
      affiliatedDomains: [createAffiliatedDomain('test.com')],
    });

    const card = document.createElement('passkey-details-card');
    card.passkey = passkey;
    document.body.appendChild(card);
    await flushTasks();

    assertEquals(passkey.username, card.$.usernameValue.value);
    assertEquals(passkey.displayName, card.$.displayNameValue.value);
    assertTrue(isVisible(card.$.noteValue));
    assertEquals(passkey.note, card.$.noteValue.note);
    assertTrue(isVisible(card.$.editButton));
    assertTrue(isVisible(card.$.deleteButton));

    const domains =
        card.shadowRoot!.querySelectorAll<HTMLAnchorElement>('a.site-link');
    assertEquals(domains.length, 1);
    assertEquals(
        passkey.affiliatedDomains![0]!.name, domains[0]!.textContent!.trim());
    assertEquals(passkey.affiliatedDomains![0]!.url, domains[0]!.href);
  });

  test('Clicking edit button opens an edit dialog', async function() {
    const passkey = createPasswordEntry({
      url: 'test.com',
      username: 'reimu',
      isPasskey: true,
      displayName: 'Reimu Hakurei',
      note: 'Remember the milk',
      affiliatedDomains: [createAffiliatedDomain('test.com')],
    });

    const card = document.createElement('passkey-details-card');
    card.passkey = passkey;
    document.body.appendChild(card);
    await flushTasks();

    card.$.editButton.click();
    await eventToPromise('cr-dialog-open', card);
    await passwordManager.whenCalled('extendAuthValidity');
    assertEquals(
        PasswordViewPageInteractions.PASSKEY_EDIT_BUTTON_CLICKED,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));
    await flushTasks();

    const editDialog = card.shadowRoot!.querySelector<EditPasskeyDialogElement>(
        'edit-passkey-dialog');
    assertTrue(!!editDialog);
    assertTrue(editDialog.$.dialog.open);
  });

});
