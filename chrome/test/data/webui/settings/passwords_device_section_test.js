// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer tests for the PasswordsDeviceSection page. */

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MultiStorePasswordUiEntry, PasswordManagerImpl, Router, routes, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {createMultiStorePasswordEntry, createPasswordEntry} from 'chrome://test/settings/passwords_and_autofill_fake_data.js';
import {simulateStoredAccounts, simulateSyncStatus} from 'chrome://test/settings/sync_test_util.m.js';
import {TestPasswordManagerProxy} from 'chrome://test/settings/test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.m.js';

/**
 * Sets the fake password data, the appropriate route and creates the element.
 * @param {!TestSyncBrowserProxy} syncBrowserProxy
 * @param {!TestPasswordManagerProxy} passwordManager
 * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} passwordList
 * @return {!Object}
 */
async function createPasswordsDeviceSection(
    syncBrowserProxy, passwordManager, passwordList) {
  passwordManager.data.passwords = passwordList;
  Router.getInstance().setCurrentRoute(
      routes.DEVICE_PASSWORDS, new URLSearchParams());
  const passwordsDeviceSection =
      document.createElement('passwords-device-section');
  document.body.appendChild(passwordsDeviceSection);
  flush();

  // Wait for the initial state of sync and account storage opt in to be queried
  // since this could cause a redirect.
  await syncBrowserProxy.whenCalled('getSyncStatus');
  await syncBrowserProxy.whenCalled('getStoredAccounts');
  await passwordManager.whenCalled('isOptedInForAccountStorage');

  return passwordsDeviceSection;
}

/**
 * @param {!Element} subsection The passwords subsection element that will be
 * checked.
 * @param {!Array<!MultiStorePasswordUiEntry>} expectedPasswords The
 *     expected passwords in this subsection.
 * @private
 */
function validatePasswordsSubsection(subsection, expectedPasswords) {
  assertDeepEquals(expectedPasswords, subsection.items);
  const listItemElements = subsection.querySelectorAll('password-list-item');
  for (let index = 0; index < expectedPasswords.length; ++index) {
    const expectedPassword = expectedPasswords[index];
    const listItemElement = listItemElements[index];
    assertTrue(!!listItemElement);
    assertEquals(
        expectedPassword.urls.shown,
        listItemElement.$.originUrl.textContent.trim());
    assertEquals(expectedPassword.urls.link, listItemElement.$.originUrl.href);
    assertEquals(expectedPassword.username, listItemElement.$.username.value);
  }
}

