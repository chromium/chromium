// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer tests for the PasswordsDeviceSection page. */

import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {IronListElement, PasswordMoveToAccountDialogElement, PasswordsDeviceSectionElement} from 'chrome://settings/lazy_load.js';
import {PasswordManagerImpl, Router, routes, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {createPasswordEntry, PasswordDeviceSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {simulateStoredAccounts, simulateSyncStatus} from './sync_test_util.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

/**
 * Sets the fake password data, the appropriate route and creates the element.
 */
async function createPasswordsDeviceSection(
    syncBrowserProxy: TestSyncBrowserProxy,
    passwordManager: TestPasswordManagerProxy,
    passwordList: chrome.passwordsPrivate.PasswordUiEntry[]):
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
  await passwordManager.whenCalled('getSavedPasswordList');

  await flushTasks();

  return passwordsDeviceSection;
}

/**
 * @param subsection The passwords subsection element that will be checked.
 * @param expectedPasswords The expected passwords in this subsection.
 */
function validatePasswordsSubsection(
    subsection: IronListElement,
    expectedPasswords: chrome.passwordsPrivate.PasswordUiEntry[]) {
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
    assertEquals(
        expectedPassword.username,
        listItemElement.$.username.textContent!.trim());
  }
}

suite('PasswordsDeviceSection', function() {
  let passwordManager: TestPasswordManagerProxy;
  let syncBrowserProxy: TestSyncBrowserProxy;
  const SIGNED_IN_ACCOUNT = {email: 'john@gmail.com'};
  let elementFactory: PasswordDeviceSectionElementFactory;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
      statusAction: StatusAction.NO_ACTION,
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
    const devicePassword =
        createPasswordEntry({username: 'device', id: 0, inProfileStore: true});
    const accountPassword =
        createPasswordEntry({username: 'account', id: 1, inAccountStore: true});
    // Password present in both account and profile storage.
    const deviceAndAccountPassword = createPasswordEntry(
        {username: 'both', id: 2, inProfileStore: true, inAccountStore: true});

    // Shuffle entries a little.
    const passwordsDeviceSection =
        await createPasswordsDeviceSection(syncBrowserProxy, passwordManager, [
          devicePassword,
          accountPassword,
          deviceAndAccountPassword,
        ]);

    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, [
          createPasswordEntry({username: 'device', id: 0}),
        ]);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList, [
          createPasswordEntry({
            username: 'both',
            id: 2,
            inProfileStore: true,
            inAccountStore: true,
          }),
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
      createPasswordEntry({id: 10, inAccountStore: true, inProfileStore: true}),
    ];

    const passwordsDeviceSection = await createPasswordsDeviceSection(
        syncBrowserProxy, passwordManager, passwordList);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, []);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList, passwordList);

    // Remove device copy.
    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        ([createPasswordEntry({id: 10, inAccountStore: true})]);
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
      createPasswordEntry({id: 10, inAccountStore: true, inProfileStore: true}),
    ];

    const passwordsDeviceSection = await createPasswordsDeviceSection(
        syncBrowserProxy, passwordManager, passwordList);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList, []);
    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceAndAccountPasswordList,
        [createPasswordEntry(
            {inAccountStore: true, inProfileStore: true, id: 10})]);

    // Remove account copy.
    passwordManager.lastCallback.addSavedPasswordListChangedListener!
        ([createPasswordEntry({id: 10, inProfileStore: true})]);
    flush();

    validatePasswordsSubsection(
        passwordsDeviceSection.$.deviceOnlyPasswordList,
        [createPasswordEntry({id: 10})]);
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
    const passwordOnAccountAndDevice = createPasswordEntry(
        {username: 'both', id: 2, inAccountStore: true, inProfileStore: true});
    const passwordsDeviceSection = await createPasswordsDeviceSection(
        syncBrowserProxy, passwordManager, [passwordOnAccountAndDevice]);

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
    // of the entry. Verify the dialog disappears.
    moveToAccountDialog.$.moveButton.click();
    const movedId = await passwordManager.whenCalled('movePasswordsToAccount');
    assertEquals(passwordOnAccountAndDevice.id, movedId[0]);
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
    const deviceEntry =
        createPasswordEntry({url: 'goo.gl', username: 'bart', id: 42});
    const moveMultipleDialog =
        elementFactory.createMoveMultiplePasswordsDialog([deviceEntry]);
    assertTrue(moveMultipleDialog.$.dialog.open);
    moveMultipleDialog.$.cancelButton.click();
    flush();
    assertFalse(moveMultipleDialog.$.dialog.open);
  });

  test('moveMultiplePasswordsDialogFiresCloseEventWhenCanceled', function() {
    const deviceEntry =
        createPasswordEntry({url: 'goo.gl', username: 'bart', id: 42});
    const moveMultipleDialog =
        elementFactory.createMoveMultiplePasswordsDialog([deviceEntry]);
    moveMultipleDialog.$.cancelButton.click();
    return eventToPromise('close', moveMultipleDialog);
  });

  // Testing moving multiple password dialog Move button.
  test('moveMultiplePasswordsDialogMoveButton', async function() {
    const deviceEntry1 =
        createPasswordEntry({url: 'goo.gl', username: 'bart1', id: 41});
    const deviceEntry2 =
        createPasswordEntry({url: 'goo.gl', username: 'bart2', id: 54});
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
    assertEquals(deviceEntry2.id, movedIds[0]);
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
            const deviceEntry =
                createPasswordEntry({url: 'goo.gl', username: 'bart', id: 42});
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
            {username: 'device', id: 0, inProfileStore: true});
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
          inProfileStore: true,
        });
        const accountPassword = createPasswordEntry({
          url: 'www.test.com',
          username: 'username',
          id: 1,
          inAccountStore: true,
        });

        const passwordsDeviceSection = await createPasswordsDeviceSection(
            syncBrowserProxy, passwordManager,
            [devicePassword, accountPassword]);

        assertTrue(passwordsDeviceSection.$.moveMultiplePasswordsBanner.hidden);
      });
});
