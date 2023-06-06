// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Password View page tests. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PasswordViewElement} from 'chrome://settings/lazy_load.js';
import {buildRouter, PasswordManagerImpl, Router, routes, SettingsRoutes} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createPasswordEntry} from './passwords_and_autofill_fake_data.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

function assertVisibilityOfPageElements(
    page: PasswordViewElement, visible: boolean) {
  ['#usernameInput',
   '#copyUsernameButton',
   '#passwordInput',
   '#showPasswordButton',
   '#copyPasswordButton',
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
   '#editButton',
  ]
      .forEach(
          element =>
              assertFalse(isVisible(page.shadowRoot!.querySelector(element))));
}

suite('PasswordViewTest', function() {
  const SITE = 'site1.com';
  const USERNAME = 'user1';
  const USERNAME2 = 'user2';
  const PASSWORD = 'p455w0rd';
  const NOTE = 'some note';
  const ID = 0;

  let passwordManager: TestPasswordManagerProxy;

  async function loadViewPage(
      credential?: chrome.passwordsPrivate.PasswordUiEntry, id?: number) {
    let requestedId;
    if (id !== undefined) {
      requestedId = id;
    } else if (!!credential && credential.id !== undefined) {
      requestedId = id;
    } else {
      requestedId = -1;
    }
    const params = new URLSearchParams({id: String(requestedId)});
    Router.getInstance().navigateTo(routes.PASSWORD_VIEW, params);
    const page = document.createElement('password-view');
    if (credential) {
      page.credential = credential;
    }
    document.body.appendChild(page);

    await flushTasks();
    return page;
  }

  setup(function() {
    loadTimeData.overrideValues({enablePasswordViewPage: true});
    Router.resetInstanceForTesting(buildRouter());
    routes.PASSWORD_VIEW =
        (Router.getInstance().getRoutes() as SettingsRoutes).PASSWORD_VIEW;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
  });

  test('Textarea is shown', async function() {
    const passwordEntry = createPasswordEntry({
      url: SITE,
      username: USERNAME,
      id: ID,
      note: NOTE,
    });

    const page = await loadViewPage(passwordEntry);

    assertVisibilityOfPageElements(page, /*visibility=*/ true);
    assertEquals(
        NOTE, page.shadowRoot!.querySelector('#note')!.innerHTML.trim());
  });

  [{
    requestedId: 1,
    expectedStoredIn: chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT,
    expectedUsername: USERNAME,
  },
   {
     requestedId: 2,
     expectedStoredIn: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
     expectedUsername: USERNAME,
   },
   {
     requestedId: 3,
     expectedStoredIn:
         chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT,
     expectedUsername: USERNAME2,
   },
   {
     requestedId: 4,
     expectedStoredIn: chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT,
     expectedUsername: USERNAME2,
   },
  ]
      .forEach(
          item => test(
              `IDs match to correct credentials for id: ${item.requestedId}`,
              async function() {
                const passwordList = [
                  // entry in the account store
                  createPasswordEntry({
                    url: SITE,
                    username: USERNAME,
                    id: 1,
                    inAccountStore: true,
                  }),
                  // entry in the profile store
                  createPasswordEntry({
                    url: SITE,
                    username: USERNAME,
                    id: 2,
                    inProfileStore: true,
                  }),
                  // entry in the both stores
                  createPasswordEntry({
                    url: SITE,
                    username: USERNAME2,
                    id: 3,
                    inProfileStore: true,
                    inAccountStore: true,
                  }),
                  createPasswordEntry({
                    url: SITE,
                    username: USERNAME2,
                    id: 4,
                    inAccountStore: true,
                  }),
                ];

                const requestedCredential = passwordList.find(
                    passwordUiEntry => passwordUiEntry.id === item.requestedId);
                const page = await loadViewPage(requestedCredential!);

                assertVisibilityOfPageElements(page, /*visibility=*/ true);
                assertEquals(item.requestedId, page.credential!.id);
                assertEquals(item.expectedStoredIn, page.credential!.storedIn);
                assertEquals(item.expectedUsername, page.credential!.username);
                assertEquals(SITE, page.credential!.urls.shown);
              }));

  test('Empty note shows placeholder text', async function() {
    const passwordEntry =
        createPasswordEntry({url: SITE, username: USERNAME, id: ID});

    const page = await loadViewPage(passwordEntry);
    assertEquals(
        'No note added',
        page.shadowRoot!.querySelector('#note')!.innerHTML.trim());
  });

  test('Federated credential layout', async function() {
    const passwordEntry = createPasswordEntry({
      federationText: 'with chromium.org',
      url: SITE,
      username: USERNAME,
      id: ID,
    });

    const page = await loadViewPage(passwordEntry);

    assertVisibilityOfFederatedCredentialElements(page);
  });

  test('Clicking show password button shows / hides it', async function() {
    const passwordEntry =
        createPasswordEntry({url: SITE, username: USERNAME, id: ID});
    passwordEntry.password = PASSWORD;


    const page = await loadViewPage(passwordEntry);

    const passwordInput =
        page.shadowRoot!.querySelector<HTMLInputElement>('#passwordInput');
    const showButton = page.shadowRoot!.querySelector<HTMLButtonElement>(
        '#showPasswordButton');
    assertTrue(!!passwordInput);
    assertTrue(!!showButton);
    assertEquals('password', passwordInput.type);
    assertEquals(PASSWORD, passwordInput.value);
    assertTrue(showButton.classList.contains('icon-visibility'));

    // hide the password
    showButton.click();
    flush();

    assertEquals('text', passwordInput.type);
    assertEquals(PASSWORD, passwordInput.value);
    assertTrue(showButton.classList.contains('icon-visibility-off'));

    // hide the password
    showButton.click();
    flush();

    assertEquals('password', passwordInput.type);
    assertEquals(PASSWORD, passwordInput.value);
    assertTrue(showButton.classList.contains('icon-visibility'));
  });

  test(
      'When edit button is tapped, the edit dialog is open with credential. ' +
          'When the username is changed, view page gets updated',
      async function() {
        const NEW_USERNAME = 'user2';
        const NEW_ID = ID + 1;
        const entry =
            createPasswordEntry({url: SITE, username: USERNAME, id: ID});
        entry.password = PASSWORD;

        const page = await loadViewPage(entry);

        page.shadowRoot!.querySelector<HTMLButtonElement>(
                            '#editButton')!.click();
        flush();

        const editDialog =
            page.shadowRoot!.querySelector('password-edit-dialog');
        assertTrue(!!editDialog);
        assertTrue(!!editDialog.existingEntry);
        assertEquals(entry.urls, editDialog.existingEntry.urls);
        assertEquals(entry.username, editDialog.existingEntry.username);
        assertEquals(PASSWORD, editDialog.existingEntry.password);

        passwordManager.setChangeSavedPasswordResponse(NEW_ID);
        // user edits the username
        editDialog.$.usernameInput.value = NEW_USERNAME;
        editDialog.$.actionButton.click();
        await flushTasks();

        assertFalse(isVisible(editDialog));

        assertEquals(NEW_USERNAME, page.credential!.username);

        const urlParams = Router.getInstance().getQueryParameters();
        assertEquals(urlParams.get('id'), String(NEW_ID));
        assertEquals(
            routes.PASSWORD_VIEW, Router.getInstance().getCurrentRoute());
      });

  test(
      'When delete button is clicked for a password on device, ' +
          'it is deleted and routed to passwords page',
      async function() {
        const entry = createPasswordEntry(
            {url: SITE, username: USERNAME, id: ID, inAccountStore: false});

        const page = await loadViewPage(entry);

        page.shadowRoot!.querySelector<HTMLButtonElement>(
                            '#deleteButton')!.click();
        const {id, fromStores} =
            await passwordManager.whenCalled('removeCredential');
        assertEquals(ID, id);
        assertEquals(
            chrome.passwordsPrivate.PasswordStoreSet.DEVICE, fromStores);
        await flushTasks();

        assertEquals(routes.PASSWORDS, Router.getInstance().getCurrentRoute());
        const newParams = Router.getInstance().getQueryParameters();
        assertEquals(entry.storedIn, newParams.get('removedFromStores'));
      });

  test(
      'When delete button is clicked for a password on device and account, ' +
          'remove dialog is opened',
      async function() {
        const entry = createPasswordEntry({
          url: SITE,
          username: USERNAME,
          id: ID,
          inAccountStore: true,
          inProfileStore: true,
        });
        entry.password = PASSWORD;
        entry.note = NOTE;

        const page = await loadViewPage(entry);

        page.shadowRoot!.querySelector<HTMLButtonElement>(
                            '#deleteButton')!.click();
        flush();

        const dialog = page.shadowRoot!.querySelector('password-remove-dialog');
        assertTrue(!!dialog);
        assertDeepEquals(entry, dialog.duplicatedPassword);

        // click delete on the dialog.
        dialog.$.removeButton.click();
        const {id, fromStores} =
            await passwordManager.whenCalled('removeCredential');
        assertEquals(ID, id);
        assertEquals(
            chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT,
            fromStores);
        await flushTasks();

        assertEquals(
            routes.PASSWORDS.path, Router.getInstance().getCurrentRoute().path);
        const pageParams = Router.getInstance().getQueryParameters();
        assertEquals(
            chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT,
            pageParams.get('removedFromStores'));
      });

  test('Copy password button shows the copy toast', async function() {
    const passwordEntry =
        createPasswordEntry({url: SITE, username: USERNAME, id: ID});

    const page = await loadViewPage(passwordEntry);

    const copyButton = page.shadowRoot!.querySelector<HTMLButtonElement>(
        '#copyPasswordButton')!;
    assertFalse(page.$.toast.open);

    passwordManager.setPlaintextPassword(PASSWORD);
    copyButton.click();
    await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();
    assertTrue(page.$.toast.open);
  });

  test(
      'When visibility state is hidden on page load, ' +
          'credential is not requested until the page becomes visible',
      async function() {
        let eventCount = 0;
        const eventHandler = (_event: any) => {
          eventCount += 1;
        };
        document.addEventListener('password-view-page-requested', eventHandler);
        Object.defineProperty(
            document, 'visibilityState', {value: 'hidden', writable: true});
        document.dispatchEvent(new Event('visibilitychange'));

        const page = await loadViewPage();

        assertEquals(0, eventCount);
        assertFalse(!!page.credential);

        Object.defineProperty(
            document, 'visibilityState', {value: 'visible', writable: true});
        document.dispatchEvent(new Event('visibilitychange'));
        await flushTasks();
        assertEquals(1, eventCount);
      });

  test('Timeout listener closes the view page', async function() {
    const passwordEntry =
        createPasswordEntry({url: SITE, username: USERNAME, id: ID});

    const page = await loadViewPage(passwordEntry);

    assertTrue(
        !!passwordManager.lastCallback.addPasswordManagerAuthTimeoutListener);
    assertTrue(!!page.credential);

    passwordManager.lastCallback.addPasswordManagerAuthTimeoutListener();
    await flushTasks();

    assertFalse(!!page.credential);
    assertEquals(routes.PASSWORDS, Router.getInstance().getCurrentRoute());
  });
});
