// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {Page, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
    assertEquals(password.urls.shown, card.$.linkValue.textContent!.trim());
    assertEquals(password.urls.link, card.$.linkValue.getAttribute('href'));
    assertEquals(loadTimeData.getString('emptyNote'), card.$.noteValue.value);
    assertTrue(isVisible(card.$.copyUsernameButton));
    assertTrue(isVisible(card.$.showPasswordButton));
    assertTrue(isVisible(card.$.copyPasswordButton));
    assertTrue(isVisible(card.$.editButton));
    assertTrue(isVisible(card.$.deleteButton));
  });
});
