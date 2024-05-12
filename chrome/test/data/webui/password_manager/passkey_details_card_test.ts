// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {DeletePasskeyDialogElement, EditPasskeyDialogElement, PasskeyDetailsCardElement} from 'chrome://password-manager/password_manager.js';
import {Page, PasswordManagerImpl, PasswordViewPageInteractions, Router} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

suite('PasskeyDetailsCardTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let passkey: chrome.passwordsPrivate.PasswordUiEntry;
  let card: PasskeyDetailsCardElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    Router.getInstance().navigateTo(Page.PASSWORDS);
    await flushTasks();

    passkey = createPasswordEntry({
      url: 'test.com',
      username: 'reimu',
      isPasskey: true,
      displayName: 'Reimu Hakurei',
      note: 'Remember the milk',
      affiliatedDomains: [createAffiliatedDomain('test.com')],
    });

    card = document.createElement('passkey-details-card');
    card.passkey = passkey;
    document.body.appendChild(card);
    await flushTasks();
  });

  test('Content displayed properly', async function() {
    assertEquals(passkey.username, card.$.usernameValue.value);
    assertEquals(passkey.displayName, card.$.displayNameValue.value);
    assertEquals(
        // 1/12/70 is the date that matches the creation time set by
        // `createPasswordEntry`.
        card.i18n('passkeyManagementInfoLabel', '1/12/70'),
        card.$.infoLabel.innerText.trim());
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

    // Close the dialog and verify it leaves the DOM.
    const editTemplate =
        card.shadowRoot!.querySelector('#editPasskeyTemplate')!;
    const domChange = eventToPromise('dom-change', editTemplate);
    editDialog.$.dialog.close();
    await passwordManager.whenCalled('extendAuthValidity');
    await domChange;
    assertEquals(card.shadowRoot!.querySelector('edit-passkey-dialog'), null);
  });

  test('Clicking delete button opens a delete dialog', async function() {
    card.$.deleteButton.click();
    await eventToPromise('cr-dialog-open', card);
    await passwordManager.whenCalled('extendAuthValidity');
    assertEquals(
        PasswordViewPageInteractions.PASSKEY_DELETE_BUTTON_CLICKED,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));
    await flushTasks();

    const deleteDialog =
        card.shadowRoot!.querySelector<DeletePasskeyDialogElement>(
            'delete-passkey-dialog');
    assertTrue(!!deleteDialog);
    assertTrue(deleteDialog.$.dialog.open);

    // Close the dialog and verify it leaves the DOM.
    const deleteTemplate =
        card.shadowRoot!.querySelector('#deletePasskeyTemplate')!;
    const domChange = eventToPromise('dom-change', deleteTemplate);
    deleteDialog.$.dialog.close();
    await passwordManager.whenCalled('extendAuthValidity');
    await domChange;
    assertEquals(card.shadowRoot!.querySelector('delete-passkey-dialog'), null);
  });
});
