// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {Page, PASSWORD_NOTE_MAX_CHARACTER_COUNT, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

suite('EditPasswordDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let metricsTracker: MetricsTracker;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metricsTracker = fakeMetricsPrivate();
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    return flushTasks();
  });

  test('password displayed correctly', async function() {
    const password =
        createPasswordEntry({id: 0, username: 'user1', password: 'sTr0nGp@@s'});
    password.affiliatedDomains = [
      createAffiliatedDomain('test.com'),
      createAffiliatedDomain('m.test.com'),
    ];

    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = password;
    document.body.appendChild(dialog);
    await flushTasks();

    assertEquals(password.username, dialog.$.usernameInput.value);
    assertEquals(password.password, dialog.$.passwordInput.value);
    assertEquals('password', dialog.$.passwordInput.type);

    const listItemElements =
        dialog.shadowRoot!.querySelectorAll<HTMLAnchorElement>('a.site-link');
    assertEquals(listItemElements.length, password.affiliatedDomains.length);

    password.affiliatedDomains.forEach((expectedDomain, i) => {
      const listItemElement = listItemElements[i];

      assertTrue(!!listItemElement);
      assertEquals(expectedDomain.name, listItemElement.textContent!.trim());
      assertEquals(expectedDomain.url, listItemElement.href);
    });
  });

  test('show/hide password', async function() {
    const password = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'vik', password: 'password69'});
    password.affiliatedDomains = [createAffiliatedDomain('test.com')];
    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = password;
    document.body.appendChild(dialog);
    await flushTasks();

    assertEquals(
        loadTimeData.getString('showPassword'),
        dialog.$.showPasswordButton.title);
    assertEquals('password', dialog.$.passwordInput.type);
    assertTrue(dialog.$.showPasswordButton.hasAttribute('class'));
    assertEquals(
        'icon-visibility', dialog.$.showPasswordButton.getAttribute('class'));

    dialog.$.showPasswordButton.click();

    assertEquals(
        loadTimeData.getString('hidePassword'),
        dialog.$.showPasswordButton.title);
    assertEquals('text', dialog.$.passwordInput.type);
    assertTrue(dialog.$.showPasswordButton.hasAttribute('class'));
    assertEquals(
        'icon-visibility-off',
        dialog.$.showPasswordButton.getAttribute('class'));
  });

  test('username validation works', async function() {
    passwordManager.data.passwords = [
      createPasswordEntry(
          {url: 'www.example.com', username: 'username1', password: 'pass'}),
      createPasswordEntry(
          {url: 'www.example2.com', username: 'username2', password: 'pass'}),
    ];
    passwordManager.data.passwords[0]!.affiliatedDomains =
        [createAffiliatedDomain('www.example.com')];
    passwordManager.data.passwords[1]!.affiliatedDomains = [
      createAffiliatedDomain('www.example.com'),
      createAffiliatedDomain('www.example2.com'),
    ];

    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = passwordManager.data.passwords[1]!;
    document.body.appendChild(dialog);
    await flushTasks();

    // Update username to the same value as other credential and observe error.
    dialog.$.usernameInput.value = 'username1';
    await dialog.$.usernameInput.updateComplete;
    assertTrue(dialog.$.usernameInput.invalid);
    assertEquals(
        dialog.i18n('usernameAlreadyUsed', 'www.example.com'),
        dialog.$.usernameInput.errorMessage);
  });

  test('username validation ignores passkeys & federated', async function() {
    passwordManager.data.passwords = [
      createPasswordEntry({
        url: 'www.example.com',
        username: 'password-username',
        password: 'pass',
      }),
      createPasswordEntry({
        url: 'www.example.com',
        username: 'passkey-username',
        isPasskey: true,
      }),
      createPasswordEntry({
        url: 'www.example.com',
        username: 'federated-username',
        federationText: 'Sign in via google.com',
      }),
    ];
    passwordManager.data.passwords[0]!.affiliatedDomains =
        [createAffiliatedDomain('www.example.com')];
    passwordManager.data.passwords[1]!.affiliatedDomains =
        [createAffiliatedDomain('www.example.com')];
    passwordManager.data.passwords[2]!.affiliatedDomains =
        [createAffiliatedDomain('www.example.com')];

    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = passwordManager.data.passwords[0]!;
    document.body.appendChild(dialog);
    await flushTasks();

    // Update username to the same value as the passkey. There should not be an
    // error.
    dialog.$.usernameInput.value = 'passkey-username';
    await dialog.$.usernameInput.updateComplete;
    assertFalse(dialog.$.usernameInput.invalid);

    // Update username to the same value as the federated credential.
    dialog.$.usernameInput.value = 'federated-username';
    await dialog.$.usernameInput.updateComplete;
    assertFalse(dialog.$.usernameInput.invalid);
  });

  test('view duplicated password', async function() {
    passwordManager.data.passwords = [
      createPasswordEntry(
          {url: 'www.example.com', username: 'test', password: 'pass'}),
      createPasswordEntry(
          {url: 'www.example2.com', username: 'test2', password: 'pass'}),
    ];
    passwordManager.data.passwords[0]!.affiliatedDomains =
        [createAffiliatedDomain('www.example.com')];
    passwordManager.data.passwords[1]!.affiliatedDomains = [
      createAffiliatedDomain('www.example.com'),
      createAffiliatedDomain('www.example2.com'),
    ];

    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = passwordManager.data.passwords[1]!;
    document.body.appendChild(dialog);
    await flushTasks();

    // Update username to the same value as other credential and observe error.
    dialog.$.usernameInput.value = 'test';
    await dialog.$.usernameInput.updateComplete;
    assertTrue(dialog.$.usernameInput.invalid);
    assertEquals(
        dialog.i18n('usernameAlreadyUsed', 'www.example.com'),
        dialog.$.usernameInput.errorMessage);

    assertTrue(dialog.$.viewExistingPasswordLink.hidden);
    dialog.showRedirect = true;

    assertFalse(dialog.$.viewExistingPasswordLink.hidden);

    dialog.$.viewExistingPasswordLink.click();
    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    assertEquals('www.example.com', Router.getInstance().currentRoute.details);
  });

  test('note validation works', async function() {
    const password = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'vik', password: 'password69'});
    password.affiliatedDomains = [createAffiliatedDomain('test.com')];
    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = password;
    document.body.appendChild(dialog);
    await flushTasks();

    assertFalse(dialog.$.passwordNote.invalid);

    // Make note 899 characters long.
    dialog.$.passwordNote.value = '.'.repeat(899);
    assertFalse(dialog.$.passwordNote.invalid);
    assertEquals('', dialog.$.passwordNote.firstFooter);
    assertEquals('', dialog.$.passwordNote.secondFooter);

    // After 900 characters there are footers.
    dialog.$.passwordNote.value = '.'.repeat(900);
    await flushTasks();
    assertFalse(dialog.$.passwordNote.invalid);
    assertEquals(
        dialog.i18n(
            'passwordNoteCharacterCountWarning',
            PASSWORD_NOTE_MAX_CHARACTER_COUNT),
        dialog.$.passwordNote.firstFooter);
    assertEquals(
        dialog.i18n(
            'passwordNoteCharacterCount', 900,
            PASSWORD_NOTE_MAX_CHARACTER_COUNT),
        dialog.$.passwordNote.secondFooter);

    // After PASSWORD_NOTE_MAX_CHARACTER_COUNT + 1 characters note is no longer
    // valid.
    dialog.$.passwordNote.value =
        '.'.repeat(PASSWORD_NOTE_MAX_CHARACTER_COUNT + 1);
    await flushTasks();
    assertTrue(dialog.$.passwordNote.invalid);
    assertEquals(
        dialog.i18n(
            'passwordNoteCharacterCountWarning',
            PASSWORD_NOTE_MAX_CHARACTER_COUNT),
        dialog.$.passwordNote.firstFooter);
    assertEquals(
        dialog.i18n(
            'passwordNoteCharacterCount', PASSWORD_NOTE_MAX_CHARACTER_COUNT + 1,
            1000),
        dialog.$.passwordNote.secondFooter);
  });

  test('password is updated', async function() {
    const password = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'username', password: 'password69'});
    password.affiliatedDomains = [createAffiliatedDomain('test.com')];
    const dialog = document.createElement('edit-password-dialog');
    dialog.credential = password;
    document.body.appendChild(dialog);
    await flushTasks();

    // Enter website
    dialog.$.usernameInput.value = 'username2';
    dialog.$.passwordInput.value = 'sTroNgPA$$wOrD';
    dialog.$.passwordNote.value = 'super secret note.';
    await Promise.all([
      dialog.$.usernameInput.updateComplete,
      dialog.$.passwordInput.updateComplete,
    ]);

    assertFalse(dialog.$.saveButton.disabled);
    dialog.$.saveButton.click();

    const updatedCredential =
        await passwordManager.whenCalled('changeCredential');

    assertEquals(password.id, updatedCredential.id);
    assertEquals(dialog.$.usernameInput.value, updatedCredential.username);
    assertEquals(dialog.$.passwordInput.value, updatedCredential.password);
    assertEquals(dialog.$.passwordNote.value, updatedCredential.note);
  });

  [{oldNote: '', newNote: '', expectedMetricBucket: 4},
   {oldNote: '', newNote: 'new note', expectedMetricBucket: 1},
   {oldNote: undefined, newNote: '', expectedMetricBucket: 4},
   {oldNote: 'some note', newNote: 'different note', expectedMetricBucket: 2},
   {oldNote: 'some note', newNote: '', expectedMetricBucket: 3},
   {oldNote: 'same note', newNote: 'same note', expectedMetricBucket: 4}]
      .forEach(
          testCase =>
              test(`changePasswordWithNotesForMetrics`, async function() {
                const password = createPasswordEntry({
                  id: 1,
                  url: 'test.com',
                  username: 'username',
                  password: 'password69',
                });
                password.affiliatedDomains =
                    [createAffiliatedDomain('test.com')];
                password.note = testCase.oldNote;
                const dialog = document.createElement('edit-password-dialog');
                dialog.credential = password;
                document.body.appendChild(dialog);
                await flushTasks();

                // Enter website
                dialog.$.passwordInput.value = 'sTroNgPA$$wOrD';
                dialog.$.passwordNote.value = testCase.newNote;
                await dialog.$.passwordInput.updateComplete;

                assertFalse(dialog.$.saveButton.disabled);
                dialog.$.saveButton.click();

                assertEquals(
                    1,
                    metricsTracker.count(
                        'PasswordManager.PasswordNoteActionInSettings2',
                        testCase.expectedMetricBucket));
              }));
});
