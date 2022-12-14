// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {CrInputElement, Page, PasswordDetailsCardElement, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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
    const note: CrInputElement|null =
        card.shadowRoot!.querySelector('#noteValue');
    assertTrue(!!note);
    assertEquals(loadTimeData.getString('emptyNote'), note.value);
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
    const note: CrInputElement|null =
        card.shadowRoot!.querySelector('#noteValue');
    assertFalse(!!note);
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
});
