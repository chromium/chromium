// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createCredentialGroup, createPasswordEntry} from './test_util.js';

suite('PasswordManagerAppTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    return flushTasks();
  });

  test('Clicking enter during search focuses first result', async function() {
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'abc.com',
        credentials: [
          createPasswordEntry({id: 0, username: 'test1'}),
        ],
      }),
      createCredentialGroup({
        name: 'bbc.org',
        credentials: [
          createPasswordEntry({id: 1, username: 'test1'}),
        ],
      }),
    ];

    const app = document.createElement('password-manager-app');
    document.body.appendChild(app);
    app.setNarrowForTesting(false);
    await flushTasks();

    assertEquals(app.shadowRoot!.activeElement, app.$.toolbar);

    app.$.toolbar.searchField.setValue('.org');
    keyDownOn(app.$.toolbar.searchField, 0, [], 'Enter');

    const firstMatch =
        app.$.passwords.shadowRoot!.querySelector('password-list-item');
    assertTrue(!!firstMatch);
    assertEquals(app.shadowRoot!.activeElement, app.$.passwords);
    assertEquals(app.$.passwords.shadowRoot!.activeElement, firstMatch);
  });
});
