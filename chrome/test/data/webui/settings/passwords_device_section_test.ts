// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer tests for the PasswordsDeviceSection page. */

import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {IronListElement, PasswordMoveToAccountDialogElement, PasswordsDeviceSectionElement} from 'chrome://settings/lazy_load.js';
import {MultiStorePasswordUiEntry, PasswordManagerImpl, Router, routes, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {createMultiStorePasswordEntry, createPasswordEntry, PasswordDeviceSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {simulateStoredAccounts, simulateSyncStatus} from './sync_test_util.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

/**
 * Sets the fake password data, the appropriate route and creates the element.
 */
async function createPasswordsDeviceSection(
    syncBrowserProxy: TestSyncBrowserProxy,
    passwordManager: TestPasswordManagerProxy,
    passwordList: Array<chrome.passwordsPrivate.PasswordUiEntry>):
    Promise<PasswordsDeviceSectionElement> {
  passwordManager.data.passwords = passwordList;
  Router.getInstance().setCurrentRoute(
      routes.DEVICE_PASSWORDS, new URLSearchParams(), false);
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
 * @param subsection The passwords subsection element that will be checked.
 * @param expectedPasswords The expected passwords in this subsection.
 */
function validatePasswordsSubsection(
    subsection: IronListElement,
    expectedPasswords: Array<MultiStorePasswordUiEntry>) {
  assertDeepEquals(expectedPasswords, subsection.items);
  const listItemElements = subsection.querySelectorAll('password-list-item');
  for (let index = 0; index < expectedPasswords.length; ++index) {
    const expectedPassword = expectedPasswords[index]!;
    const listItemElement = listItemElements[index];
    assertTrue(!!listItemElement);
    assertEquals(
        expectedPassword.urls.shown,
        listItemElement.$.originUrl.textContent!.trim());
    assertEquals(expectedPassword.urls.link, listItemElement.$.originUrl.href);
    assertEquals(expectedPassword.username, listItemElement.$.username.value);
  }
}

suite('PasswordsDeviceSection', function() {
  let passwordManager: TestPasswordManagerProxy;
  let syncBrowserProxy: TestSyncBrowserProxy;
  const SIGNED_IN_ACCOUNT = {email: 'john@gmail.com'};
  let elementFactory: PasswordDeviceSectionElementFactory;

  setup(function() {
    document.body.innerHTML = '';
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);
    elementFactory = new PasswordDeviceSectionElementFactory(document);

    // The user only enters this page when they are eligible (signed-in but not
    // syncing) and opted-in to account storage.
    syncBrowserProxy.storedAccounts = [SIGNED_IN_ACCOUNT];
    simulateStoredAccounts(syncBrowserProxy.storedAccounts);
    syncBrowserProxy.testSyncStatus = {
      signedIn: false,
      statusAction: StatusAction.NO_ACTION
    };
    simulateSyncStatus(syncBrowserProxy.testSyncStatus);
    passwordManager.setIsOptedInForAccountStorageAndNotify(true);
  });

  // Test verifies that the fallback text is displayed when passwords are not
  // present.
  test('verifyPasswordsEmptySubsections', async function() {
    const passwordsDeviceSection = await createPasswordsDeviceSection(
        syncBrowserProxy, passwordManager, []);
    assertFalse(
        passwordsDeviceSection.shadowRoot!
            .querySelector<HTMLElement>('#noDeviceOnlyPasswordsLabel')!.hidden);
    assertFalse(passwordsDeviceSection.shadowRoot!
                    .querySelector<HTMLElement>(
                        '#noDeviceAndAccountPasswordsLabel')!.hidden);
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
    assertTrue(
        passwordsDeviceSection.shadowRoot!
            .querySelector<HTMLElement>('#noDeviceOnlyPasswordsLabel')!.hidden);
    assertTrue(passwordsDeviceSection.shadowRoot!
                   .querySelector<HTMLElement>(
                       '#noDeviceAndAccountPasswordsLabel')!.hidden);
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
    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        (passwordList);
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
    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        (passwordList);
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
            passwordsDeviceSection.shadowRoot!.querySelectorAll(
                'password-list-item');

        passwordElements[0]!.$.moreActionsButton.click();
        flush();
        let moveToAccountOption = passwordsDeviceSection.$.passwordsListHandler
                                      .$.menuMovePasswordToAccount;
        assertFalse(moveToAccountOption.hidden);

        passwordsDeviceSection.$.passwordsListHandler.$.menu.close();

        passwordElements[1]!.$.moreActionsButton.click();
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
    const password = passwordsDeviceSection.shadowRoot!.querySelectorAll(
        'password-list-item')[0]!;

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
        {username: 'both', id: 2, frontendId: 42, fromAccountStore: true});
    const deviceCopy = createPasswordEntry(
        {username: 'both', id: 1, frontendId: 42, fromAccountStore: false});
    const passwordsDeviceSection = await createPasswordsDeviceSection(
        syncBrowserProxy, passwordManager, [deviceCopy, accountCopy]);

    // At first the dialog is not shown.
    assertFalse(!!passwordsDeviceSection.$.passwordsListHandler.shadowRoot!
                      .querySelector('#passwordMoveToAccountDialog'));

    // Click the option in the overflow menu to move the password. Verify the
    // dialog is now open.
    const password = passwordsDeviceSection.shadowRoot!.querySelectorAll(
        'password-list-item')[0]!;
    password.$.moreActionsButton.click();
    passwordsDeviceSection.$.passwordsListHandler.$.menuMovePasswordToAccount
        .click();
    flush();
    const moveToAccountDialog =
        passwordsDeviceSection.$.passwordsListHandler.shadowRoot!
            .querySelector<PasswordMoveToAccountDialogElement>(
                '#passwordMoveToAccountDialog');
    assertTrue(!!moveToAccountDialog);

    // Click the Move button in the dialog. The API should be called with the id
    // for the device copy. Verify the dialog disappears.
    moveToAccountDialog.$.moveButton.click();
    const movedId = await passwordManager.whenCalled('movePasswordsToAccount');
    assertEquals(deviceCopy.id, movedId[0]);
  });

  // Test verifies that Chrome navigates to the standard passwords page if the
  // user enables sync.
  test('leavesPageIfSyncIsEnabled', async function() {
    await createPasswordsDeviceSection(syncBrowserProxy, passwordManager, []);
    assertEquals(Router.getInstance().currentRoute, routes.DEVICE_PASSWORDS);
    simulateSyncStatus({signedIn: true, statusAction: StatusAction.NO_ACTION});
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

  // The move multiple password dialog is dismissable.
  test('moveMultiplePasswordsDialogDismissable', function() {
    const deviceEntry = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', deviceId: 42});
    const moveMultipleDialog =
        elementFactory.createMoveMultiplePasswordsDialog([deviceEntry]);
    assertTrue(moveMultipleDialog.$.dialog.open);
    moveMultipleDialog.$.cancelButton.click();
    flush();
    assertFalse(moveMultipleDialog.$.dialog.open);
  });

  test('moveMultiplePasswordsDialogFiresCloseEventWhenCanceled', function() {
    const deviceEntry = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', deviceId: 42});
    const moveMultipleDialog =
        elementFactory.createMoveMultiplePasswordsDialog([deviceEntry]);
    moveMultipleDialog.$.cancelButton.click();
    return eventToPromise('close', moveMultipleDialog);
  });

  // Testing moving multiple password dialog Move button.
  test('moveMultiplePasswordsDialogMoveButton', async function() {
    const deviceEntry1 = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart1', deviceId: 41});
    const deviceEntry2 = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart2', deviceId: 54});
    const moveMultipleDialog = elementFactory.createMoveMultiplePasswordsDialog(
        [deviceEntry1, deviceEntry2]);
    // Uncheck the first entry.
    const firstPasswordItem =
        moveMultipleDialog.shadowRoot!.querySelector('password-list-item')!;
    firstPasswordItem.querySelector('cr-checkbox')!.click();
    // Press the Move button
    moveMultipleDialog.$.moveButton.click();
    flush();
    // Only the 2nd entry should be moved
    const movedIds = await passwordManager.whenCalled('movePasswordsToAccount');
    assertEquals(1, movedIds.length);
    assertEquals(deviceEntry2.deviceId, movedIds[0]);
    // The dialog should be closed.
    assertFalse(moveMultipleDialog.$.dialog.open);
  });

  // Testing moving multiple password dialog doesn't have more actions menu
  // button next to each password row but it has the eye icon
  [false, true].forEach(
      enablePasswordViewPage => test(
          `moveMultiplePasswordsDialogButtonVisibilities` +
              `WhenPasswordNotesEnabledIs_${enablePasswordViewPage}`,
          function() {
            loadTimeData.overrideValues({enablePasswordViewPage});
            const deviceEntry = createMultiStorePasswordEntry(
                {url: 'goo.gl', username: 'bart', deviceId: 42});
            const moveMultipleDialog =
                elementFactory.createMoveMultiplePasswordsDialog([deviceEntry]);
            const firstPasswordItem =
                moveMultipleDialog.shadowRoot!.querySelector(
                    'password-list-item')!;
            assertFalse(isVisible(firstPasswordItem.$.moreActionsButton));
            assertTrue(isVisible(firstPasswordItem.shadowRoot!.querySelector(
                '#showPasswordButton')));
          }));

  test(
      'moveMultiplePasswordsBannerHiddenWhenNoLocalPasswords',
      async function() {
        const passwordsDeviceSection = await createPasswordsDeviceSection(
            syncBrowserProxy, passwordManager, []);

        assertTrue(passwordsDeviceSection.$.moveMultiplePasswordsBanner.hidden);
      });

  test(
      'moveMultiplePasswordsBannerVisibleWhenLocalPasswords', async function() {
        const devicePassword = createPasswordEntry(
            {username: 'device', id: 0, fromAccountStore: false});
        const passwordsDeviceSection = await createPasswordsDeviceSection(
            syncBrowserProxy, passwordManager, [devicePassword]);

        assertFalse(
            passwordsDeviceSection.$.moveMultiplePasswordsBanner.hidden);
      });

  test(
      'moveMultiplePasswordsBannerHiddenWhenConflictingLocalAndDevicesPasswords',
      async function() {
        // The existence of two entries with the same url and password username
        // indicate that they must have different passwords. Otherwise, they
        // would have deduped earlier.
        const devicePassword = createPasswordEntry({
          url: 'www.test.com',
          username: 'username',
          id: 0,
          fromAccountStore: false
        });
        const accountPassword = createPasswordEntry({
          url: 'www.test.com',
          username: 'username',
          id: 1,
          fromAccountStore: true
        });

        const passwordsDeviceSection = await createPasswordsDeviceSection(
            syncBrowserProxy, passwordManager,
            [devicePassword, accountPassword]);

        assertTrue(passwordsDeviceSection.$.moveMultiplePasswordsBanner.hidden);
      });
});
