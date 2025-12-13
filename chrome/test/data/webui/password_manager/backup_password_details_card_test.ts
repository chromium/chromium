// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {BackupPasswordDetailsCardElement} from 'chrome://password-manager/password_manager.js';
import {Page, PasswordManagerImpl, PasswordViewPageInteractions, Router, SyncBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

async function createCardElement(
    password: chrome.passwordsPrivate.PasswordUiEntry|null =
        null): Promise<BackupPasswordDetailsCardElement> {
  if (!password) {
    password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });
  }

  const card = document.createElement('backup-password-details-card');
  card.password = password;
  document.body.appendChild(card);
  await flushTasks();
  return card;
}

suite('BackupPasswordDetailsCardTest', function() {
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
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });

    const card = await createCardElement(password);

    assertEquals(password.username, card.$.usernameValue.value);
    assertEquals(password.backupPassword!.value, card.$.passwordValue.value);
    assertEquals('password', card.$.passwordValue.type);
    assertEquals(
        card.$.noteValue.note,
        `${card.i18n('passwordDetailsCardBackupPasswordNote')}\n\n${
            card.i18n(
                'passwordDetailsCardBackupPasswordNoteDetails',
                password.backupPassword!.creationDate,
                card.i18n('localPasswordManager'))}`);
    assertTrue(card.$.noteValue.alwaysExpanded);
    assertTrue(isVisible(card.$.showPasswordButton));
    assertTrue(isVisible(card.$.copyPasswordButton));
    assertTrue(isVisible(card.$.deleteButton));
  });

  test('Copy password', async function() {
    const password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      password: 'password',
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });

    const card = await createCardElement(password);

    assertTrue(isVisible(card.$.copyPasswordButton));

    card.$.copyPasswordButton.click();
    await eventToPromise('value-copied', card);
    await passwordManager.whenCalled('extendAuthValidity');
    const {id} =
        await passwordManager.whenCalled('copyPlaintextBackupPassword');
    assertEquals(password.id, id);
    assertEquals(
        PasswordViewPageInteractions.PASSWORD_COPY_BUTTON_CLICKED,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));

    await flushTasks();
  });

  test('Delete password', async function() {
    const password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      password: 'password',
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });

    const card = await createCardElement(password);

    assertTrue(isVisible(card.$.deleteButton));

    card.$.deleteButton.click();

    const params = await passwordManager.whenCalled('removeBackupPassword');
    assertEquals(params.id, password.id);
  });

  test('Links properly displayed', async function() {
    const password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });
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
      assertEquals(expectedDomain.name, listItemElement.textContent.trim());
      assertEquals(expectedDomain.url, listItemElement.href);
    });
  });

  test('show/hide password', async function() {
    const password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      password: 'password',
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });

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

  test('Sites title', async function() {
    const password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });
    password.affiliatedDomains = [
      {
        name: 'test.com',
        url: 'https://test.com',
        signonRealm: 'https://test.com/',
      },
    ];

    const card = await createCardElement(password);

    assertEquals(
        card.$.domainLabel.textContent.trim(),
        loadTimeData.getString('sitesLabel'));
  });

  test('Apps title', async function() {
    const password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });
    password.affiliatedDomains = [
      {
        name: 'test.com',
        url: 'https://test.com',
        signonRealm: 'android://someHash/',
      },
    ];

    const card = await createCardElement(password);

    assertEquals(
        card.$.domainLabel.textContent.trim(),
        loadTimeData.getString('appsLabel'));
  });

  test('Apps and sites title', async function() {
    const password = createPasswordEntry({
      url: 'test.com',
      username: 'vik',
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });
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
        card.$.domainLabel.textContent.trim(),
        loadTimeData.getString('sitesAndAppsLabel'));
  });

  test('Password value is hidden if object was changed', async function() {
    const password1 = createPasswordEntry({
      id: 1,
      url: 'test.com',
      username: 'vik',
      password: 'password69',
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });
    password1.affiliatedDomains = [createAffiliatedDomain('test.com')];

    const password2 = createPasswordEntry({
      id: 1,
      url: 'test.com',
      username: 'viktor',
      password: 'password69',
      backupPassword: {value: 'backup', creationDate: 'Mar 17'},
    });
    password2.affiliatedDomains = [createAffiliatedDomain('test.com')];

    const card = await createCardElement(password1);
    card.isPasswordVisible = true;

    card.password = password2;
    assertFalse(card.isPasswordVisible);
  });
});
