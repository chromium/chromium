// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {SettingsUsersAddUserDialogElement} from 'chrome://os-settings/chromeos/lazy_load.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeUsersPrivate} from '../fake_users_private.js';

suite('<settings-users-add-user-dialog>', () => {
  let dialog: SettingsUsersAddUserDialogElement|null = null;

  setup(() => {
    dialog = document.createElement('settings-users-add-user-dialog');
    // @ts-ignore:next-line - Overriding private member for test
    dialog.usersPrivate_ = new FakeUsersPrivate();
    document.body.appendChild(dialog);

    dialog.open();
  });

  teardown(() => {
    dialog!.remove();
    dialog = null;
  });

  /**
   * Test that the dialog reacts to valid and invalid input correctly.
   */
  test('Add user', () => {
    const userInputBox =
        dialog!.shadowRoot!.querySelector<CrInputElement>('#addUserInput');
    assertTrue(!!userInputBox);
    assertFalse(userInputBox.invalid);

    const addButton =
        dialog!.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!addButton);
    assertTrue(addButton.disabled);

    // Try to add a valid username without domain
    userInputBox.value = 'abcdef';
    assertFalse(addButton.disabled);
    assertFalse(userInputBox.invalid);

    // Try to add a valid username with domain
    userInputBox.value = 'abcdef@xyz.com';
    assertFalse(addButton.disabled);
    assertFalse(userInputBox.invalid);

    // Try to add an invalid username
    userInputBox.value = 'abcdef@';
    assertTrue(addButton.disabled);
    assertTrue(userInputBox.invalid);
  });

  test('Add duplicate user', async () => {
    const userInputBox =
        dialog!.shadowRoot!.querySelector<CrInputElement>('#addUserInput');
    assertTrue(!!userInputBox);

    const addButton =
        dialog!.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!addButton);

    // Add user for the first time.
    const duplicateUserEmail = 'duplicateUser@google.com';
    userInputBox.value = duplicateUserEmail;
    addButton.click();
    await waitAfterNextRender(userInputBox);
    assertEquals('', userInputBox.value);
    assertFalse(userInputBox.invalid);
    assertEquals('', userInputBox.errorMessage);

    // Add user for the second time. It should be registered as a duplicate and
    // will create an error message.
    userInputBox.value = duplicateUserEmail;
    addButton.click();
    await waitAfterNextRender(userInputBox);
    assertEquals(duplicateUserEmail, userInputBox.value);
    assertTrue(userInputBox.invalid);
    assertNotEquals('', userInputBox.errorMessage);
  });

  test('Add new user', async () => {
    const userInputBox =
        dialog!.shadowRoot!.querySelector<CrInputElement>('#addUserInput');
    assertTrue(!!userInputBox);

    const addButton =
        dialog!.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!addButton);

    const newUserEmail = 'newUser@google.com';
    userInputBox.value = newUserEmail;
    addButton.click();
    await waitAfterNextRender(userInputBox);
    assertEquals('', userInputBox.value);
    assertFalse(userInputBox.invalid);
    assertEquals('', userInputBox.errorMessage);
  });

  test('Add two new users', async () => {
    const userInputBox =
        dialog!.shadowRoot!.querySelector<CrInputElement>('#addUserInput');
    assertTrue(!!userInputBox);

    const addButton =
        dialog!.shadowRoot!.querySelector<HTMLButtonElement>('.action-button');
    assertTrue(!!addButton);

    const firstUserEmail = 'firstUser@google.com';
    const secondUserEmail = 'secondUser@google.com';

    userInputBox.value = firstUserEmail;
    addButton.click();
    await waitAfterNextRender(userInputBox);
    assertEquals('', userInputBox.value);
    assertFalse(userInputBox.invalid);
    assertEquals('', userInputBox.errorMessage);

    userInputBox.value = secondUserEmail;
    addButton.click();
    await waitAfterNextRender(userInputBox);
    assertEquals('', userInputBox.value);
    assertFalse(userInputBox.invalid);
    assertEquals('', userInputBox.errorMessage);
  });
});
