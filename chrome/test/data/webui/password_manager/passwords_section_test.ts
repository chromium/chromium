// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {AddPasswordDialogElement, Page, PasswordListItemElement, PasswordManagerImpl, PasswordsSectionElement, Router, UrlParam} from 'chrome://password-manager/password_manager.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createCredentialGroup, createPasswordEntry} from './test_util.js';

/**
 * @param subsection The passwords subsection element that will be checked.
 * @param expectedPasswords The expected passwords in this subsection.
 */
function validatePasswordsSubsection(
    section: PasswordsSectionElement,
    expectedGroups: chrome.passwordsPrivate.CredentialGroup[],
    searchTerm: string) {
  const listItemElements =
      section.shadowRoot!.querySelectorAll('password-list-item');
  assertEquals(listItemElements.length, expectedGroups.length);

  for (let index = 0; index < expectedGroups.length; ++index) {
    const expectedGroup = expectedGroups[index]!;
    const listItemElement = listItemElements[index];

    const matchingUsername =
        expectedGroup.entries.find(cred => cred.username.includes(searchTerm))
            ?.username;
    const matchingDomain =
        expectedGroup.entries
            .find(
                cred => cred.affiliatedDomains?.some(
                    domain => domain.name.includes(searchTerm)))
            ?.affiliatedDomains
            ?.find(domain => domain.name.includes(searchTerm))
            ?.name;

    assertTrue(!!listItemElement);
    if (!searchTerm || expectedGroup.name.includes(searchTerm)) {
      assertEquals(
          expectedGroup.name,
          listItemElement.$.displayedName.textContent!.trim());
    } else if (matchingUsername) {
      assertEquals(
          expectedGroup.name + ' • ' + matchingUsername,
          listItemElement.$.displayedName.textContent!.trim());
    } else {
      assertEquals(
          expectedGroup.name + ' • ' + matchingDomain,
          listItemElement.$.displayedName.textContent!.trim());
    }
  }
}

