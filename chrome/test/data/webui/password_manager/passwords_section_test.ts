// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl, PasswordsSectionElement} from 'chrome://password-manager/password_manager.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createPasswordEntry} from './test_util.js';

/**
 * @param subsection The passwords subsection element that will be checked.
 * @param expectedPasswords The expected passwords in this subsection.
 */
async function validatePasswordsSubsection(
    list: IronListElement,
    expectedPasswords: chrome.passwordsPrivate.PasswordUiEntry[]) {
  assertDeepEquals(expectedPasswords, list.items);

  const listItemElements = list.querySelectorAll('password-list-item');
  for (let index = 0; index < expectedPasswords.length; ++index) {
    const expectedPassword = expectedPasswords[index]!;
    const listItemElement = listItemElements[index];

    assertTrue(!!listItemElement);
    assertEquals(
        expectedPassword.urls.shown,
        listItemElement.$.displayName.textContent!.trim());
  }
}

suite('PasswordsSectionTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    return flushTasks();
  });

  test('password section listens to updates', async function() {
    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    document.body.appendChild(section);
    await flushTasks();

    // PasswordsList is hidden as there are no passwords.
    assertFalse(isVisible(section.$.passwordsList));

    // Add passwords
    const passwordList = [
      createPasswordEntry({url: 'test.com', username: 'user', id: 0}),
      createPasswordEntry({url: 'test2.com', username: 'user', id: 1}),
    ];
    assertTrue(!!passwordManager.listeners.savedPasswordListChangedListener);
    passwordManager.listeners.savedPasswordListChangedListener!(passwordList);
    await flushTasks();

    validatePasswordsSubsection(section.$.passwordsList, passwordList);
  });
});
