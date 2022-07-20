// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Password Settings tests. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {isChromeOS, isLacros, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PasswordsSectionElement} from 'chrome://settings/lazy_load.js';
import {buildRouter, HatsBrowserProxyImpl, MultiStorePasswordUiEntry, PasswordCheckReferrer, PasswordManagerImpl, Router, routes, SettingsPluralStringProxyImpl,StatusAction, TrustedVaultBannerState, TrustSafetyInteraction} from 'chrome://settings/settings.js';
import {SettingsRoutes} from 'chrome://settings/settings_routes.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {eventToPromise, flushTasks, isVisible} from 'chrome://webui-test/test_util.js';

import {createExceptionEntry, createPasswordEntry, makeCompromisedCredential, makePasswordCheckStatus, PasswordSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {getSyncAllPrefs, simulateStoredAccounts, simulateSyncStatus} from './sync_test_util.js';
import {TestHatsBrowserProxy} from './test_hats_browser_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

const PasswordCheckState = chrome.passwordsPrivate.PasswordCheckState;

/**
 * Helper method that validates a that elements in the password list match
 * the expected data.
 * @param passwordsSection The passwords section element that will be checked.
 * @param expectedPasswords The expected data.
 */
function validateMultiStorePasswordList(
    passwordsSection: PasswordsSectionElement,
    expectedPasswords: MultiStorePasswordUiEntry[]) {
  const passwordList = passwordsSection.$.passwordList;
  if (passwordList.filter) {
    // `passwordList.items` will always contain all items, even when there is a
    // filter to be applied. Thus apply `passwordList.filter` to obtain the list
    // of items that are user visible.
    assertDeepEquals(
        expectedPasswords,
        passwordList.items!.filter(
            passwordList.filter as (item: MultiStorePasswordUiEntry) =>
                boolean));
  } else {
    assertDeepEquals(expectedPasswords, passwordList.items);
  }

  const listItems =
      passwordsSection.shadowRoot!.querySelectorAll('password-list-item');
  for (let index = 0; index < expectedPasswords.length; ++index) {
    const expected = expectedPasswords[index]!;
    const listItem = listItems[index]!;
    assertTrue(!!listItem);
    assertEquals(expected.urls.shown, listItem.$.originUrl.textContent!.trim());
    assertEquals(expected.urls.link, listItem.$.originUrl.href);
    assertEquals(expected.username, listItem.$.username.value);
  }
}

/**
 * Convenience version of validateMultiStorePasswordList() for when store
 * duplicates don't exist.
 * @param passwordsSection The passwords section element that will be checked.
 * @param passwordList The expected data.
 */
function validatePasswordList(
    passwordsSection: PasswordsSectionElement,
    passwordList: chrome.passwordsPrivate.PasswordUiEntry[]) {
  validateMultiStorePasswordList(
      passwordsSection,
      passwordList.map(entry => new MultiStorePasswordUiEntry(entry)));
}

/**
 * Helper method that validates a that elements in the exception list match
 * the expected data.
 * @param nodes The nodes that will be checked.
 * @param exceptionList The expected data.
 */
function validateExceptionList(
    nodes: NodeListOf<HTMLElement>,
    exceptionList: chrome.passwordsPrivate.ExceptionEntry[]) {
  assertEquals(exceptionList.length, nodes.length);
  for (let index = 0; index < exceptionList.length; ++index) {
    const node = nodes[index]!;
    const exception = exceptionList[index]!;
    assertEquals(
        exception.urls.shown,
        node.querySelector<HTMLElement>('#exception')!.textContent!.trim());
    assertEquals(
        exception.urls.link.toLowerCase(),
        node.querySelector<HTMLAnchorElement>('#exception')!.href);
  }
}

/**
 * Returns all children of an element that has children added by a dom-repeat.
 */
function getDomRepeatChildren(element: HTMLElement): NodeListOf<HTMLElement> {
  const nodes = element.querySelectorAll<HTMLElement>('.list-item:not([id])');
  return nodes;
}

/**
 * Extracts the first password-list-item in the a password-section element.
 */
function getFirstPasswordListItem(passwordsSection: HTMLElement) {
  return passwordsSection.shadowRoot!.querySelector('password-list-item')!;
}

/**
 * Helper method used to test for a url in a list of passwords.
 * @param url The URL that is being searched for.
 */
function listContainsUrl(
    passwordList:
        (MultiStorePasswordUiEntry[]|chrome.passwordsPrivate.PasswordUiEntry[]),
    url: string): boolean {
  return passwordList.some(item => item.urls.origin === url);
}

/**
 * Helper method used to test for a url in a list of passwords.
 * @param url The URL that is being searched for.
 */
function exceptionsListContainsUrl(
    exceptionList: chrome.passwordsPrivate.ExceptionEntry[],
    url: string): boolean {
  return exceptionList.some(
      item => (item.urls as unknown as {originUrl: string}).originUrl === url);
}

/**
 * Helper function to check password visibility when open password-edit-dialog.
 */
async function openPasswordEditDialogHelper(
    passwordManager: TestPasswordManagerProxy,
    elementFactory: PasswordSectionElementFactory) {
  const PASSWORD = 'p4ssw0rd';
  const passwordList = [
    createPasswordEntry({username: 'user0', id: 0}),
  ];
  passwordManager.setPlaintextPassword(PASSWORD);

  const passwordsSection =
      elementFactory.createPasswordsSection(passwordManager, passwordList, []);

  const passwordListItem = getFirstPasswordListItem(passwordsSection);
  passwordListItem.shadowRoot!
      .querySelector<HTMLElement>('#showPasswordButton')!.click();
  flush();
  await passwordManager.whenCalled('requestPlaintextPassword');
  await flushTasks();
  passwordManager.resetResolver('requestPlaintextPassword');

  assertEquals(
      'text',
      passwordListItem.shadowRoot!.querySelector<HTMLInputElement>(
                                      '#password')!.type);
  assertFalse(
      passwordListItem.shadowRoot!.querySelector<HTMLInputElement>(
                                      '#password')!.disabled);
  assertTrue(passwordListItem.shadowRoot!
                 .querySelector<HTMLElement>('#showPasswordButton')!.classList
                 .contains('icon-visibility-off'));

  // Open Edit Dialog.
  passwordListItem.$.moreActionsButton.click();
  passwordsSection.$.passwordsListHandler.$.menuEditPassword.click();
  flush();

  await passwordManager.whenCalled('requestPlaintextPassword');
  await flushTasks();
  passwordManager.resetResolver('requestPlaintextPassword');


  assertEquals(
      'password',
      passwordListItem.shadowRoot!.querySelector<HTMLInputElement>(
                                      '#password')!.type);
  assertTrue(
      passwordListItem.shadowRoot!.querySelector<HTMLInputElement>(
                                      '#password')!.disabled);
  assertTrue(
      passwordListItem.shadowRoot!
          .querySelector<HTMLElement>(
              '#showPasswordButton')!.classList.contains('icon-visibility'));

  // Verify that edit dialog password is hidden.
  const passwordEditDialog =
      passwordsSection.$.passwordsListHandler.shadowRoot!.querySelector(
          'password-edit-dialog')!;
  const showPasswordButton =
      passwordEditDialog.shadowRoot!.querySelector<HTMLElement>(
          '#showPasswordButton')!;
  assertEquals('password', passwordEditDialog.$.passwordInput.type);
  assertTrue(showPasswordButton.classList.contains('icon-visibility'));

  showPasswordButton.click();
  flush();

  assertEquals('text', passwordEditDialog.$.passwordInput.type);
  assertTrue(showPasswordButton.classList.contains('icon-visibility-off'));

  // Close the dialog, verify that the list item password remains hidden.
  // Note that the password only gets hidden in the on-close handler, thus we
  // need to await this event first.
  passwordManager.setChangeSavedPasswordResponse({deviceId: 1});
  passwordEditDialog.$.actionButton.click();
  await eventToPromise('close', passwordEditDialog);

  assertEquals('', passwordListItem.entry.password);
  assertEquals(
      'password',
      passwordListItem.shadowRoot!.querySelector<HTMLInputElement>(
                                      '#password')!.type);
  assertTrue(
      passwordListItem.shadowRoot!.querySelector<HTMLInputElement>(
                                      '#password')!.disabled);
  assertTrue(
      passwordListItem.shadowRoot!
          .querySelector<HTMLElement>(
              '#showPasswordButton')!.classList.contains('icon-visibility'));
}

/**
 * Simulates user who is eligible and opted-in for account storage. Should be
 * called after the PasswordsSection element is created.
 */
function simulateAccountStorageUser(passwordManager: TestPasswordManagerProxy) {
  simulateSyncStatus({signedIn: false, statusAction: StatusAction.NO_ACTION});
  simulateStoredAccounts([{email: 'john@gmail.com'}]);
  passwordManager.setIsOptedInForAccountStorageAndNotify(true);

  flush();
}

// TODO(crbug.com/1260310): Split into multiple test suits.
suite('PasswordsSection', function() {
  let passwordManager: TestPasswordManagerProxy;

  let elementFactory: PasswordSectionElementFactory;

  let pluralString: TestPluralStringProxy;

  let testHatsBrowserProxy: TestHatsBrowserProxy;

  setup(function() {
    document.body.innerHTML = '';
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    pluralString = new TestPluralStringProxy();
    SettingsPluralStringProxyImpl.setInstance(pluralString);
    testHatsBrowserProxy = new TestHatsBrowserProxy();
    HatsBrowserProxyImpl.setInstance(testHatsBrowserProxy);

    PasswordManagerImpl.setInstance(passwordManager);
    elementFactory = new PasswordSectionElementFactory(document);
    loadTimeData.overrideValues({
      enableAutomaticPasswordChangeInSettings: false,
      enablePasswordViewPage: false,
      unifiedPasswordManagerEnabled: false,
    });
  });

  test('testPasswordsExtensionIndicator', function() {
    // Initialize with dummy prefs.
    const element = document.createElement('passwords-section');
    element.prefs = {
      credentials_enable_service: {},
    };
    document.body.appendChild(element);

    assertFalse(
        !!element.shadowRoot!.querySelector('#passwordsExtensionIndicator'));
    element.set('prefs.credentials_enable_service.extensionId', 'test-id');
    flush();

    assertTrue(
        !!element.shadowRoot!.querySelector('#passwordsExtensionIndicator'));
  });

  test('verifyNoSavedPasswords', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);

    validatePasswordList(passwordsSection, []);

    assertFalse(passwordsSection.$.noPasswordsLabel.hidden);
    assertTrue(passwordsSection.$.savedPasswordsHeaders.hidden);
  });

  test('verifySavedPasswordEntries', function() {
    const passwordList = [
      createPasswordEntry({url: 'site1.com', username: 'luigi', id: 0}),
      createPasswordEntry({url: 'longwebsite.com', username: 'peach', id: 1}),
      createPasswordEntry({url: 'site2.com', username: 'mario', id: 2}),
      createPasswordEntry({url: 'site1.com', username: 'peach', id: 3}),
      createPasswordEntry({url: 'google.com', username: 'mario', id: 4}),
      createPasswordEntry({url: 'site2.com', username: 'luigi', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    // Assert that the data is passed into the iron list. If this fails,
    // then other expectations will also fail.
    assertDeepEquals(
        passwordList.map(entry => new MultiStorePasswordUiEntry(entry)),
        passwordsSection.$.passwordList.items);

    validatePasswordList(passwordsSection, passwordList);

    assertTrue(passwordsSection.$.noPasswordsLabel.hidden);
    assertFalse(passwordsSection.$.savedPasswordsHeaders.hidden);
  });

  // Test verifies that removing a password will update the elements.
  test('verifyPasswordListRemove', function() {
    const passwordList = [
      createPasswordEntry(
          {url: 'anotherwebsite.com', username: 'luigi', id: 0}),
      createPasswordEntry({url: 'longwebsite.com', username: 'peach', id: 1}),
      createPasswordEntry({url: 'website.com', username: 'mario', id: 2}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validatePasswordList(passwordsSection, passwordList);
    // Simulate 'longwebsite.com' being removed from the list.
    passwordList.splice(1, 1);
    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        (passwordList);
    flush();

    assertFalse(
        listContainsUrl(passwordsSection.savedPasswords, 'longwebsite.com'));
    assertFalse(listContainsUrl(passwordList, 'longwebsite.com'));

    validatePasswordList(passwordsSection, passwordList);
  });

  // Regression test for crbug.com/1110290.
  // Test verifies that if the password list is updated, all the plaintext
  // passwords are hidden.
  test('updatingPasswordListHidesPlaintextPasswords', async function() {
    const PASSWORD = 'pwd';
    const passwordList = [
      createPasswordEntry({url: 'goo.gl', username: 'user0', id: 0}),
      createPasswordEntry({url: 'goo.gl', username: 'user1', id: 1}),
    ];
    passwordManager.setPlaintextPassword(PASSWORD);

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    const passwordListItems =
        passwordsSection.shadowRoot!.querySelectorAll('password-list-item');
    assertEquals(2, passwordListItems.length);

    passwordListItems[0]!.shadowRoot!
        .querySelector<HTMLElement>('#showPasswordButton')!.click();
    flush();
    await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();
    passwordManager.resetResolver('requestPlaintextPassword');

    passwordListItems[1]!.shadowRoot!
        .querySelector<HTMLElement>('#showPasswordButton')!.click();
    await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();

    assertEquals(
        'text',
        passwordListItems[0]!.shadowRoot!
            .querySelector<HTMLInputElement>('#password')!.type);
    assertEquals(
        'text',
        passwordListItems[1]!.shadowRoot!
            .querySelector<HTMLInputElement>('#password')!.type);

    // Remove first row and verify that the remaining password is hidden.
    passwordList.splice(0, 1);
    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        (passwordList);
    flush();
    assertEquals('', getFirstPasswordListItem(passwordsSection).entry.password);
    assertEquals(
        'password',
        getFirstPasswordListItem(passwordsSection)
            .shadowRoot!.querySelector<HTMLInputElement>('#password')!.type);
    assertEquals(
        'user1', getFirstPasswordListItem(passwordsSection).entry.username);
  });

  test('listItemEditDialogShowAndHideInterplay', async function() {
    await openPasswordEditDialogHelper(passwordManager, elementFactory);
  });

  // Test verifies that adding a password will update the elements.
  test('verifyPasswordListAdd', function() {
    const passwordList = [
      createPasswordEntry(
          {url: 'anotherwebsite.com', username: 'luigi', id: 0}),
      createPasswordEntry({url: 'longwebsite.com', username: 'peach', id: 1}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validatePasswordList(passwordsSection, passwordList);
    // Simulate 'website.com' being added to the list.
    passwordList.unshift(
        createPasswordEntry({url: 'website.com', username: 'mario', id: 2}));
    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        (passwordList);
    flush();

    validatePasswordList(passwordsSection, passwordList);
  });

  // Test verifies that removing one out of two passwords for the same website
  // will update the elements.
  test('verifyPasswordListRemoveSameWebsite', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);

    // Set-up initial list.
    let passwordList = [
      createPasswordEntry({url: 'website.com', username: 'mario', id: 0}),
      createPasswordEntry({url: 'website.com', username: 'luigi', id: 1}),
    ];

    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        (passwordList);
    flush();
    validatePasswordList(passwordsSection, passwordList);

    // Simulate '(website.com, mario)' being removed from the list.
    passwordList.shift();
    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        (passwordList);
    flush();
    validatePasswordList(passwordsSection, passwordList);

    // Simulate '(website.com, luigi)' being removed from the list as well.
    passwordList = [];
    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        (passwordList);
    flush();
    validatePasswordList(passwordsSection, passwordList);
  });

  // Test verifies that pressing the 'remove' button will trigger a remove
  // event. Does not actually remove any passwords.
  test('verifyPasswordItemRemoveButton', async function() {
    const passwordList = [
      createPasswordEntry({url: 'one', username: 'six', id: 0}),
      createPasswordEntry({url: 'two', username: 'five', id: 1}),
      createPasswordEntry({url: 'three', username: 'four', id: 2}),
      createPasswordEntry({url: 'four', username: 'three', id: 3}),
      createPasswordEntry({url: 'five', username: 'two', id: 4}),
      createPasswordEntry({url: 'six', username: 'one', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    const firstNode = getFirstPasswordListItem(passwordsSection);
    assertTrue(!!firstNode);
    const firstPassword = passwordList[0]!;

    // Click the remove button on the first password.
    firstNode.$.moreActionsButton.click();
    passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();

    const {id} = await passwordManager.whenCalled('removeSavedPassword');
    // Verify that the expected value was passed to the proxy.
    assertEquals(firstPassword.id, id);
    assertEquals(
        passwordsSection.i18n('passwordDeleted'),
        passwordsSection.$.passwordsListHandler.$.removalNotification
            .textContent);
  });

  // Test verifies that 'Copy password' button is hidden for Federated
  // (passwordless) credentials. Does not test Copy button.
  test('verifyCopyAbsentForFederatedPasswordInMenu', function() {
    const passwordList = [
      createPasswordEntry({federationText: 'with chromium.org'}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    getFirstPasswordListItem(passwordsSection).$.moreActionsButton.click();
    flush();
    assertTrue(
        passwordsSection.$.passwordsListHandler.$.menuCopyPassword.hidden);
  });

  // Test verifies that 'Copy password' button is not hidden for common
  // credentials. Does not test Copy button.
  test('verifyCopyPresentInMenu', function() {
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'hey'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    getFirstPasswordListItem(passwordsSection).$.moreActionsButton.click();
    flush();
    assertFalse(
        passwordsSection.$.passwordsListHandler.$.menuCopyPassword.hidden);
  });

  // Test verifies that 'Edit' button is replaced to 'Details' for Federated
  // (passwordless) credentials. Does not test Details and Edit button.
  test('verifyEditReplacedToDetailsForFederatedPasswordInMenu', function() {
    const passwordList = [
      createPasswordEntry({federationText: 'with chromium.org'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    getFirstPasswordListItem(passwordsSection).$.moreActionsButton.click();
    flush();
    assertEquals(
        passwordsSection.i18n('passwordViewDetails'),
        passwordsSection.$.passwordsListHandler.$.menuEditPassword.textContent!
            .trim());
  });

  // Test verifies that 'Edit' button is replaced to 'Details' for Federated
  // (passwordless) credentials.
  // Does not test Details and Edit button.
  test('verifyDetailsForFederatedPasswordInMenu', function() {
    const passwordList = [
      createPasswordEntry({federationText: 'with chromium.org'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    getFirstPasswordListItem(passwordsSection).$.moreActionsButton.click();
    flush();
    assertEquals(
        passwordsSection.i18n('passwordViewDetails'),
        passwordsSection.$.passwordsListHandler.$.menuEditPassword.textContent!
            .trim());
  });

  // Test verifies that 'Edit' button is shown instead of 'Details' for
  // common credentials.
  // Does not test Details and Edit button.
  test('verifyEditButtonInMenu', function() {
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'hey'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    getFirstPasswordListItem(passwordsSection).$.moreActionsButton.click();
    flush();
    assertEquals(
        passwordsSection.i18n('editPassword'),
        passwordsSection.$.passwordsListHandler.$.menuEditPassword.textContent!
            .trim());
  });

  test('verifyFilterPasswords', function() {
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'SHOW', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'shower', id: 1}),
      createPasswordEntry({url: 'three.com/show', username: 'four', id: 2}),
      createPasswordEntry({url: 'four.com', username: 'three', id: 3}),
      createPasswordEntry({url: 'five.com', username: 'two', id: 4}),
      createPasswordEntry({url: 'six-show.com', username: 'one', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    passwordsSection.filter = 'SHow';
    flush();

    const expectedList = [
      createPasswordEntry({url: 'one.com', username: 'SHOW', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'shower', id: 1}),
      createPasswordEntry({url: 'three.com/show', username: 'four', id: 2}),
      createPasswordEntry({url: 'six-show.com', username: 'one', id: 5}),
    ];

    validatePasswordList(passwordsSection, expectedList);
  });

  test('verifyFilterPasswordsWithRemoval', function() {
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'SHOW', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'shower', id: 1}),
      createPasswordEntry({url: 'three.com/show', username: 'four', id: 2}),
      createPasswordEntry({url: 'four.com', username: 'three', id: 3}),
      createPasswordEntry({url: 'five.com', username: 'two', id: 4}),
      createPasswordEntry({url: 'six-show.com', username: 'one', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    passwordsSection.filter = 'SHow';
    flush();

    let expectedList = [
      createPasswordEntry({url: 'one.com', username: 'SHOW', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'shower', id: 1}),
      createPasswordEntry({url: 'three.com/show', username: 'four', id: 2}),
      createPasswordEntry({url: 'six-show.com', username: 'one', id: 5}),
    ];

    validatePasswordList(passwordsSection, expectedList);

    // Simulate removal of three.com/show
    passwordList.splice(2, 1);
    flush();

    expectedList = [
      createPasswordEntry({url: 'one.com', username: 'SHOW', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'shower', id: 1}),
      createPasswordEntry({url: 'six-show.com', username: 'one', id: 5}),
    ];

    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        (passwordList);
    flush();
    validatePasswordList(passwordsSection, expectedList);
  });

  test('verifyFilterPasswordExceptions', function() {
    const exceptionList = [
      createExceptionEntry({url: 'docsshoW.google.com', id: 0}),
      createExceptionEntry({url: 'showmail.com', id: 1}),
      createExceptionEntry({url: 'google.com', id: 2}),
      createExceptionEntry({url: 'inbox.google.com', id: 3}),
      createExceptionEntry({url: 'mapsshow.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.comshow', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [], exceptionList);
    passwordsSection.filter = 'shOW';
    flush();

    const expectedExceptionList = [
      createExceptionEntry({url: 'docsshoW.google.com', id: 0}),
      createExceptionEntry({url: 'showmail.com', id: 1}),
      createExceptionEntry({url: 'mapsshow.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.comshow', id: 5}),
    ];

    validateExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
        expectedExceptionList);
  });

  test('verifyNoPasswordExceptions', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);

    validateExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList), []);

    assertFalse(passwordsSection.$.noExceptionsLabel.hidden);
  });

  test('verifyPasswordExceptions', function() {
    const exceptionList = [
      createExceptionEntry({url: 'docs.google.com', id: 0}),
      createExceptionEntry({url: 'mail.com', id: 1}),
      createExceptionEntry({url: 'google.com', id: 2}),
      createExceptionEntry({url: 'inbox.google.com', id: 3}),
      createExceptionEntry({url: 'maps.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.com', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [], exceptionList);

    validateExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
        exceptionList);

    assertTrue(passwordsSection.$.noExceptionsLabel.hidden);
  });

  // Test verifies that removing an exception will update the elements.
  test('verifyPasswordExceptionRemove', function() {
    const exceptionList = [
      createExceptionEntry({url: 'docs.google.com', id: 0}),
      createExceptionEntry({url: 'mail.com', id: 1}),
      createExceptionEntry({url: 'google.com', id: 2}),
      createExceptionEntry({url: 'inbox.google.com', id: 3}),
      createExceptionEntry({url: 'maps.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.com', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [], exceptionList);

    validateExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
        exceptionList);

    // Simulate 'mail.com' being removed from the list.
    passwordsSection.splice('passwordExceptions', 1, 1);
    flush();
    assertFalse(exceptionsListContainsUrl(
        passwordsSection.passwordExceptions, 'mail.com'));
    assertFalse(exceptionsListContainsUrl(exceptionList, 'mail.com'));

    const expectedExceptionList = [
      createExceptionEntry({url: 'docs.google.com', id: 0}),
      createExceptionEntry({url: 'google.com', id: 2}),
      createExceptionEntry({url: 'inbox.google.com', id: 3}),
      createExceptionEntry({url: 'maps.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.com', id: 5}),
    ];
    validateExceptionList(
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList),
        expectedExceptionList);
  });

  // Test verifies that pressing the 'remove' button will trigger a remove
  // event. Does not actually remove any exceptions.
  test('verifyPasswordExceptionRemoveButton', function() {
    const exceptionList = [
      createExceptionEntry({url: 'docs.google.com', id: 0}),
      createExceptionEntry({url: 'mail.com', id: 1}),
      createExceptionEntry({url: 'google.com', id: 2}),
      createExceptionEntry({url: 'inbox.google.com', id: 3}),
      createExceptionEntry({url: 'maps.google.com', id: 4}),
      createExceptionEntry({url: 'plus.google.com', id: 5}),
    ];

    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [], exceptionList);

    const exceptions =
        getDomRepeatChildren(passwordsSection.$.passwordExceptionsList);

    // The index of the button currently being checked.
    let item = 0;

    const clickRemoveButton = function() {
      exceptions[item]!.querySelector<HTMLElement>(
                           '#removeExceptionButton')!.click();
    };

    // Removes the next exception item, verifies that the expected method was
    // called on |passwordManager| and continues recursively until no more items
    // exist.
    function removeNextRecursive(): Promise<void> {
      passwordManager.resetResolver('removeException');
      clickRemoveButton();
      return passwordManager.whenCalled('removeException').then(id => {
        // Verify that the event matches the expected value.
        assertTrue(item < exceptionList.length);
        assertEquals(id, exceptionList[item]!.id);

        if (++item < exceptionList.length) {
          return removeNextRecursive();
        }
        return Promise.resolve();
      });
    }

    // Click 'remove' on all passwords, one by one.
    return removeNextRecursive();
  });

  test('showSavedPasswordListItem', async function() {
    const PASSWORD = 'bAn@n@5';
    const item = createPasswordEntry({url: 'goo.gl', username: 'bart', id: 1});
    passwordManager.setPlaintextPassword(PASSWORD);

    const passwordListItem = elementFactory.createPasswordListItem(item);

    // Hidden passwords should be disabled.
    assertTrue(passwordListItem.shadowRoot!
                   .querySelector<HTMLInputElement>('#password')!.disabled);

    passwordListItem.shadowRoot!
        .querySelector<HTMLElement>('#showPasswordButton')!.click();
    const {id, reason} =
        await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();
    assertEquals(1, id);
    assertEquals('VIEW', reason);

    assertEquals(
        PASSWORD,
        passwordListItem.shadowRoot!
            .querySelector<HTMLInputElement>('#password')!.value);
    // Password should be visible.
    assertEquals(
        'text',
        passwordListItem.shadowRoot!
            .querySelector<HTMLInputElement>('#password')!.type);
    // Visible passwords should not be disabled.
    assertFalse(passwordListItem.shadowRoot!
                    .querySelector<HTMLInputElement>('#password')!.disabled);

    // Hide Password Button should be shown.
    assertTrue(passwordListItem.shadowRoot!
                   .querySelector<HTMLElement>('#showPasswordButton')!.classList
                   .contains('icon-visibility-off'));

    // Hide the Password again.
    passwordListItem.shadowRoot!
        .querySelector<HTMLElement>('#showPasswordButton')!.click();
    flush();

    assertEquals(
        'password',
        passwordListItem.shadowRoot!
            .querySelector<HTMLInputElement>('#password')!.type);
    assertTrue(passwordListItem.shadowRoot!
                   .querySelector<HTMLInputElement>('#password')!.disabled);
    assertTrue(
        passwordListItem.shadowRoot!
            .querySelector<HTMLElement>(
                '#showPasswordButton')!.classList.contains('icon-visibility'));
  });

  test('clickingTheRowOpensSubpageWhenViewPageEnabled', async function() {
    loadTimeData.overrideValues({enablePasswordViewPage: true});
    Router.resetInstanceForTesting(buildRouter());
    routes.PASSWORD_VIEW =
        (Router.getInstance().getRoutes() as SettingsRoutes).PASSWORD_VIEW;
    const URL = 'goo.gl';
    const USERNAME = 'bart';
    const item = createPasswordEntry({url: URL, username: USERNAME, id: 1});

    const passwordSection =
        elementFactory.createPasswordsSection(passwordManager, [item], []);
    const passwordListItem = getFirstPasswordListItem(passwordSection);

    assertFalse(
        isVisible(passwordListItem.shadowRoot!.querySelector<HTMLElement>(
            '#showPasswordButton')));
    assertFalse(isVisible(passwordListItem.$.moreActionsButton));
    const subpageButton = passwordListItem.$.seePasswordDetails;
    assertTrue(isVisible(subpageButton));
    subpageButton.click();
    await flushTasks();

    const router = Router.getInstance();
    assertEquals(routes.PASSWORD_VIEW, router.getCurrentRoute());
    const expectedParams = new URLSearchParams();
    expectedParams.set('username', USERNAME);
    expectedParams.set('site', URL);
    assertDeepEquals(expectedParams, router.getQueryParameters());
  });

  // Tests that pressing 'Edit password' sets the corresponding password.
  test('requestPlaintextPasswordInPasswordEditDialog', async function() {
    const PASSWORD = 'password';
    const entry = createPasswordEntry({url: 'goo.gl', username: 'bart', id: 1});
    passwordManager.setPlaintextPassword(PASSWORD);

    const passwordSection =
        elementFactory.createPasswordsSection(passwordManager, [entry], []);

    getFirstPasswordListItem(passwordSection).$.moreActionsButton.click();
    passwordSection.$.passwordsListHandler.$.menuEditPassword.click();
    flush();

    const {id, reason} =
        await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();
    assertEquals(1, id);
    assertEquals('EDIT', reason);

    const passwordEditDialog =
        passwordSection.$.passwordsListHandler.shadowRoot!.querySelector(
            'password-edit-dialog')!;
    assertEquals('password', passwordEditDialog.$.passwordInput.type);
    assertEquals(PASSWORD, passwordEditDialog.$.passwordInput.value);
    assertTrue(
        passwordEditDialog.shadowRoot!
            .querySelector<HTMLElement>(
                '#showPasswordButton')!.classList.contains('icon-visibility'));
  });

  test('onShowSavedPasswordListItem', async function() {
    const expectedItem =
        createPasswordEntry({url: 'goo.gl', username: 'bart', id: 1});
    const passwordListItem =
        elementFactory.createPasswordListItem(expectedItem);
    assertEquals('', passwordListItem.entry.password);

    passwordManager.setPlaintextPassword('password');
    passwordListItem.shadowRoot!
        .querySelector<HTMLElement>('#showPasswordButton')!.click();
    const {id, reason} =
        await passwordManager.whenCalled('requestPlaintextPassword');
    await flushTasks();
    assertEquals(1, id);
    assertEquals('VIEW', reason);
    assertEquals('password', passwordListItem.entry.password);
  });

  test('onCopyPasswordListItem', function() {
    const expectedItem =
        createPasswordEntry({url: 'goo.gl', username: 'bart', id: 1});
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [expectedItem], []);

    getFirstPasswordListItem(passwordsSection).$.moreActionsButton.click();
    passwordsSection.$.passwordsListHandler.$.menuCopyPassword.click();

    return passwordManager.whenCalled('requestPlaintextPassword')
        .then(({id, reason}) => {
          assertEquals(1, id);
          assertEquals('COPY', reason);
        });
  });

  test('onEditPasswordListItem', function() {
    const expectedItem =
        createPasswordEntry({url: 'goo.gl', username: 'bart', id: 1});
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [expectedItem], []);

    getFirstPasswordListItem(passwordsSection).$.moreActionsButton.click();
    passwordsSection.$.passwordsListHandler.$.menuEditPassword.click();

    return passwordManager.whenCalled('requestPlaintextPassword')
        .then(({id, reason}) => {
          assertEquals(1, id);
          assertEquals('EDIT', reason);
        });
  });

  test('closingPasswordsSectionHidesUndoToast', function() {
    const passwordEntry =
        createPasswordEntry({url: 'goo.gl', username: 'bart'});
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [passwordEntry], []);
    const toastManager = passwordsSection.$.passwordsListHandler.$.removalToast;

    // Click the remove button on the first password and assert that an undo
    // toast is shown.
    getFirstPasswordListItem(passwordsSection).$.moreActionsButton.click();
    passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();
    flush();
    assertTrue(toastManager.open);

    // Remove the passwords section from the DOM and check that this closes
    // the undo toast.
    document.body.removeChild(passwordsSection);
    flush();
    assertFalse(toastManager.open);
  });

  // Chrome offers the export option when there are passwords.
  test('offerExportWhenPasswords', function() {
    const passwordList = [
      createPasswordEntry({url: 'googoo.com', username: 'Larry'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validatePasswordList(passwordsSection, passwordList);
    assertFalse(passwordsSection.$.menuExportPassword.hidden);
  });

  // Chrome shouldn't offer the option to export passwords if there are no
  // passwords.
  test('noExportIfNoPasswords', function() {
    const passwordList: chrome.passwordsPrivate.PasswordUiEntry[] = [];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    validatePasswordList(passwordsSection, passwordList);
    assertTrue(passwordsSection.$.menuExportPassword.hidden);
  });

  test(
      'importPasswordsButtonShownOnlyWhenPasswordsImportFeatureEnabled',
      function() {
        loadTimeData.overrideValues({showImportPasswords: false});
        const passwordsSectionImportPasswordsDisabled =
            elementFactory.createPasswordsSection(passwordManager, [], []);
        assertTrue(
            passwordsSectionImportPasswordsDisabled.shadowRoot!
                .querySelector<HTMLElement>('#menuImportPassword')!.hidden);
        loadTimeData.overrideValues({showImportPasswords: true});
        const passwordsSectionImportPasswordsEnabled =
            elementFactory.createPasswordsSection(passwordManager, [], []);
        assertFalse(
            passwordsSectionImportPasswordsEnabled.shadowRoot!
                .querySelector<HTMLElement>('#menuImportPassword')!.hidden);
      });

  test('importButtonOpensPasswordsImportDialog', function() {
    loadTimeData.overrideValues({showImportPasswords: true});
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    assertFalse(!!passwordsSection.shadowRoot!.querySelector<HTMLElement>(
        '#importPasswordsDialog'));

    passwordsSection.shadowRoot!
        .querySelector<HTMLElement>('#menuImportPassword')!.click();
    flush();
    const importDialog =
        passwordsSection.shadowRoot!.querySelector<HTMLElement>(
            '#importPasswordsDialog');
    assertTrue(!!importDialog);
  });

  // Test that clicking the Export Passwords menu item opens the export
  // dialog.
  test('exportOpen', async function() {
    const passwordList = [
      createPasswordEntry({url: 'googoo.com', username: 'Larry'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);

    passwordsSection.$.menuExportPassword.click();
    // The export dialog calls requestExportProgressStatus() when opening.
    await passwordManager.whenCalled('requestExportProgressStatus');
  });

  if (!(isChromeOS || isLacros)) {
    // Test verifies that the overflow menu does not offer an option to move a
    // password to the account.
    test('noMoveToAccountOption', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);
      assertTrue(passwordsSection.$.passwordsListHandler.$
                     .menuMovePasswordToAccount.hidden);
    });

    // Tests that the opt-in/opt-out buttons appear for signed-in (non-sync)
    // users and that the text content changes accordingly.
    test('changeOptInButtonsBasedOnSignInAndAccountStorageOptIn', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      // Sync is disabled and the user is initially signed out.
      simulateSyncStatus(
          {signedIn: false, statusAction: StatusAction.NO_ACTION});
      function isDisplayed(element: HTMLElement): boolean {
        return !!element && !element.hidden;
      }
      assertFalse(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));

      // User signs in but is not opted in yet.
      simulateStoredAccounts([{email: 'john@gmail.com'}]);
      passwordManager.setIsOptedInForAccountStorageAndNotify(false);
      flush();
      assertTrue(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));
      assertTrue(isDisplayed(passwordsSection.$.optInToAccountStorageButton));
      assertFalse(isDisplayed(passwordsSection.$.optOutOfAccountStorageButton));
      assertTrue(isDisplayed(passwordsSection.$.accountStorageOptInBody));
      assertFalse(isDisplayed(passwordsSection.$.accountStorageOptOutBody));

      // Opt in.
      passwordManager.setIsOptedInForAccountStorageAndNotify(true);
      flush();
      assertTrue(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));
      assertFalse(isDisplayed(passwordsSection.$.optInToAccountStorageButton));
      assertTrue(isDisplayed(passwordsSection.$.optOutOfAccountStorageButton));
      assertTrue(isDisplayed(passwordsSection.$.accountStorageOptOutBody));
      assertFalse(isDisplayed(passwordsSection.$.accountStorageOptInBody));
      assertEquals('john@gmail.com', passwordsSection.$.accountEmail.innerText);

      // Sign out
      simulateStoredAccounts([]);
      assertFalse(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));
    });

    // Test verifies the the account storage buttons are not shown for custom
    // passphrase users.
    test('accountStorageButonsNotShownForCustomPassphraseUser', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      simulateSyncStatus(
          {signedIn: false, statusAction: StatusAction.NO_ACTION});
      simulateStoredAccounts([{email: 'john@gmail.com'}]);
      // Simulate custom passphrase.
      const syncPrefs = getSyncAllPrefs();
      syncPrefs.encryptAllData = true;
      webUIListenerCallback('sync-prefs-changed', syncPrefs);
      flush();

      assertTrue(
          !passwordsSection.$.accountStorageButtonsContainer ||
          passwordsSection.$.accountStorageButtonsContainer.hidden);
    });

    // Test verifies that enabling sync hides the buttons for account storage
    // opt-in/out and the 'device passwords' page.
    test('enablingSyncHidesAccountStorageButtons', function() {
      const passwordsSection =
          elementFactory.createPasswordsSection(passwordManager, [], []);

      simulateAccountStorageUser(passwordManager);
      function isDisplayed(element: HTMLElement): boolean {
        return element && !element.hidden;
      }
      assertTrue(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));

      // Enable sync.
      simulateSyncStatus(
          {signedIn: true, statusAction: StatusAction.NO_ACTION});
      assertFalse(
          isDisplayed(passwordsSection.$.accountStorageButtonsContainer));
    });

    // Test verifies that the button linking to the 'device passwords' page is
    // only visible when there is at least one device password.
    test('verifyDevicePasswordsButtonVisibility', function() {
      // Set up user eligible to the account-scoped password storage, not
      // opted in and with no device passwords. Button should be hidden.
      const passwordList =
          [createPasswordEntry({inAccountStore: true, id: 10})];
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, passwordList, []);
      simulateSyncStatus(
          {signedIn: false, statusAction: StatusAction.NO_ACTION});
      simulateStoredAccounts([{email: 'john@gmail.com'}]);
      assertTrue(passwordsSection.$.devicePasswordsLink.hidden);

      // Opting in still doesn't display it because the user has no device
      // passwords yet.
      passwordManager.setIsOptedInForAccountStorageAndNotify(true);
      flush();
      assertTrue(passwordsSection.$.devicePasswordsLink.hidden);

      // Add a device password. The button shows up.
      passwordList.unshift(createPasswordEntry({inProfileStore: true, id: 20}));
      passwordManager.lastCallback.addSavedPasswordListChangedListener!
          (passwordList);
      flush();
      assertFalse(passwordsSection.$.devicePasswordsLink.hidden);
    });

    // Test verifies that, for account-scoped password storage users, removing
    // a password stored in a single location indicates the location in the
    // toast manager message.
    test(
        'passwordRemovalMessageSpecifiesStoreForAccountStorageUsers',
        function() {
          const passwordList = [
            createPasswordEntry(
                {username: 'account', id: 0, inAccountStore: true}),
            createPasswordEntry(
                {username: 'local', id: 1, inProfileStore: true}),
          ];
          const passwordsSection = elementFactory.createPasswordsSection(
              passwordManager, passwordList, []);

          simulateAccountStorageUser(passwordManager);

          // No removal actually happens, so all passwords keep their position.
          const passwordListItems =
              passwordsSection.shadowRoot!.querySelectorAll(
                  'password-list-item');
          passwordListItems[0]!.$.moreActionsButton.click();
          passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();
          flush();
          assertEquals(
              passwordsSection.i18n('passwordDeletedFromAccount'),
              passwordsSection.$.passwordsListHandler.$.removalNotification
                  .textContent);

          passwordListItems[1]!.$.moreActionsButton.click();
          passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();
          flush();
          assertEquals(
              passwordsSection.i18n('passwordDeletedFromDevice'),
              passwordsSection.$.passwordsListHandler.$.removalNotification
                  .textContent);
        });

    // Test verifies that if the user attempts to remove a password stored
    // both on the device and in the account, the PasswordRemoveDialog shows up.
    // Clicking the button in the dialog then removes both versions of the
    // password.
    test('verifyPasswordRemoveDialogRemoveBothCopies', async function() {
      const password = createPasswordEntry(
          {id: 0, inAccountStore: true, inProfileStore: true});
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [password], []);

      simulateAccountStorageUser(passwordManager);

      // At first the dialog is not shown.
      assertTrue(
          !passwordsSection.$.passwordsListHandler.shadowRoot!.querySelector(
              'password-remove-dialog'));

      // Clicking remove in the overflow menu shows the dialog.
      getFirstPasswordListItem(passwordsSection).$.moreActionsButton.click();
      passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();
      flush();
      const removeDialog =
          passwordsSection.$.passwordsListHandler.shadowRoot!.querySelector(
              'password-remove-dialog');
      assertTrue(!!removeDialog);

      // Both checkboxes are selected by default. Confirming removes from both
      // account and device.
      assertTrue(
          removeDialog.$.removeFromAccountCheckbox.checked &&
          removeDialog.$.removeFromDeviceCheckbox.checked);
      removeDialog.$.removeButton.click();
      const {id, fromStores} =
          await passwordManager.whenCalled('removeSavedPassword');
      assertEquals(password.id, id);
      assertEquals('DEVICE_AND_ACCOUNT', fromStores);
    });

    // Test verifies that if the user attempts to remove a password stored
    // both on the device and in the account, the PasswordRemoveDialog shows up.
    // The user then chooses to remove only of the copies.
    test('verifyPasswordRemoveDialogRemoveSingleCopy', async function() {
      const onAccountAndDevice = createPasswordEntry(
          {id: 0, inAccountStore: true, inProfileStore: true});
      const passwordsSection = elementFactory.createPasswordsSection(
          passwordManager, [onAccountAndDevice], []);

      simulateAccountStorageUser(passwordManager);

      // At first the dialog is not shown.
      assertTrue(
          !passwordsSection.$.passwordsListHandler.shadowRoot!.querySelector(
              '#passwordRemoveDialog'));

      // Clicking remove in the overflow menu shows the dialog.
      getFirstPasswordListItem(passwordsSection).$.moreActionsButton.click();
      passwordsSection.$.passwordsListHandler.$.menuRemovePassword.click();
      flush();
      const removeDialog =
          passwordsSection.$.passwordsListHandler.shadowRoot!.querySelector(
              'password-remove-dialog');
      assertTrue(!!removeDialog);

      // Uncheck the account checkboxes then confirm. Only the device copy is
      // removed.
      removeDialog.$.removeFromAccountCheckbox.click();
      flush();
      assertTrue(
          !removeDialog.$.removeFromAccountCheckbox.checked &&
          removeDialog.$.removeFromDeviceCheckbox.checked);
      removeDialog.$.removeButton.click();
      const {id, fromStores} =
          await passwordManager.whenCalled('removeSavedPassword');
      assertEquals(onAccountAndDevice.id, id);
      assertEquals(chrome.passwordsPrivate.PasswordStoreSet.DEVICE, fromStores);
    });
  }

  test('hideLinkToPasswordManagerWhenEncrypted', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    const syncPrefs = getSyncAllPrefs();
    syncPrefs.encryptAllData = true;
    webUIListenerCallback('sync-prefs-changed', syncPrefs);
    simulateSyncStatus({signedIn: true, statusAction: StatusAction.NO_ACTION});
    flush();
    assertTrue(passwordsSection.$.manageLink.hidden);
  });

  test('showLinkToPasswordManagerWhenNotEncrypted', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    const syncPrefs = getSyncAllPrefs();
    syncPrefs.encryptAllData = false;
    webUIListenerCallback('sync-prefs-changed', syncPrefs);
    flush();
    assertFalse(passwordsSection.$.manageLink.hidden);
  });

  test('hideLinkToPasswordManagerWhenUnifiedPasswordManagerEnabled', () => {
    loadTimeData.overrideValues({unifiedPasswordManagerEnabled: true});
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    assertTrue(passwordsSection.$.manageLink.hidden);
  });

  test('showLinkToPasswordManagerWhenNotSignedIn', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    const syncPrefs = getSyncAllPrefs();
    simulateSyncStatus({signedIn: false, statusAction: StatusAction.NO_ACTION});
    webUIListenerCallback('sync-prefs-changed', syncPrefs);
    flush();
    assertFalse(passwordsSection.$.manageLink.hidden);
  });

  test(
      'showPasswordCheckBannerWhenNotCheckedBeforeAndSignedInAndHavePasswords',
      function() {
        // Suppose no check done initially, non-empty list of passwords,
        // signed in.
        assertEquals(
            passwordManager.data.checkStatus.elapsedTimeSinceLastCheck,
            undefined);
        const passwordList = [
          createPasswordEntry({url: 'site1.com', username: 'luigi'}),
        ];
        const passwordsSection = elementFactory.createPasswordsSection(
            passwordManager, passwordList, []);
        return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
          flush();
          assertFalse(passwordsSection.$.checkPasswordsBannerContainer.hidden);
          assertFalse(passwordsSection.$.checkPasswordsButtonRow.hidden);
          assertTrue(passwordsSection.$.checkPasswordsLinkRow.hidden);
        });
      });

  test(
      'showPasswordCheckBannerWhenCanceledCheckedBeforeAndSignedInAndHavePasswords',
      async function() {
        // Suppose initial check was canceled, non-empty list of passwords,
        // signed in.
        assertEquals(
            passwordManager.data.checkStatus.elapsedTimeSinceLastCheck,
            undefined);
        const passwordList = [
          createPasswordEntry({url: 'site1.com', username: 'luigi', id: 0}),
          createPasswordEntry({url: 'site2.com', username: 'luigi', id: 1}),
        ];
        passwordManager.data.checkStatus.state = PasswordCheckState.CANCELED;
        passwordManager.data.leakedCredentials = [
          makeCompromisedCredential(
              'site1.com', 'luigi',
              chrome.passwordsPrivate.CompromiseType.LEAKED),
        ];
        pluralString.text = '1 compromised password';

        const passwordsSection = elementFactory.createPasswordsSection(
            passwordManager, passwordList, []);

        await passwordManager.whenCalled('getCompromisedCredentials');
        await pluralString.whenCalled('getPluralString');

        flush();
        assertTrue(passwordsSection.$.checkPasswordsBannerContainer.hidden);
        assertTrue(passwordsSection.$.checkPasswordsButtonRow.hidden);
        assertFalse(passwordsSection.$.checkPasswordsLinkRow.hidden);
        assertEquals(
            pluralString.text,
            passwordsSection.shadowRoot!
                .querySelector<HTMLElement>(
                    '#checkPasswordLeakCount')!.innerText!.trim());
      });

  test('showPasswordCheckLinkButtonWithoutWarningWhenNotSignedIn', function() {
    // Suppose no check done initially, non-empty list of passwords,
    // signed out.
    assertEquals(
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck, undefined);
    const passwordList = [
      createPasswordEntry({url: 'site1.com', username: 'luigi'}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    simulateSyncStatus({signedIn: false, statusAction: StatusAction.NO_ACTION});
    return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
      flush();
      assertTrue(passwordsSection.$.checkPasswordsBannerContainer.hidden);
      assertTrue(passwordsSection.$.checkPasswordsButtonRow.hidden);
      assertFalse(passwordsSection.$.checkPasswordsLinkRow.hidden);
    });
  });

  test('showPasswordCheckLinkButtonWithoutWarningWhenNoPasswords', function() {
    // Suppose no check done initially, empty list of passwords, signed
    // in.
    assertEquals(
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck, undefined);
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
      flush();
      assertTrue(passwordsSection.$.checkPasswordsBannerContainer.hidden);
      assertTrue(passwordsSection.$.checkPasswordsButtonRow.hidden);
      assertFalse(passwordsSection.$.checkPasswordsLinkRow.hidden);
    });
  });

  test(
      'showPasswordCheckLinkButtonWithoutWarningWhenNoCredentialsLeaked',
      function() {
        // Suppose no leaks initially, non-empty list of passwords, signed in.
        passwordManager.data.leakedCredentials = [];
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck =
            '5 min ago';
        const passwordList = [
          createPasswordEntry({url: 'site1.com', username: 'luigi'}),
        ];
        const passwordsSection = elementFactory.createPasswordsSection(
            passwordManager, passwordList, []);
        return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
          flush();
          assertTrue(passwordsSection.$.checkPasswordsBannerContainer.hidden);
          assertTrue(passwordsSection.$.checkPasswordsButtonRow.hidden);
          assertFalse(passwordsSection.$.checkPasswordsLinkRow.hidden);
          assertFalse(passwordsSection.$.checkPasswordLeakDescription.hidden);
          assertTrue(passwordsSection.$.checkPasswordWarningIcon.hidden);
          assertTrue(passwordsSection.$.checkPasswordLeakCount.hidden);
        });
      });

  test(
      'showPasswordCheckLinkButtonWithWarningWhenSomeCredentialsLeaked',
      function() {
        // Suppose no leaks initially, non-empty list of passwords, signed in.
        passwordManager.data.leakedCredentials = [
          makeCompromisedCredential(
              'one.com', 'test4',
              chrome.passwordsPrivate.CompromiseType.LEAKED),
          makeCompromisedCredential(
              'two.com', 'test3',
              chrome.passwordsPrivate.CompromiseType.PHISHED),
        ];
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck =
            '5 min ago';
        const passwordList = [
          createPasswordEntry({url: 'site1.com', username: 'luigi'}),
        ];
        const passwordsSection = elementFactory.createPasswordsSection(
            passwordManager, passwordList, []);
        return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
          flush();
          assertTrue(passwordsSection.$.checkPasswordsBannerContainer.hidden);
          assertTrue(passwordsSection.$.checkPasswordsButtonRow.hidden);
          assertFalse(passwordsSection.$.checkPasswordsLinkRow.hidden);
          assertTrue(passwordsSection.$.checkPasswordLeakDescription.hidden);
          assertFalse(passwordsSection.$.checkPasswordWarningIcon.hidden);
          assertFalse(passwordsSection.$.checkPasswordLeakCount.hidden);
        });
      });

  test('makeWarningAppearWhenLeaksDetected', function() {
    // Suppose no leaks detected initially, non-empty list of passwords,
    // signed in.
    assertEquals(
        passwordManager.data.checkStatus.elapsedTimeSinceLastCheck, undefined);
    passwordManager.data.leakedCredentials = [];
    passwordManager.data.checkStatus.elapsedTimeSinceLastCheck = '5 min ago';
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'test4', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'test3', id: 1}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
      flush();
      assertTrue(passwordsSection.$.checkPasswordsBannerContainer.hidden);
      assertTrue(passwordsSection.$.checkPasswordsButtonRow.hidden);
      assertFalse(passwordsSection.$.checkPasswordsLinkRow.hidden);
      assertFalse(passwordsSection.$.checkPasswordLeakDescription.hidden);
      assertTrue(passwordsSection.$.checkPasswordWarningIcon.hidden);
      assertTrue(passwordsSection.$.checkPasswordLeakCount.hidden);
      // Suppose two newly detected leaks come in.
      const leakedCredentials = [
        makeCompromisedCredential(
            'one.com', 'test4', chrome.passwordsPrivate.CompromiseType.LEAKED),
        makeCompromisedCredential(
            'two.com', 'test3', chrome.passwordsPrivate.CompromiseType.PHISHED),
      ];
      const elapsedTimeSinceLastCheck = 'just now';
      passwordManager.data.leakedCredentials = leakedCredentials;
      passwordManager.data.checkStatus.elapsedTimeSinceLastCheck =
          elapsedTimeSinceLastCheck;
      passwordManager.lastCallback.addCompromisedCredentialsListener!
          (leakedCredentials);
      passwordManager.lastCallback.addPasswordCheckStatusListener!
          (makePasswordCheckStatus(
              /*state=*/ PasswordCheckState.RUNNING,
              /*checked=*/ 2,
              /*remaining=*/ 0,
              /*elapsedTime=*/ elapsedTimeSinceLastCheck));
      flush();
      assertTrue(passwordsSection.$.checkPasswordsBannerContainer.hidden);
      assertTrue(passwordsSection.$.checkPasswordsButtonRow.hidden);
      assertFalse(passwordsSection.$.checkPasswordsLinkRow.hidden);
      assertTrue(passwordsSection.$.checkPasswordLeakDescription.hidden);
      assertFalse(passwordsSection.$.checkPasswordWarningIcon.hidden);
      assertFalse(passwordsSection.$.checkPasswordLeakCount.hidden);
    });
  });

  test('makeBannerDisappearWhenSignedOut', function() {
    // Suppose no leaks detected initially, non-empty list of passwords,
    // signed in.
    const passwordList = [
      createPasswordEntry({url: 'one.com', username: 'test4', id: 0}),
      createPasswordEntry({url: 'two.com', username: 'test3', id: 1}),
    ];
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, passwordList, []);
    return passwordManager.whenCalled('getPasswordCheckStatus').then(() => {
      flush();
      assertFalse(passwordsSection.$.checkPasswordsBannerContainer.hidden);
      assertFalse(passwordsSection.$.checkPasswordsButtonRow.hidden);
      assertTrue(passwordsSection.$.checkPasswordsLinkRow.hidden);

      simulateSyncStatus(
          {signedIn: false, statusAction: StatusAction.NO_ACTION});
      assertTrue(passwordsSection.$.checkPasswordsBannerContainer.hidden);
      assertTrue(passwordsSection.$.checkPasswordsButtonRow.hidden);
      assertFalse(passwordsSection.$.checkPasswordsLinkRow.hidden);
    });
  });

  test('clickingCheckPasswordsButtonStartsCheck', async function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    passwordsSection.shadowRoot!
        .querySelector<HTMLElement>('#checkPasswordsButton')!.click();
    flush();
    const router = Router.getInstance();
    assertEquals(routes.CHECK_PASSWORDS, router.currentRoute);
    assertEquals('true', router.getQueryParameters().get('start'));
    const referrer =
        await passwordManager.whenCalled('recordPasswordCheckReferrer');
    assertEquals(PasswordCheckReferrer.PASSWORD_SETTINGS, referrer);
  });

  test('clickingCheckPasswordsRowStartsCheck', async function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    passwordsSection.shadowRoot!
        .querySelector<HTMLElement>('#checkPasswordsLinkRow')!.click();
    flush();
    const router = Router.getInstance();
    assertEquals(routes.CHECK_PASSWORDS, router.currentRoute);
    assertEquals('true', router.getQueryParameters().get('start'));
    const referrer =
        await passwordManager.whenCalled('recordPasswordCheckReferrer');
    assertEquals(PasswordCheckReferrer.PASSWORD_SETTINGS, referrer);
  });

  test('hatsInformedOnOpen', async function() {
    elementFactory.createPasswordsSection(passwordManager, [], []);
    const interaction =
        await testHatsBrowserProxy.whenCalled('trustSafetyInteractionOccurred');
    assertEquals(TrustSafetyInteraction.OPENED_PASSWORD_MANAGER, interaction);
  });

  test('passwordScriptsRefreshedOnOpen', async function() {
    loadTimeData.overrideValues(
        {enableAutomaticPasswordChangeInSettings: true});
    elementFactory.createPasswordsSection(passwordManager, [], []);
    Router.getInstance().navigateTo(routes.PASSWORDS);
    await passwordManager.whenCalled('refreshScriptsIfNecessary');
  });

  test(
      'addPasswordButtonShownOnlyWhenPasswordManagerNotDisabledByPolicy',
      function() {
        const passwordsSection =
            elementFactory.createPasswordsSection(passwordManager, [], []);
        const addButton =
            passwordsSection.shadowRoot!.querySelector<HTMLElement>(
                '#addPasswordButton')!;

        passwordsSection.set('prefs.credentials_enable_service', {
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          value: false,
        });
        flush();
        assertTrue(addButton.style.display === 'none');

        passwordsSection.set('prefs.credentials_enable_service.value', true);
        flush();
        assertFalse(addButton.style.display === 'none');
      });

  test('addPasswordButtonOpensAddPasswordDialog', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    assertFalse(!!passwordsSection.shadowRoot!.querySelector<HTMLElement>(
        '#addPasswordDialog'));

    passwordsSection.shadowRoot!
        .querySelector<HTMLElement>('#addPasswordButton')!.click();
    flush();
    const addDialog = passwordsSection.shadowRoot!.querySelector<HTMLElement>(
        '#addPasswordDialog');
    assertTrue(!!addDialog);
  });

  test('trustedVaultBannerVisibilityChangesWithState', function() {
    const passwordsSection =
        elementFactory.createPasswordsSection(passwordManager, [], []);
    webUIListenerCallback(
        'trusted-vault-banner-state-changed',
        TrustedVaultBannerState.NOT_SHOWN);
    flush();
    assertTrue(passwordsSection.$.trustedVaultBanner.hidden);

    webUIListenerCallback(
        'trusted-vault-banner-state-changed',
        TrustedVaultBannerState.OFFER_OPT_IN);
    flush();
    assertFalse(passwordsSection.$.trustedVaultBanner.hidden);
    assertEquals(
        passwordsSection.i18n('trustedVaultBannerSubLabelOfferOptIn'),
        passwordsSection.$.trustedVaultBanner.subLabel);

    webUIListenerCallback(
        'trusted-vault-banner-state-changed', TrustedVaultBannerState.OPTED_IN);
    flush();
    assertFalse(passwordsSection.$.trustedVaultBanner.hidden);
    assertEquals(
        passwordsSection.i18n('trustedVaultBannerSubLabelOptedIn'),
        passwordsSection.$.trustedVaultBanner.subLabel);
  });

  test('routingWithRemovalParamsShowsNotification', function() {
    const passwordEntry =
        createPasswordEntry({url: 'goo.gl', username: 'bart'});
    const passwordsSection = elementFactory.createPasswordsSection(
        passwordManager, [passwordEntry], []);
    const toastManager = passwordsSection.$.passwordsListHandler.$.removalToast;

    const params = new URLSearchParams();
    params.set('removedFromStores', passwordEntry.storedIn);
    Router.getInstance().navigateTo(routes.PASSWORDS, params);

    flush();
    assertTrue(toastManager.open);

    // Remove the passwords section from the DOM and check that this closes
    // the undo toast.
    document.body.removeChild(passwordsSection);
    flush();
    assertFalse(toastManager.open);
  });
});
