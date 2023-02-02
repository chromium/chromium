// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createPasswordEntry} from './test_util.js';

suite('EditPasswordDialogTest', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    return flushTasks();
  });

  test('password displayed correctly', async function() {
    const password =
        createPasswordEntry({id: 0, username: 'user1', password: 'sTr0nGp@@s'});
    password.affiliatedDomains = [
      {name: 'test.com', url: 'https://test.com/'},
      {name: 'Test App', url: 'https://m.test.com/'},
    ];

    const dialog = document.createElement('edit-password-dialog');
    dialog.password = password;
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
    password.affiliatedDomains = [{name: 'test.com', url: 'https://test.com/'}];
    const dialog = document.createElement('edit-password-dialog');
    dialog.password = password;
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
});
