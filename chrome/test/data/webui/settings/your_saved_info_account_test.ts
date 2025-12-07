// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData, ProfileInfoBrowserProxyImpl, resetRouterForTesting, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
// clang-format off
// <if expr="not is_chromeos">
import {ChromeSigninAccessPoint, Router, routes, SignedInState, StatusAction} from 'chrome://settings/settings.js';
import {assertFalse} from 'chrome://webui-test/chai_assert.js';
// </if>
// clang-format on

import {TestProfileInfoBrowserProxy} from './test_profile_info_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

const ICON_DATA_URL = 'data:image/gif;base64,' +
    +'R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==';

suite('YourSavedInfoAccount', function() {
  let accountCardElement: HTMLElement;
  let profileInfoBrowserProxy: TestProfileInfoBrowserProxy;
  let syncBrowserProxy: TestSyncBrowserProxy;

  setup(async function() {
    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    createAccountCardElement();

    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();
  });

  function createAccountCardElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    accountCardElement = document.createElement('settings-account-card');
    document.body.appendChild(accountCardElement);
  }

  // <if expr="not is_chromeos">
  async function setupSync(syncStatus: any) {
    syncBrowserProxy.testSyncStatus = syncStatus;
    webUIListenerCallback('sync-status-changed', syncStatus);
    flush();
    await waitBeforeNextRender(accountCardElement);
  }

  test('displaysSyncingAccountInfo', async function() {
    syncBrowserProxy.storedAccounts = [{
      fullName: 'test name',
      givenName: 'test',
      email: 'test@gmail.com',
      avatarImage: ICON_DATA_URL,
      isPrimaryAccount: true,
    }];
    await setupSync({
      syncSystemEnabled: true,
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'test@gmail.com',
      statusAction: StatusAction.NO_ACTION,
      hasError: false,
    });

    assertTrue(
        isChildVisible(accountCardElement, 'settings-sync-account-control'));
    const syncControl =
        accountCardElement.shadowRoot!.querySelector<HTMLElement>(
            'settings-sync-account-control')!;

    // The sync-account-control will fetch stored accounts.
    await syncBrowserProxy.whenCalled('getStoredAccounts');
    webUIListenerCallback(
        'stored-accounts-updated', syncBrowserProxy.storedAccounts);
    flush();
    await waitBeforeNextRender(syncControl);

    // Check that the avatar row is visible and displays correct info.
    const avatarRow = syncControl.shadowRoot!.querySelector('#avatar-row')!;
    assertTrue(!!avatarRow);
    assertTrue(isChildVisible(syncControl, '#avatar-row'));
    const accountName =
        avatarRow.querySelector<HTMLElement>('#user-info > .text-elide')!;
    assertTrue(!!accountName);
    assertTrue(isChildVisible(syncControl, '#user-info'));
    assertEquals('test name', accountName.textContent.trim());
  });

  test('displaysAccountLinkRow', async function() {
    loadTimeData.overrideValues({
      replaceSyncPromosWithSignInPromos: true,
    });
    resetRouterForTesting();
    // Re-create the element to apply the new loadTimeData.
    createAccountCardElement();

    syncBrowserProxy.storedAccounts = [{
      fullName: 'test name',
      givenName: 'test',
      email: 'test@gmail.com',
      avatarImage: ICON_DATA_URL,
      isPrimaryAccount: true,
    }];
    await syncBrowserProxy.whenCalled('getSyncStatus');
    await setupSync({
      syncSystemEnabled: true,
      signedInState: SignedInState.SIGNED_IN,
      signedInUsername: 'test@gmail.com',
      statusAction: StatusAction.NO_ACTION,
      hasError: false,
    });

    await syncBrowserProxy.whenCalled('getStoredAccounts');
    webUIListenerCallback(
        'stored-accounts-updated', syncBrowserProxy.storedAccounts);
    await flush();

    assertFalse(
        isChildVisible(accountCardElement, 'settings-sync-account-control'));
    assertTrue(isChildVisible(accountCardElement, '#account-subpage-row'));

    const accountName =
        accountCardElement.shadowRoot!.querySelector(
                                          '#account-name')!.textContent.trim();
    assertEquals('test name', accountName);

    // Clicking the account row navigates to the account settings page.
    const accountRow =
        accountCardElement.shadowRoot!.querySelector<HTMLElement>(
            '#account-subpage-row')!;
    accountRow.click();
    assertEquals(routes.ACCOUNT, Router.getInstance().getCurrentRoute());
  });

  test('cardHiddenIfSigninNotAllowed', async function() {
    loadTimeData.overrideValues({
      signinAllowed: false,
    });
    resetRouterForTesting();
    // Re-create the element to apply the new loadTimeData.
    createAccountCardElement();
    await flush();

    assertFalse(isChildVisible(accountCardElement, '#account-card'));
  });

  test('signInRecordsCorrectMetric', async function() {
    loadTimeData.overrideValues({
      signinAllowed: true,
    });
    resetRouterForTesting();
    createAccountCardElement();
    syncBrowserProxy.storedAccounts = [];
    await syncBrowserProxy.whenCalled('getSyncStatus');
    await setupSync({
      syncSystemEnabled: true,
      signinAllowed: true,
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
      hasError: false,
    });

    assertTrue(
        isChildVisible(accountCardElement, 'settings-sync-account-control'),
        'sync-account-control should be visible');
    const syncControl =
        accountCardElement.shadowRoot!.querySelector<HTMLElement>(
            'settings-sync-account-control')!;

    assertTrue(
        isChildVisible(syncControl, '#signIn'),
        'Sign-in button should be visible');
    const signInButton =
        syncControl.shadowRoot!.querySelector<HTMLElement>('#signIn')!;
    signInButton.click();

    const accessPoint = await syncBrowserProxy.whenCalled('startSignIn');
    assertEquals(
        ChromeSigninAccessPoint.SETTINGS_YOUR_SAVED_INFO, accessPoint,
        'Sign-in should be called with SettingsYourSavedInfo Access point');
  });
  // </if>

  // <if expr="is_chromeos">
  test('displaysProfileNameOnChromeOS', async function() {
    loadTimeData.overrideValues({
      isAccountManagerEnabled: false,
    });
    resetRouterForTesting();
    // Re-create the element to apply the new loadTimeData.
    createAccountCardElement();
    await flush();
    await profileInfoBrowserProxy.whenCalled('getProfileInfo');

    assertEquals(
        profileInfoBrowserProxy.fakeProfileInfo.name,
        accountCardElement.shadowRoot!
            .querySelector<HTMLElement>('#profile-name')!.textContent.trim());

    // Update profile info and check again.
    webUIListenerCallback(
        'profile-info-changed', {name: 'pushedName', iconUrl: ICON_DATA_URL});
    await flush();
    await waitBeforeNextRender(accountCardElement);

    assertEquals(
        'pushedName',
        accountCardElement.shadowRoot!
            .querySelector<HTMLElement>('#profile-name')!.textContent.trim());
    assertTrue(isChildVisible(accountCardElement, '#profile-name'));
  });
  // </if>
});
