// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {EditPasswordDialogElement, Page, PasswordDetailsCardElement, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createPasswordEntry} from './test_util.js';

suite('PasswordDetailsCardTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    Router.getInstance().navigateTo(Page.PASSWORDS);
    return flushTasks();
  });

  test('Content displayed properly', async function() {
    const password = createPasswordEntry(
        {url: 'test.com', username: 'vik', password: 'password69'});

    const card = document.createElement('password-details-card');
    card.password = password;
    document.body.appendChild(card);
    await flushTasks();

    assertEquals(password.username, card.$.usernameValue.value);
    assertEquals(password.password, card.$.passwordValue.value);
    assertEquals('password', card.$.passwordValue.type);
    assertTrue(isVisible(card.$.noteValue));
    assertEquals(
        loadTimeData.getString('emptyNote'),
        card.$.noteValue.textContent!.trim());
    assertTrue(isVisible(card.$.copyUsernameButton));
    assertTrue(isVisible(card.$.showPasswordButton));
    assertTrue(isVisible(card.$.copyPasswordButton));
    assertTrue(isVisible(card.$.editButton));
    assertTrue(isVisible(card.$.deleteButton));
  });

  test('Content displayed properly for federated credential', async function() {
    const password = createPasswordEntry(
        {url: 'test.com', username: 'vik', federationText: 'federation.com'});

    const card: PasswordDetailsCardElement =
        document.createElement('password-details-card');
    card.password = password;
    document.body.appendChild(card);
    await flushTasks();

    assertEquals(password.username, card.$.usernameValue.value);
    assertEquals(password.federationText, card.$.passwordValue.value);
    assertEquals('text', card.$.passwordValue.type);
    assertFalse(isVisible(card.$.noteValue));
    assertTrue(isVisible(card.$.copyUsernameButton));
    assertFalse(isVisible(card.$.showPasswordButton));
    assertFalse(isVisible(card.$.copyPasswordButton));
    assertFalse(isVisible(card.$.editButton));
    assertTrue(isVisible(card.$.deleteButton));
  });

  test('Copy username', async function() {
    const password = createPasswordEntry({url: 'test.com', username: 'vik'});

    const card = document.createElement('password-details-card');
    card.password = password;
    document.body.appendChild(card);
    await flushTasks();

    assertTrue(isVisible(card.$.copyUsernameButton));
    assertFalse(card.$.toast.open);

    card.$.copyUsernameButton.click();

    assertTrue(card.$.toast.open);
    assertEquals(
        loadTimeData.getString('usernameCopiedToClipboard'),
        card.$.toast.textContent!.trim());
  });

  test('Copy password', async function() {
    const password = createPasswordEntry(
        {id: 1, url: 'test.com', username: 'vik', password: 'password69'});

    const card = document.createElement('password-details-card');
    card.password = password;
    document.body.appendChild(card);
    await flushTasks();

    assertTrue(isVisible(card.$.copyPasswordButton));
    assertFalse(card.$.toast.open);

    card.$.copyPasswordButton.click();
    const {id, reason} =
        await passwordManager.whenCalled('requestPlaintextPassword');
    assertEquals(password.id, id);
    assertEquals(chrome.passwordsPrivate.PlaintextReason.COPY, reason);

    await flushTasks();
    assertTrue(card.$.toast.open);
    assertEquals(
        loadTimeData.getString('passwordCopiedToClipboard'),
        card.$.toast.textContent!.trim());
  });

  test('Links properly displayed', async function() {
    const password = createPasswordEntry(
        {url: 'test.com', username: 'vik', password: 'password69'});
    password.affiliatedDomains = [
      {name: 'test.com', url: 'https://test.com/'},
      {name: 'Test App', url: 'https://m.test.com/'},
    ];

    const card = document.createElement('password-details-card');
    card.password = password;
    document.body.appendChild(card);
    await flushTasks();

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

    const card = document.createElement('password-details-card');
    card.password = password;
    document.body.appendChild(card);
    await flushTasks();

    assertEquals(
        loadTimeData.getString('showPassword'),
        card.$.showPasswordButton.title);
    assertEquals('password', card.$.passwordValue.type);
    assertTrue(card.$.showPasswordButton.hasAttribute('class'));
    assertEquals(
        'icon-visibility', card.$.showPasswordButton.getAttribute('class'));

    card.$.showPasswordButton.click();

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
    password.affiliatedDomains = [{name: 'test.com', url: 'https://test.com/'}];

    const card = document.createElement('password-details-card');
    card.password = password;
    document.body.appendChild(card);
    await flushTasks();

    card.$.editButton.click();
    await eventToPromise('cr-dialog-open', card);
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

    const card = document.createElement('password-details-card');
    card.password = password;
    document.body.appendChild(card);
    await flushTasks();

    assertTrue(isVisible(card.$.deleteButton));

    card.$.deleteButton.click();

    const params = await passwordManager.whenCalled('removeSavedPassword');
    assertEquals(params.id, password.id);
    assertEquals(params.fromStores, password.storedIn);
  });

  test('short note is shown fully', async function() {
    const password = createPasswordEntry({
      id: 1,
      url: 'test.com',
      username: 'vik',
      note: 'This is just a short note. It is cold out there.',
    });

    const card = document.createElement('password-details-card');
    card.password = password;
    document.body.appendChild(card);
    await flushTasks();

    assertEquals(password.note, card.$.noteValue.textContent!.trim());
    assertTrue(card.$.showMore.hidden);
  });

  test('long note is shown fully', async function() {
    const password = createPasswordEntry({
      id: 1,
      url: 'test.com',
      username: 'vik',
      note:
          'It is a long established fact that a reader will be distracted by ' +
          'the readable content of a page when looking at its layout. The ' +
          'point of using Lorem Ipsum is that it has a more-or-less normal ' +
          'distribution of letters, as opposed to using \'Content here, ' +
          'content here\', making it look like readable English.',
    });

    const card = document.createElement('password-details-card');
    card.password = password;
    document.body.appendChild(card);
    await flushTasks();

    assertEquals(password.note, card.$.noteValue.textContent!.trim());
    assertTrue(card.$.noteValue.hasAttribute('limit-note'));
    assertFalse(card.$.showMore.hidden);

    // Open note fully
    card.$.showMore.click();
    assertFalse(card.$.noteValue.hasAttribute('limit-note'));
  });
});