suite('PasswordsDeviceSection', function() {
  /** @type {TestPasswordManagerProxy} */
  let passwordManager = null;
  /** @type {TestSyncBrowserProxy} */
  let syncBrowserProxy = null;
  /** @type {!settings.StoredAccount} */
  const SIGNED_IN_ACCOUNT = {email: 'john@gmail.com'};

  setup(function() {
    PolymerTest.clearBody();
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.instance_ = passwordManager;
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

    // The user only enters this page when they are eligible (signed-in but not
    // syncing) and opted-in to account storage.
    syncBrowserProxy.storedAccounts = [SIGNED_IN_ACCOUNT];
    simulateStoredAccounts(syncBrowserProxy.storedAccounts);
    syncBrowserProxy.syncStatus = {signedIn: false};
    simulateSyncStatus(syncBrowserProxy.syncStatus);
    passwordManager.setIsOptedInForAccountStorageAndNotify(true);
  });

  // Test verifies that the fallback text is displayed when passwords are not
  // present.
  test('verifyPasswordsEmptySubsections', async function() {
    const passwordsDeviceSection = await createPasswordsDeviceSection(
        syncBrowserProxy, passwordManager, []);
    assertFalse(passwordsDeviceSection.shadowRoot
                    .querySelector('#noDeviceOnlyPasswordsLabel')
                    .hidden);
    assertFalse(passwordsDeviceSection.shadowRoot
                    .querySelector('#noDeviceAndAccountPasswordsLabel')
                    .hidden);
  });

  // Test verifies that account passwords are not displayed, whereas
  // device-only and device-and-account ones end up in the correct subsection.
  test('verifyPasswordsFilledSubsections', async function() {
    const devicePassword = createPasswordEntry(
        {username: 'device', id: 0, fromAccountStore: false});
    const accountPassword = createPasswordEntry(
        {username: 'account', id: 1, fromAccountStore: true});
    // Create duplicate that gets merged.
    const deviceCopyPassword = createPasswordEntry(
        {username: 'both', frontendId: 42, id: 2, fromAccountStore: false});
    const accountCopyPassword = createPasswordEntry(
        {username: 'both', frontendId: 42, id: 3, fromAccountStore: true});

    // Shuffle entries a little.
    const passwordsDeviceSection =
        await createPasswordsDeviceSection(syncBrowserProxy, passwordManager, [
          devicePassword,
          deviceCopyPassword,
          accountPassword,
          accountCopyPassword,
        ]);

    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, [
          createMultiStorePasswordEntry({username: 'device', deviceId: 0}),
        ]);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList, [
          createMultiStorePasswordEntry(
              {username: 'both', deviceId: 2, accountId: 3}),
        ]);
    assertTrue(passwordsDeviceSection.shadowRoot
                   .querySelector('#noDeviceOnlyPasswordsLabel')
                   .hidden);
    assertTrue(passwordsDeviceSection.shadowRoot
                   .querySelector('#noDeviceAndAccountPasswordsLabel')
                   .hidden);
  });

  // Test verifies that removing the device copy of a duplicated password
  // removes it from both lists.
  test('verifyPasswordListRemoveDeviceCopy', async function() {
    const passwordList = [
      createPasswordEntry({frontendId: 42, id: 10, fromAccountStore: true}),
      createPasswordEntry({frontendId: 42, id: 20, fromAccountStore: false}),
    ];

    const passwordsDeviceSection = await createPasswordsDeviceSection(
        syncBrowserProxy, passwordManager, passwordList);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, []);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList,
        [createMultiStorePasswordEntry({accountId: 10, deviceId: 20})]);

    // Remove device copy.
    passwordList.splice(1, 1);
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, []);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList, []);
  });

  // Test verifies that removing the account copy of a duplicated password
  // moves it to the other subsection.
  test('verifyPasswordListRemoveDeviceCopy', async function() {
    const passwordList = [
      createPasswordEntry({frontendId: 42, id: 10, fromAccountStore: true}),
      createPasswordEntry({frontendId: 42, id: 20, fromAccountStore: false}),
    ];

    const passwordsDeviceSection = await createPasswordsDeviceSection(
        syncBrowserProxy, passwordManager, passwordList);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, []);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList,
        [createMultiStorePasswordEntry({accountId: 10, deviceId: 20})]);

    // Remove account copy.
    passwordList.splice(0, 1);
    passwordManager.lastCallback.addSavedPasswordListChangedListener(
        passwordList);
    flush();

    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList,
        [createMultiStorePasswordEntry({deviceId: 20})]);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList, []);
  });

  // Test checks that when the overflow menu is opened for any password not
  // corresponding to the first signed-in account, an option to move it to that
  // account is shown.
  test(
      'hasMoveToAccountOptionIfIsNotSignedInAccountPassword', async function() {
        const nonGooglePasswordWithSameEmail = createPasswordEntry(
            {username: SIGNED_IN_ACCOUNT.email, url: 'not-google.com'});
        const googlePasswordWithDifferentEmail = createPasswordEntry(
            {username: 'another-user', url: 'accounts.google.com'});
        const passwordsDeviceSection = await createPasswordsDeviceSection(
            syncBrowserProxy, passwordManager,
            [nonGooglePasswordWithSameEmail, googlePasswordWithDifferentEmail]);
        const passwordElements =
            passwordsDeviceSection.root.querySelectorAll('password-list-item');

        passwordElements[0].$.moreActionsButton.click();
        flush();
        let moveToAccountOption = passwordsDeviceSection.$.passwordsListHandler
                                      .$.menuMovePasswordToAccount;
        assertFalse(moveToAccountOption.hidden);

        passwordsDeviceSection.$.passwordsListHandler.$.menu.close();

        passwordElements[1].$.moreActionsButton.click();
        flush();
        moveToAccountOption = passwordsDeviceSection.$.passwordsListHandler.$
                                  .menuMovePasswordToAccount;
        assertFalse(moveToAccountOption.hidden);
      });

  // Test checks that when the overflow menu is opened for the password
  // corresponding to the first signed-in account, no option to move it to the
  // same account is shown.
  test('hasNoMoveToAccountOptionIfIsSignedInAccountPassword', async function() {
    const signedInGoogleAccountPassword = createPasswordEntry(
        {username: SIGNED_IN_ACCOUNT.email, url: 'accounts.google.com'});
    const passwordsDeviceSection = await createPasswordsDeviceSection(
        syncBrowserProxy, passwordManager, [signedInGoogleAccountPassword]);
    const [password] =
        passwordsDeviceSection.root.querySelectorAll('password-list-item');

    password.$.moreActionsButton.click();
    flush();
    const moveToAccountOption = passwordsDeviceSection.$.passwordsListHandler.$
                                    .menuMovePasswordToAccount;
    assertTrue(moveToAccountOption.hidden);
  });


  // Test verifies that clicking the 'move to account' button displays the
  // dialog and that clicking the "Move" button then moves the device copy.
  test('verifyMovesCorrectIdToAccount', async function() {
    // Create duplicated password that will be merged in the UI.
    const accountCopy = createPasswordEntry(
        {user: 'both', id: 2, frontendId: 42, fromAccountStore: true});
    const deviceCopy = createPasswordEntry(
        {user: 'both', id: 1, frontendId: 42, fromAccountStore: false});
    const passwordsDeviceSection = await createPasswordsDeviceSection(
        syncBrowserProxy, passwordManager, [deviceCopy, accountCopy]);

    // At first the dialog is not shown.
    assertFalse(!!passwordsDeviceSection.$.passwordsListHandler.$$(
        '#passwordMoveToAccountDialog'));

    // Click the option in the overflow menu to move the password. Verify the
    // dialog is now open.
    const [password] =
        passwordsDeviceSection.root.querySelectorAll('password-list-item');
    password.$.moreActionsButton.click();
    passwordsDeviceSection.$.passwordsListHandler.$.menuMovePasswordToAccount
        .click();
    flush();
    const moveToAccountDialog =
        passwordsDeviceSection.$.passwordsListHandler.$$(
            '#passwordMoveToAccountDialog');
    assertTrue(!!moveToAccountDialog);

    // Click the Move button in the dialog. The API should be called with the id
    // for the device copy. Verify the dialog disappears.
    moveToAccountDialog.$.moveButton.click();
    const movedId = await passwordManager.whenCalled('movePasswordToAccount');
    assertEquals(deviceCopy.id, movedId);
  });

  // Test verifies that Chrome navigates to the standard passwords page if the
  // user enables sync.
  test('leavesPageIfSyncIsEnabled', async function() {
    await createPasswordsDeviceSection(syncBrowserProxy, passwordManager, []);
    assertEquals(Router.getInstance().currentRoute, routes.DEVICE_PASSWORDS);
    simulateSyncStatus({signedIn: true});
    flush();
    assertEquals(Router.getInstance().currentRoute, routes.PASSWORDS);
  });

  // Test verifies that Chrome navigates to the standard passwords page if the
  // user signs out.
  test('leavesPageIfUserSignsOut', async function() {
    await createPasswordsDeviceSection(syncBrowserProxy, passwordManager, []);
    assertEquals(Router.getInstance().currentRoute, routes.DEVICE_PASSWORDS);
    simulateStoredAccounts([]);
    flush();
    assertEquals(Router.getInstance().currentRoute, routes.PASSWORDS);
  });

  // Test verifies that Chrome navigates to the standard passwords page if the
  // user opts out of the account-scoped password storage.
  test('leavesPageIfUserOptsOut', async function() {
    await createPasswordsDeviceSection(syncBrowserProxy, passwordManager, []);
    assertEquals(Router.getInstance().currentRoute, routes.DEVICE_PASSWORDS);
    passwordManager.setIsOptedInForAccountStorageAndNotify(false);
    flush();
    assertEquals(Router.getInstance().currentRoute, routes.PASSWORDS);
  });
});
