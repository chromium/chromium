// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {Page, PasswordManagerImpl, PasswordsSectionElement, Router} from 'chrome://password-manager/password_manager.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createCredentialGroup, createPasswordEntry} from './test_util.js';

/**
 * @param subsection The passwords subsection element that will be checked.
 * @param expectedPasswords The expected passwords in this subsection.
 */
function validatePasswordsSubsection(
    list: IronListElement,
    expectedGroups: chrome.passwordsPrivate.CredentialGroup[]) {
  assertDeepEquals(expectedGroups, list.items);

  const listItemElements = list.querySelectorAll('password-list-item');
  assertEquals(listItemElements.length, expectedGroups.length);

  for (let index = 0; index < expectedGroups.length; ++index) {
    const expectedGroup = expectedGroups[index]!;
    const listItemElement = listItemElements[index];

    assertTrue(!!listItemElement);
    assertEquals(
        expectedGroup.name, listItemElement.$.displayName.textContent!.trim());
  }
}

suite('PasswordsSectionTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    return flushTasks();
  });

  test('groups shown correctly', async function() {
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'test.com',
        credentials: [createPasswordEntry({username: 'user', id: 0})],
      }),
      createCredentialGroup({
        name: 'test2.com',
        credentials: [createPasswordEntry({username: 'user', id: 1})],
      }),
    ];

    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    await flushTasks();

    validatePasswordsSubsection(
        section.$.passwordsList, passwordManager.data.groups);
  });

  test('passwords list is hidden if nothing to show', async function() {
    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    await flushTasks();

    // PasswordsList is hidden as there are no passwords.
    assertFalse(isVisible(section.$.passwordsList));
  });

  test('clicking group navigates to details page', async function() {
    passwordManager.data.groups = [createCredentialGroup({name: 'test.com'})];

    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    await flushTasks();

    const listEntry =
        section.shadowRoot!.querySelector<HTMLElement>('password-list-item');
    assertTrue(!!listEntry);
    listEntry.click();

    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
  });
});
