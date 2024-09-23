// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {EditPasswordDialogElement, PasswordDetailsCardElement} from 'chrome://password-manager/password_manager.js';
import {Page, PasswordManagerImpl, PasswordViewPageInteractions, Router, SyncBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {createAffiliatedDomain, createPasswordEntry, makePasswordManagerPrefs} from './test_util.js';

async function createCardElement(
    password: chrome.passwordsPrivate.PasswordUiEntry|null =
        null): Promise<PasswordDetailsCardElement> {
  if (!password) {
    password = createPasswordEntry(
        {url: 'test.com', username: 'vik', password: 'password47'});
  }

  const card = document.createElement('password-details-card');
  card.password = password;
  card.prefs = makePasswordManagerPrefs();
  document.body.appendChild(card);
  await flushTasks();
  return card;
}

suite('PasswordDetailsCardTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let syncProxy: TestSyncBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);
    Router.getInstance().navigateTo(Page.PASSWORDS);
    return flushTasks();
  });

  test('Content displayed properly', async function() {
    const password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      password: 'password69',
      note: 'note',
    });

    const card = await createCardElement(password);

    assertEquals(password.username, card.$.usernameValue.value);
    assertEquals(password.password, card.$.passwordValue.value);
    assertEquals('password', card.$.passwordValue.type);
    assertTrue(isVisible(card.$.noteValue));
    assertEquals(password.note, card.$.noteValue.note);
    assertTrue(isVisible(card.$.showPasswordButton));
    assertTrue(isVisible(card.$.copyPasswordButton));
    assertTrue(isVisible(card.$.editButton));
    assertTrue(isVisible(card.$.deleteButton));
  });

  test('Content displayed properly for federated credential', async function() {
    const password = createPasswordEntry(
        {url: 'test.com', username: 'vik', federationText: 'federation.com'});

    const card = await createCardElement(password);

    assertEquals(password.username, card.$.usernameValue.value);
    assertEquals(password.federationText, card.$.passwordValue.value);
    assertEquals('text', card.$.passwordValue.type);
    assertFalse(isVisible(card.$.noteValue));
    assertFalse(isVisible(card.$.showPasswordButton));
    assertFalse(isVisible(card.$.copyPasswordButton));
    assertFalse(isVisible(card.$.editButton));
    assertTrue(isVisible(card.$.deleteButton));
  });

  test('Copy password', async function() {
    const password = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'vik', password: 'password69'});

    const card = await createCardElement(password);

    assertTrue(isVisible(card.$.copyPasswordButton));

    card.$.copyPasswordButton.click();
    await eventToPromise('value-copied', card);
    await passwordManager.whenCalled('extendAuthValidity');
    const {id, reason} =
        await passwordManager.whenCalled('requestPlaintextPassword');
    assertEquals(password.id, id);
    assertEquals(chrome.passwordsPrivate.PlaintextReason.COPY, reason);
    assertEquals(
        PasswordViewPageInteractions.PASSWORD_COPY_BUTTON_CLICKED,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));

    await flushTasks();
  });

  test('Links properly displayed', async function() {
    const password = createPasswordEntry(
        {url: 'test.com', username: 'vik', password: 'password69'});
    password.affiliatedDomains = [
      createAffiliatedDomain('test.com'),
      createAffiliatedDomain('m.test.com'),
    ];

    const card = await createCardElement(password);

    const listItemElements =
        card.shadowRoot!.querySelectorAll<HTMLAnchorElement>('a.site-link');
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

    const card = await createCardElement(password);

    assertEquals(
        loadTimeData.getString('showPassword'),
        card.$.showPasswordButton.title);
    assertEquals('password', card.$.passwordValue.type);
    assertTrue(card.$.showPasswordButton.hasAttribute('class'));
    assertEquals(
        'icon-visibility', card.$.showPasswordButton.getAttribute('class'));

    card.$.showPasswordButton.click();
    assertEquals(
        PasswordViewPageInteractions.PASSWORD_SHOW_BUTTON_CLICKED,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));

    assertEquals(
        loadTimeData.getString('hidePassword'),
        card.$.showPasswordButton.title);
    assertEquals('text', card.$.passwordValue.type);
    assertTrue(card.$.showPasswordButton.hasAttribute('class'));
    assertEquals(
        'icon-visibility-off', card.$.showPasswordButton.getAttribute('class'));
  });

  test('clicking edit button opens an edit dialog', async function() {
    const password = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'vik', password: 'password69'});
    password.affiliatedDomains = [createAffiliatedDomain('test.com')];

    const card = await createCardElement(password);

    card.$.editButton.click();
    await eventToPromise('cr-dialog-open', card);
    await passwordManager.whenCalled('extendAuthValidity');
    assertEquals(
        PasswordViewPageInteractions.PASSWORD_EDIT_BUTTON_CLICKED,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));
    await flushTasks();

    const editDialog =
        card.shadowRoot!.querySelector<EditPasswordDialogElement>(
            'edit-password-dialog');
    assertTrue(!!editDialog);
    assertTrue(editDialog.$.dialog.open);
  });

  test('delete password', async function() {
    const password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      id: 0,
    });

    const card = await createCardElement(password);

    assertTrue(isVisible(card.$.deleteButton));

    card.$.deleteButton.click();
    assertEquals(
        PasswordViewPageInteractions.PASSWORD_DELETE_BUTTON_CLICKED,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));

    const params = await passwordManager.whenCalled('removeCredential');
    assertEquals(params.id, password.id);
    assertEquals(params.fromStores, password.storedIn);
  });

  [chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT,
   chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
   chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT]
      .forEach(
          store => test(
              `delete multi store password from ${store} `, async function() {
                const password = createPasswordEntry({
                  url: 'test.com',
                  username: 'vik',
                  id: 0,
                });
                password.affiliatedDomains = [
                  createAffiliatedDomain('test.com'),
                  createAffiliatedDomain('m.test.com'),
                ];
                password.storedIn =
                    chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT;

                const card = await createCardElement(password);

                assertTrue(isVisible(card.$.deleteButton));

                card.$.deleteButton.click();
                await flushTasks();

                // Verify that password was not deleted immediately.
                assertEquals(
                    0, passwordManager.getCallCount('removeCredential'));

                const deleteDialog = card.shadowRoot!.querySelector(
                    'multi-store-delete-password-dialog');
                assertTrue(!!deleteDialog);
                assertTrue(deleteDialog.$.dialog.open);

                assertTrue(deleteDialog.$.removeFromAccountCheckbox.checked);
                assertTrue(deleteDialog.$.removeFromDeviceCheckbox.checked);

                if (store === chrome.passwordsPrivate.PasswordStoreSet.DEVICE) {
                  deleteDialog.$.removeFromAccountCheckbox.click();
                  await deleteDialog.$.removeFromAccountCheckbox.updateComplete;
                } else if (
                    store ===
                    chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT) {
                  deleteDialog.$.removeFromDeviceCheckbox.click();
                  await deleteDialog.$.removeFromDeviceCheckbox.updateComplete;
                }
                deleteDialog.$.removeButton.click();

                const params =
                    await passwordManager.whenCalled('removeCredential');
                assertEquals(password.id, params.id);
                assertEquals(store, params.fromStores);
              }));

  test('delete disabled when no store selected', async function() {
    const password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      id: 0,
      inAccountStore: true,
      inProfileStore: true,
    });
    password.affiliatedDomains = [
      createAffiliatedDomain('test.com'),
      createAffiliatedDomain('m.test.com'),
    ];

    const card = await createCardElement(password);

    assertTrue(isVisible(card.$.deleteButton));

    card.$.deleteButton.click();
    await flushTasks();

    // Verify that password was not deleted immediately.
    assertEquals(0, passwordManager.getCallCount('removeCredential'));

    const deleteDialog =
        card.shadowRoot!.querySelector('multi-store-delete-password-dialog');
    assertTrue(!!deleteDialog);
    assertTrue(deleteDialog.$.dialog.open);
    deleteDialog.$.removeFromAccountCheckbox.click();
    await deleteDialog.$.removeFromAccountCheckbox.updateComplete;
    deleteDialog.$.removeFromDeviceCheckbox.click();
    await deleteDialog.$.removeFromDeviceCheckbox.updateComplete;

    assertFalse(deleteDialog.$.removeFromAccountCheckbox.checked);
    assertFalse(deleteDialog.$.removeFromDeviceCheckbox.checked);

    assertTrue(deleteDialog.$.removeButton.disabled);
  });

  test('Sites title', async function() {
    const password = createPasswordEntry(
        {url: 'test.com', username: 'vik', password: 'password69'});
    password.affiliatedDomains = [
      {
        name: 'test.com',
        url: 'https://test.com',
        signonRealm: 'https://test.com/',
      },
    ];

    const card = await createCardElement(password);

    assertEquals(
        card.$.domainLabel.textContent!.trim(),
        loadTimeData.getString('sitesLabel'));
  });

  test('Apps title', async function() {
    const password = createPasswordEntry(
        {url: 'test.com', username: 'vik', password: 'password69'});
    password.affiliatedDomains = [
      {
        name: 'test.com',
        url: 'https://test.com',
        signonRealm: 'android://someHash/',
      },
    ];

    const card = await createCardElement(password);

    assertEquals(
        card.$.domainLabel.textContent!.trim(),
        loadTimeData.getString('appsLabel'));
  });

  test('Apps and sites title', async function() {
    const password = createPasswordEntry(
        {url: 'test.com', username: 'vik', password: 'password69'});
    password.affiliatedDomains = [
      {
        name: 'test.com',
        url: 'https://test.com',
        signonRealm: 'android://someHash/',
      },
      {
        name: 'test.com',
        url: 'https://test.com',
        signonRealm: 'https://test.com/',
      },
    ];

    const card = await createCardElement(password);

    assertEquals(
        card.$.domainLabel.textContent!.trim(),
        loadTimeData.getString('sitesAndAppsLabel'));
  });

  // <if expr="_google_chrome">
  test('share button available when sync enabled', async function() {
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: false,
      isSyncingPasswords: true,
    };

    const card = await createCardElement();

    assertTrue(isVisible(card.$.shareButton));
    assertEquals(card.$.shareButton.textContent!.trim(), card.i18n('share'));

    assertFalse(!!card.shadowRoot!.querySelector('share-password-flow'));

    // Share flow should become available after the button click.
    card.$.shareButton.click();
    await passwordManager.whenCalled('fetchFamilyMembers');
    await flushTasks();

    const shareFlow = card.shadowRoot!.querySelector('share-password-flow');
    assertTrue(!!shareFlow);
  });

  test('share button available for account store users', async function() {
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    passwordManager.data.isAccountStorageEnabled = true;

    const card = await createCardElement();

    assertFalse(card.$.shareButton.hidden);
    assertTrue(isVisible(card.$.shareButton));
    assertFalse(card.$.shareButton.disabled);
    assertEquals(card.$.shareButton.textContent!.trim(), card.i18n('share'));
  });

  test('sharing disabled by policy', async function() {
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: false,
      isSyncingPasswords: true,
    };

    const card = document.createElement('password-details-card');
    card.password = createPasswordEntry();
    card.prefs = makePasswordManagerPrefs();
    card.prefs.password_manager.password_sharing_enabled.value = false;
    card.prefs.password_manager.password_sharing_enabled.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    document.body.appendChild(card);
    await flushTasks();

    assertTrue(isVisible(card.$.shareButton));
    assertTrue(card.$.shareButton.disabled);
  });

  test('sharing unavailable for federated credentials', async function() {
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: false,
      isSyncingPasswords: true,
    };

    const card =
        await createCardElement(createPasswordEntry({federationText: 'text'}));

    assertFalse(isVisible(card.$.shareButton));

    const sharePasswordFlow =
        card.shadowRoot!.querySelector('share-password-flow');
    assertFalse(!!sharePasswordFlow);
  });

  test('share button unavailable when sync disabled', async function() {
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: false,
      isSyncingPasswords: false,
    };

    const card = await createCardElement();

    assertFalse(isVisible(card.$.shareButton));

    const sharePasswordFlow =
        card.shadowRoot!.querySelector('share-password-flow');
    assertFalse(!!sharePasswordFlow);
  });
  // </if>

  test(
      'clicking save password in account opens move password dialog',
      async function() {
        passwordManager.data.isAccountStorageEnabled = true;
        syncProxy.syncInfo = {
          isEligibleForAccountStorage: true,
          isSyncingPasswords: false,
        };

        const card = await createCardElement();
        card.isUsingAccountStore = true;
        await flushTasks();

        const movePasswordLabel = card!.shadowRoot!.querySelector<HTMLElement>(
            '.move-password-container div');
        assertTrue(!!movePasswordLabel);
        assertTrue(isVisible(movePasswordLabel));

        movePasswordLabel!.click();
        await flushTasks();

        const moveDialog =
            card.shadowRoot!.querySelector('move-single-password-dialog');
        assertTrue(!!moveDialog);
        const dialog = moveDialog!.shadowRoot!.querySelector('#dialog');
        assertTrue(!!dialog);
      });

  test('Password value is hidden if object was changed', async function() {
    const password1 = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'vik', password: 'password69'});
    password1.affiliatedDomains = [createAffiliatedDomain('test.com')];

    const password2 = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'viktor', password: 'password69'});
    password2.affiliatedDomains = [createAffiliatedDomain('test.com')];

    const card = await createCardElement(password1);
    card.isPasswordVisible = true;

    card.password = password2;
    assertFalse(card.isPasswordVisible);
  });
});