suite('PasswordsSectionTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let pluralString: TestPluralStringProxy;

  async function createPasswordsSection(): Promise<PasswordsSectionElement> {
    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    await flushTasks();

    return section;
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);
    Router.getInstance().updateRouterParams(new URLSearchParams());
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

    const section = await createPasswordsSection();

    validatePasswordsSubsection(section, passwordManager.data.groups, '');
  });

  test('passwords list is hidden if nothing to show', async function() {
    const section = await createPasswordsSection();

    // PasswordsList is hidden as there are no passwords.
    assertFalse(isVisible(section.$.passwordsList));
  });

  test('clicking group navigates to details page', async function() {
    Router.getInstance().navigateTo(Page.PASSWORDS);
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 0}),
        createPasswordEntry({id: 1}),
      ],
    })];
    passwordManager.setRequestCredentialsDetailsResponse(
        passwordManager.data.groups[0]!.entries.slice());

    const section = await createPasswordsSection();

    const listEntry =
        section.shadowRoot!.querySelector<HTMLElement>('password-list-item');
    assertTrue(!!listEntry);
    listEntry.click();
    assertArrayEquals(
        [0, 1], await passwordManager.whenCalled('requestCredentialsDetails'));

    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
  });

  test('clicking group does nothing when auth fails', async function() {
    Router.getInstance().navigateTo(Page.PASSWORDS);
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 0}),
        createPasswordEntry({id: 1}),
      ],
    })];

    const section = await createPasswordsSection();

    const listEntry =
        section.shadowRoot!.querySelector<HTMLElement>('password-list-item');
    assertTrue(!!listEntry);
    listEntry.click();
    // Without setRequestCredentialsDetailsResponse auth is considered failed.
    assertArrayEquals(
        [0, 1], await passwordManager.whenCalled('requestCredentialsDetails'));

    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
  });

  test('number of accounts is shown', async function() {
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'test.com',
        credentials: [createPasswordEntry({username: 'user', id: 0})],
      }),
      createCredentialGroup({
        name: 'test2.com',
        credentials: [
          createPasswordEntry({username: 'user1', id: 1}),
          createPasswordEntry({username: 'user2', id: 2}),
        ],
      }),
    ];
    pluralString.text = '2 accounts';

    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    await pluralString.whenCalled('getPluralString');
    await flushTasks();

    const listEntries =
        section.shadowRoot!.querySelectorAll<PasswordListItemElement>(
            'password-list-item');
    assertEquals(2, listEntries.length);

    // Since there is only 1 PasswordUIEntry in the group number of accounts is
    // hidden.
    assertTrue(listEntries[0]!.$.numberOfAccounts.hidden);
    // For group with 2 PasswordUIEntries |numberOfAccounts| is visible.
    assertFalse(listEntries[1]!.$.numberOfAccounts.hidden);
    assertEquals(
        pluralString.text,
        listEntries[1]!.$.numberOfAccounts.textContent!.trim());
  });

  test('search by group name', async function() {
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'foo.com',
      }),
      createCredentialGroup({
        name: 'bar.com',
      }),
    ];

    const section = await createPasswordsSection();

    validatePasswordsSubsection(section, passwordManager.data.groups, '');

    const query = new URLSearchParams();
    query.set(UrlParam.SEARCH_TERM, 'bar');
    Router.getInstance().updateRouterParams(query);
    await flushTasks();

    validatePasswordsSubsection(
        section, passwordManager.data.groups.slice(1), 'bar');
  });

  test('search by username', async function() {
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'foo.com',
        credentials: [createPasswordEntry({username: 'qwerty', id: 0})],
      }),
      createCredentialGroup({
        name: 'bar.com',
        credentials: [createPasswordEntry({username: 'username', id: 1})],
      }),
    ];

    const section = await createPasswordsSection();

    validatePasswordsSubsection(section, passwordManager.data.groups, '');

    const query = new URLSearchParams();
    query.set(UrlParam.SEARCH_TERM, 'ert');
    Router.getInstance().updateRouterParams(query);
    await flushTasks();

    validatePasswordsSubsection(
        section, passwordManager.data.groups.slice(0, 1), 'ert');
  });

  test('search by domain', async function() {
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'foo.com',
        credentials: [createPasswordEntry({username: 'qwerty', id: 0})],
      }),
      createCredentialGroup({
        name: 'bar.com',
        credentials: [createPasswordEntry({username: 'username', id: 1})],
      }),
    ];
    passwordManager.data.groups[0]!.entries[0]!.affiliatedDomains = [
      {name: 'foo.de', url: 'https://foo.de/'},
      {name: 'Foo App', url: 'https://m.foo.com/'},
    ];
    passwordManager.data.groups[1]!.entries[0]!.affiliatedDomains = [
      {name: 'bar.uk', url: 'https://bar.uk/'},
      {name: 'Bar App', url: 'https://m.bar.com/'},
    ];

    const section = await createPasswordsSection();

    validatePasswordsSubsection(section, passwordManager.data.groups, '');

    const query = new URLSearchParams();
    query.set(UrlParam.SEARCH_TERM, 'bar.uk');
    Router.getInstance().updateRouterParams(query);
    await flushTasks();

    validatePasswordsSubsection(
        section, passwordManager.data.groups.slice(1), 'bar.uk');
  });

  test('clicking add button opens an add password dialog', async function() {
    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    document.body.appendChild(section);
    await flushTasks();

    section.$.addPasswordButton.click();
    await eventToPromise('cr-dialog-open', section);

    const addDialog =
        section.shadowRoot!.querySelector<AddPasswordDialogElement>(
            'add-password-dialog');
    assertTrue(!!addDialog);
    assertTrue(addDialog.$.dialog.open);
  });

  test('search calls plural string proxy to announce result', async function() {
    passwordManager.data.groups = [
      createCredentialGroup({name: 'foo.com'}),
      createCredentialGroup({name: 'bar.com'}),
    ];

    await createPasswordsSection();

    pluralString.reset();
    const query = new URLSearchParams();
    query.set(UrlParam.SEARCH_TERM, 'Foo');
    Router.getInstance().updateRouterParams(query);
    const params = await pluralString.whenCalled('getPluralString');
    await flushTasks();

    assertEquals('searchResults', params.messageName);
    assertEquals(1, params.itemCount);
  });
});
