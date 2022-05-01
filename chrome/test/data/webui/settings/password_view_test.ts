// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Password View page tests. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PasswordViewElement} from 'chrome://settings/lazy_load.js';
import {buildRouter, PasswordManagerImpl, Router, routes} from 'chrome://settings/settings.js';
import {SettingsRoutes} from 'chrome://settings/settings_routes.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isVisible} from 'chrome://webui-test/test_util.js';

import {createMultiStorePasswordEntry, createPasswordEntry} from './passwords_and_autofill_fake_data.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

function assertVisibilityOfPageElements(
    page: PasswordViewElement, visible: boolean) {
  ['#usernameInput',
   '#copyUsernameButton',
   '#passwordInput',
   '#showPasswordButton',
   '#copyPasswordButton',
   'settings-textarea',
   '#editButton',
   '#deleteButton',
  ]
      .forEach(
          element => assertEquals(
              visible, isVisible(page.shadowRoot!.querySelector(element))));
}

function assertVisibilityOfFederatedCredentialElements(
    page: PasswordViewElement) {
  ['#usernameInput',
   '#copyUsernameButton',
   '#passwordInput',
   '#deleteButton',
  ]
      .forEach(
          element =>
              assertTrue(isVisible(page.shadowRoot!.querySelector(element))));

  ['#showPasswordButton',
   '#copyPasswordButton',
   'settings-textarea',
   '#editButton',
  ]
      .forEach(
          element =>
              assertFalse(isVisible(page.shadowRoot!.querySelector(element))));
}

