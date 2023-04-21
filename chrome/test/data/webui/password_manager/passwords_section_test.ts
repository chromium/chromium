// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {AddPasswordDialogElement, AuthTimedOutDialogElement, Page, PasswordListItemElement, PasswordManagerImpl, PasswordsSectionElement, PasswordViewPageInteractions, PluralStringProxyImpl, Router, SyncBrowserProxyImpl, UrlParam} from 'chrome://password-manager/password_manager.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {createAffiliatedDomain, createCredentialGroup, createPasswordEntry, makePasswordManagerPrefs} from './test_util.js';

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
  let syncProxy: TestSyncBrowserProxy;

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
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);
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
    assertEquals(
        PasswordViewPageInteractions.CREDENTIAL_ROW_CLICKED,
        await passwordManager.whenCalled('recordPasswordViewInteraction'));
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
      createAffiliatedDomain('foo.de'),
      createAffiliatedDomain('m.foo.com'),
    ];
    passwordManager.data.groups[1]!.entries[0]!.affiliatedDomains = [
      createAffiliatedDomain('bar.uk'),
      createAffiliatedDomain('m.bar.com'),
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

  test('auth timed out dialog is shown', async function() {
    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    document.body.appendChild(section);
    await flushTasks();

    window.dispatchEvent(new CustomEvent('auth-timed-out', {
      bubbles: true,
      composed: true,
    }));
    await eventToPromise('cr-dialog-open', section);

    const addDialog =
        section.shadowRoot!.querySelector<AuthTimedOutDialogElement>(
            'auth-timed-out-dialog');
    assertTrue(!!addDialog);
    assertTrue(addDialog.$.dialog.open);
  });

  test('move passwords label shown', async function() {
    passwordManager.data.isOptedInAccountStorage = true;
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [createPasswordEntry(
          {username: 'user', id: 0, inProfileStore: true})],
    })];
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const section = await createPasswordsSection();

    assertFalse(section.$.movePasswords.hidden);

    // Assert that password section subscribed as a listener to opt in state and
    // opt out from account storage.
    assertTrue(!!passwordManager.listeners.accountStorageOptInStateListener);
    passwordManager.data.isOptedInAccountStorage = false;
    passwordManager.listeners.accountStorageOptInStateListener(false);
    await flushTasks();

    // Now move passwords option is hidden.
    assertTrue(section.$.movePasswords.hidden);
  });

  test('move passwords label hidden if no passwords to move', async function() {
    passwordManager.data.isOptedInAccountStorage = true;
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [createPasswordEntry(
          {username: 'user', id: 0, inAccountStore: true})],
    })];
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const section = await createPasswordsSection();

    assertTrue(section.$.movePasswords.hidden);

    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'test.com',
        credentials: [createPasswordEntry(
            {username: 'user', id: 0, inAccountStore: true})],
      }),
      createCredentialGroup({
        name: 'test.org',
        credentials: [createPasswordEntry(
            {username: 'user', id: 1, inProfileStore: true})],
      }),
    ];
    // Assert that password section listens to passwords update and invoke
    // an update.
    assertTrue(!!passwordManager.listeners.savedPasswordListChangedListener);
    passwordManager.listeners.savedPasswordListChangedListener([]);
    await flushTasks();

    // Now move passwords option is visible.
    assertFalse(section.$.movePasswords.hidden);
  });

  test('import passwords label shown', async function() {
    const section = await createPasswordsSection();

    assertFalse(section.$.importPasswords.hidden);

    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'test.com',
        credentials: [createPasswordEntry(
            {username: 'user', id: 0, inAccountStore: true})],
      }),
    ];
    // Assert that password section listens to passwords update and invoke
    // an update.
    assertTrue(!!passwordManager.listeners.savedPasswordListChangedListener);
    passwordManager.listeners.savedPasswordListChangedListener([]);
    await flushTasks();

    // Now import passwords option is hidden.
    assertTrue(section.$.importPasswords.hidden);
  });

  test('add button hidden when pref disabled', async function() {
    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    section.prefs = makePasswordManagerPrefs();
    section.prefs.credentials_enable_service.value = false;
    section.prefs.credentials_enable_service.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    document.body.appendChild(section);
    await flushTasks();

    assertFalse(isVisible(section.$.addPasswordButton));
  });

  test('import hidden when policy disabled', async function() {
    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    section.prefs = makePasswordManagerPrefs();
    section.prefs.credentials_enable_service.value = false;
    section.prefs.credentials_enable_service.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    document.body.appendChild(section);
    await flushTasks();

    assertFalse(isVisible(section.$.importPasswords));
  });

  test('clicking move passwords opens move passwords dialog', async function() {
    passwordManager.data.isOptedInAccountStorage = true;
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [createPasswordEntry({
        username: 'user',
        id: 0,
        inProfileStore: true,
        affiliatedDomains: [createAffiliatedDomain('test.com')],
      })],
    })];
    passwordManager.setRequestCredentialsDetailsResponse(
        passwordManager.data.groups[0]!.entries);
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const section = await createPasswordsSection();

    assertTrue(isVisible(section.$.movePasswords));

    section.$.movePasswords.click();
    await flushTasks();

    const movdeDialog =
        section.shadowRoot!.querySelector('move-passwords-dialog');
    assertTrue(!!movdeDialog);
    assertTrue(movdeDialog.$.dialog.open);
  });

  test('description is hidden during search', async function() {
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'foo.com',
      }),
      createCredentialGroup({
        name: 'bar.com',
      }),
    ];

    const section = await createPasswordsSection();

    assertTrue(isVisible(section.$.descriptionLabel));

    const query = new URLSearchParams();
    query.set(UrlParam.SEARCH_TERM, 'bar');
    Router.getInstance().updateRouterParams(query);
    await flushTasks();

    assertFalse(isVisible(section.$.descriptionLabel));
  });

  test('Move passwords is hidden during search', async function() {
    passwordManager.data.isOptedInAccountStorage = true;
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [createPasswordEntry({
        username: 'user',
        id: 0,
        inProfileStore: true,
        affiliatedDomains: [createAffiliatedDomain('test.com')],
      })],
    })];
    passwordManager.setRequestCredentialsDetailsResponse(
        passwordManager.data.groups[0]!.entries);
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const section = await createPasswordsSection();

    assertTrue(isVisible(section.$.movePasswords));

    const query = new URLSearchParams();
    query.set(UrlParam.SEARCH_TERM, 'bar');
    Router.getInstance().updateRouterParams(query);
    await flushTasks();

    assertFalse(isVisible(section.$.movePasswords));
  });

  test('No password is shown when no matches', async function() {
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'foo.com',
      }),
      createCredentialGroup({
        name: 'bar.com',
      }),
    ];

    const section = await createPasswordsSection();

    assertFalse(isVisible(section.$.noPasswordsFound));

    const query = new URLSearchParams();
    query.set(UrlParam.SEARCH_TERM, 'bar');
    Router.getInstance().updateRouterParams(query);
    await flushTasks();
    assertFalse(isVisible(section.$.noPasswordsFound));

    query.set(UrlParam.SEARCH_TERM, 'bar.org');
    Router.getInstance().updateRouterParams(query);
    await flushTasks();
    assertTrue(isVisible(section.$.noPasswordsFound));
  });

  test('No password is hidden when there are no passwords', async function() {
    const section = await createPasswordsSection();

    assertFalse(isVisible(section.$.noPasswordsFound));

    const query = new URLSearchParams();
    query.set(UrlParam.SEARCH_TERM, 'test');
    Router.getInstance().updateRouterParams(query);
    await flushTasks();

    assertFalse(isVisible(section.$.noPasswordsFound));
  });
});