suite('PasswordViewTest', function() {
  const SITE = 'site1.com';
  const USERNAME = 'user1';
  const PASSWORD = 'p455w0rd';
  const ID = 0;

  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    loadTimeData.overrideValues({enablePasswordNotes: true});
    Router.resetInstanceForTesting(buildRouter());
    routes.PASSWORD_VIEW =
        (Router.getInstance().getRoutes() as SettingsRoutes).PASSWORD_VIEW;
    document.body.innerHTML = '';
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
  });

  test('Valid site and username displays an entry', async function() {
    const passwordList = [
      createPasswordEntry({url: SITE, username: USERNAME, id: ID}),
    ];

    passwordManager.setPlaintextPassword(PASSWORD);
    passwordManager.data.passwords = passwordList;
    const page = document.createElement('password-view');
    document.body.appendChild(page);
    const params = new URLSearchParams({
      username: USERNAME,
      site: SITE,
    });
    Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);

    await flushTasks();
    assertVisibilityOfPageElements(page, /*visibility=*/ true);
  });

  test(
      'Invalid site and username does not display an entry ' +
          'and routes to passwords page',
      async function() {
        const passwordList = [
          createPasswordEntry({url: SITE, username: 'user2', id: ID}),
        ];

        passwordManager.data.passwords = passwordList;
        const page = document.createElement('password-view');
        document.body.appendChild(page);
        const params = new URLSearchParams({
          username: USERNAME,
          site: SITE,
        });
        Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);

        await flushTasks();
        assertVisibilityOfPageElements(page, /*visibility=*/ false);

        assertEquals(routes.PASSWORDS, Router.getInstance().getCurrentRoute());
      });

  test('Federated credential layout', async function() {
    const passwordList = [
      createPasswordEntry({
        federationText: 'with chromium.org',
        url: SITE,
        username: USERNAME,
      }),
    ];
    passwordManager.data.passwords = passwordList;
    const page = document.createElement('password-view');
    document.body.appendChild(page);
    const params = new URLSearchParams({
      username: USERNAME,
      site: SITE,
    });
    Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);

    await flushTasks();
    assertVisibilityOfFederatedCredentialElements(page);
  });

  test('Clicking show password button shows / hides it', async function() {
    const passwordList = [
      createPasswordEntry({url: SITE, username: USERNAME, id: ID}),
    ];

    passwordManager.data.passwords = passwordList;
    const page = document.createElement('password-view');
    document.body.appendChild(page);
    const params = new URLSearchParams({
      username: USERNAME,
      site: SITE,
    });
    Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);

    await flushTasks();
    const passwordInput =
        page.shadowRoot!.querySelector<HTMLInputElement>('#passwordInput');
    const showButton = page.shadowRoot!.querySelector<HTMLButtonElement>(
        '#showPasswordButton');
    assertTrue(!!passwordInput);
    assertTrue(!!showButton);
    assertEquals('password', passwordInput.type);
    assertEquals(' '.repeat(10), passwordInput.value);
    assertTrue(showButton.classList.contains('icon-visibility'));

    showButton.click();

    // this will fail because setPlaintextPassword is not called.
    await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();
    assertEquals('password', passwordInput.type);
    assertEquals(' '.repeat(10), passwordInput.value);
    assertTrue(showButton.classList.contains('icon-visibility'));

    passwordManager.setPlaintextPassword(PASSWORD);
    // show the password
    showButton.click();

    const {id, reason} =
        await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();
    assertEquals(ID, id);
    assertEquals(chrome.passwordsPrivate.PlaintextReason.VIEW, reason);
    assertEquals('text', passwordInput.type);
    assertEquals(PASSWORD, passwordInput.value);
    assertTrue(showButton.classList.contains('icon-visibility-off'));

    // hide the password by re-clicking
    showButton.click();
    flush();
    assertEquals('password', passwordInput.type);
    assertEquals(' '.repeat(10), passwordInput.value);
    assertTrue(showButton.classList.contains('icon-visibility'));
  });

  test(
      'When saved passwords change credential is still shown',
      async function() {
        const passwordEntry =
            createPasswordEntry({url: SITE, username: USERNAME, id: ID});

        passwordManager.data.passwords = [passwordEntry];
        const page = document.createElement('password-view');
        document.body.appendChild(page);
        const params = new URLSearchParams({
          username: USERNAME,
          site: SITE,
        });
        Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);
        await flushTasks();
        assertTrue(!!page.credential);
        assertEquals(SITE, page.credential.urls.shown);
        assertEquals(USERNAME, page.credential.username);

        passwordManager.lastCallback.addSavedPasswordListChangedListener!
            ([passwordEntry].concat([
              createPasswordEntry({url: 'site2.com', username: 'user2', id: 1}),
            ]));

        await flushTasks();
        assertTrue(!!page.credential);
        assertEquals(SITE, page.credential.urls.shown);
        assertEquals(USERNAME, page.credential.username);
      });

  test(
      'When saved passwords change and credential is removed, page is empty',
      async function() {
        const passwordList = [
          createPasswordEntry({url: SITE, username: USERNAME, id: ID}),
        ];

        passwordManager.setPlaintextPassword(PASSWORD);
        passwordManager.data.passwords = passwordList;
        const page = document.createElement('password-view');
        document.body.appendChild(page);
        const params = new URLSearchParams({
          username: USERNAME,
          site: SITE,
        });
        Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);

        await flushTasks();
        assertVisibilityOfPageElements(page, /*visibility=*/ true);

        passwordManager.lastCallback.addSavedPasswordListChangedListener!([]);

        await flushTasks();
        assertVisibilityOfPageElements(page, /*visibility=*/ false);
      });

  test(
      'When edit button is tapped, the edit dialog is open with credential. ' +
          'When the username is changed, view page gets updated',
      async function() {
        const NEW_USERNAME = 'user2';
        const entry =
            createPasswordEntry({url: SITE, username: USERNAME, id: 0});

        passwordManager.setPlaintextPassword(PASSWORD);
        passwordManager.data.passwords = [entry];
        const page = document.createElement('password-view');
        document.body.appendChild(page);
        const params = new URLSearchParams({
          username: USERNAME,
          site: SITE,
        });
        Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);

        await flushTasks();

        page.shadowRoot!.querySelector<HTMLButtonElement>(
                            '#editButton')!.click();
        flush();

        await passwordManager.whenCalled('requestPlaintextPassword');
        await flushTasks();

        const editDialog =
            page.shadowRoot!.querySelector('password-edit-dialog');
        assertTrue(!!editDialog);
        assertTrue(!!editDialog.existingEntry);
        assertEquals(entry.urls, editDialog.existingEntry.urls);
        assertEquals(entry.username, editDialog.existingEntry.username);
        assertEquals(PASSWORD, editDialog.existingEntry.password);

        // user edits the username
        editDialog.$.usernameInput.value = NEW_USERNAME;
        editDialog.$.actionButton.click();
        entry.username = NEW_USERNAME;
        passwordManager.lastCallback.addSavedPasswordListChangedListener!
            ([entry]);

        await flushTasks();
        assertFalse(isVisible(editDialog));

        assertEquals(NEW_USERNAME, page.credential!.username);

        const urlParams = Router.getInstance().getQueryParameters();
        assertEquals(urlParams.get('site'), SITE);
        assertEquals(urlParams.get('username'), NEW_USERNAME);
        assertEquals(
            routes.PASSWORD_VIEW, Router.getInstance().getCurrentRoute());
      });

  test(
      'When delete button is clicked for a password on device, ' +
          'it is deleted and routed to passwords page',
      async function() {
        const entry = createPasswordEntry(
            {url: SITE, username: USERNAME, id: ID, fromAccountStore: false});

        passwordManager.setPlaintextPassword(PASSWORD);
        passwordManager.data.passwords = [entry];
        const page = document.createElement('password-view');
        document.body.appendChild(page);
        const params = new URLSearchParams({
          username: USERNAME,
          site: SITE,
        });
        Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);

        await flushTasks();

        page.shadowRoot!.querySelector<HTMLButtonElement>(
                            '#deleteButton')!.click();
        const id = await passwordManager.whenCalled('removeSavedPassword');
        assertEquals(ID, id);
        await flushTasks();

        assertEquals(routes.PASSWORDS, Router.getInstance().getCurrentRoute());
        const newParams = Router.getInstance().getQueryParameters();
        assertEquals('false', newParams.get('removedFromAccount'));
        assertEquals('true', newParams.get('removedFromDevice'));
      });

  test(
      'When delete button is clicked for a password on device and account, ' +
          'remove dialog is opened',
      async function() {
        passwordManager.setPlaintextPassword(PASSWORD);
        passwordManager.data.passwords = [
          createPasswordEntry({
            url: SITE,
            username: USERNAME,
            id: 0,
            frontendId: ID,
            fromAccountStore: false
          }),
          createPasswordEntry({
            url: SITE,
            username: USERNAME,
            id: 1,
            frontendId: ID,
            fromAccountStore: true
          })
        ];
        const page = document.createElement('password-view');
        document.body.appendChild(page);
        const params = new URLSearchParams({
          username: USERNAME,
          site: SITE,
        });
        Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);
        await flushTasks();

        page.shadowRoot!.querySelector<HTMLButtonElement>(
                            '#deleteButton')!.click();
        flush();

        const dialog = page.shadowRoot!.querySelector('password-remove-dialog');
        assertTrue(!!dialog);
        assertDeepEquals(
            createMultiStorePasswordEntry(
                {url: SITE, username: USERNAME, deviceId: 0, accountId: 1}),
            dialog.duplicatedPassword);

        // click delete on the dialog.
        dialog.$.removeButton.click();
        await passwordManager.whenCalled('removeSavedPasswords');
        await flushTasks();

        assertEquals(
            routes.PASSWORDS.path, Router.getInstance().getCurrentRoute().path);
        const pageParams = Router.getInstance().getQueryParameters();
        assertEquals('true', pageParams.get('removedFromAccount'));
        assertEquals('true', pageParams.get('removedFromDevice'));
      });
});
