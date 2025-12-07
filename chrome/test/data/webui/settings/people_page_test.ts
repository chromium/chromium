// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import type {SettingsPeoplePageElement} from 'chrome://settings/settings.js';
import {ProfileInfoBrowserProxyImpl, resetRouterForTesting, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {simulateSyncStatus} from './sync_test_util.js';
import {TestProfileInfoBrowserProxy} from './test_profile_info_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// <if expr="not is_chromeos">
import {listenOnce} from 'chrome://resources/js/util.js';
import type {CrCheckboxElement} from 'chrome://settings/lazy_load.js';
import {assertLT} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import type {StoredAccount} from 'chrome://settings/settings.js';

import {simulateStoredAccounts} from './sync_test_util.js';
// </if>

// clang-format on

let peoplePage: SettingsPeoplePageElement;
let profileInfoBrowserProxy: TestProfileInfoBrowserProxy;
let syncBrowserProxy: TestSyncBrowserProxy;

function reset() {
  peoplePage.remove();
  loadTimeData.overrideValues({
    signinAllowed: true,
    replaceSyncPromosWithSignInPromos: false,
  });
  resetRouterForTesting();
  Router.getInstance().navigateTo(routes.BASIC);
}

suite('ProfileInfoTests', function() {
  suiteSetup(function() {
    // <if expr="is_chromeos">
    loadTimeData.overrideValues({
      // Account Manager is tested in people_page_test_cros.js
      isAccountManagerEnabled: false,
    });
    // </if>
  });

  setup(async function() {
    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);

    await syncBrowserProxy.whenCalled('getSyncStatus');
    await profileInfoBrowserProxy.whenCalled('getProfileInfo');
    flush();
  });

  teardown(function() {
    reset();
  });

  test('GetProfileInfo', function() {
    assertEquals(
        profileInfoBrowserProxy.fakeProfileInfo.name,
        peoplePage.shadowRoot!.querySelector<HTMLElement>(
                                  '#profile-name')!.textContent.trim());
    const bg =
        peoplePage.shadowRoot!.querySelector<HTMLElement>(
                                  '#profile-icon')!.style.backgroundImage;
    assertTrue(bg.includes(profileInfoBrowserProxy.fakeProfileInfo.iconUrl));

    const iconDataUrl = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA' +
        'LAAAAAABAAEAAAICTAEAOw==';
    webUIListenerCallback(
        'profile-info-changed', {name: 'pushedName', iconUrl: iconDataUrl});

    flush();
    assertEquals(
        'pushedName',
        peoplePage.shadowRoot!.querySelector<HTMLElement>(
                                  '#profile-name')!.textContent.trim());
    const newBg =
        peoplePage.shadowRoot!.querySelector<HTMLElement>(
                                  '#profile-icon')!.style.backgroundImage;
    assertTrue(newBg.includes(iconDataUrl));
  });
});

// <if expr="not is_chromeos">
suite('SigninDisallowedTests', function() {
  setup(function() {
    loadTimeData.overrideValues({signinAllowed: false});

    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);
  });

  teardown(function() {
    reset();
  });

  test('ShowCorrectRows', async function() {
    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();

    // The correct /manageProfile link row is shown.
    assertFalse(!!peoplePage.shadowRoot!.querySelector('#edit-profile'));
    assertTrue(!!peoplePage.shadowRoot!.querySelector('#profile-row'));

    // Control element doesn't exist when policy forbids sync.
    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
      syncSystemEnabled: true,
      statusAction: StatusAction.NO_ACTION,
    });
    assertFalse(!!peoplePage.shadowRoot!.querySelector(
        'settings-sync-account-control'));
  });
});

suite('SyncStatusTests', function() {
  setup(function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);
  });

  teardown(function() {
    reset();
  });

  test('Toast', function() {
    assertFalse(peoplePage.$.toast.open);
    webUIListenerCallback('sync-settings-saved');
    assertTrue(peoplePage.$.toast.open);
  });

  test('ShowCorrectRows', async function() {
    await syncBrowserProxy.whenCalled('getSyncStatus');
    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
      syncSystemEnabled: true,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();

    // The correct /manageProfile link row is shown.
    assertTrue(!!peoplePage.shadowRoot!.querySelector('#edit-profile'));
    assertFalse(!!peoplePage.shadowRoot!.querySelector('#profile-row'));

    // The control element should exist when policy allows.
    assertTrue(isChildVisible(peoplePage, 'settings-sync-account-control'));

    // Control element doesn't exist when policy forbids sync.
    simulateSyncStatus({
      syncSystemEnabled: false,
      statusAction: StatusAction.NO_ACTION,
    });
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));

    // Do not show Google Account when sync status could not be retrieved.
    simulateStoredAccounts([]);
    simulateSyncStatus(undefined);
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));

    simulateStoredAccounts([]);
    simulateSyncStatus({
      statusAction: StatusAction.NO_ACTION,
    });
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));

    simulateStoredAccounts([]);
    simulateSyncStatus({
      statusAction: StatusAction.NO_ACTION,
    });
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));

    // A stored account with sync off but no error should result in the
    // Google Account being shown.
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
      hasError: false,
      statusAction: StatusAction.NO_ACTION,
    });
    assertTrue(isChildVisible(peoplePage, '#manage-google-account'));

    // A stored account with sync off and error should not result in the
    // Google Account being shown.
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
      hasError: true,
      statusAction: StatusAction.NO_ACTION,
    });
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));

    // A stored account with sync on but no error should result in the
    // Google Account being shown.
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      hasError: false,
      statusAction: StatusAction.NO_ACTION,
    });
    assertTrue(isChildVisible(peoplePage, '#manage-google-account'));

    // A stored account with sync on but with error should not result in
    // the Google Account being shown.
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      hasError: true,
      statusAction: StatusAction.NO_ACTION,
    });
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));
  });

  test('SignOutNavigationNormalProfile', async function() {
    // Navigate to chrome://settings/signOut
    Router.getInstance().navigateTo(routes.SIGN_OUT);

    await flushTasks();
    const signoutDialog =
        peoplePage.shadowRoot!.querySelector('settings-signout-dialog')!;
    assertTrue(signoutDialog.$.dialog.open);
    const deleteProfileCheckbox =
        signoutDialog.shadowRoot!.querySelector<CrCheckboxElement>(
            '#deleteProfile');
    assertTrue(!!deleteProfileCheckbox);
    assertFalse(deleteProfileCheckbox.hidden);

    assertLT(0, deleteProfileCheckbox.clientHeight);

    const disconnectConfirm = signoutDialog.$.disconnectConfirm;
    assertTrue(!!disconnectConfirm);
    assertFalse(disconnectConfirm.hidden);

    disconnectConfirm.click();

    await new Promise(function(resolve) {
      listenOnce(window, 'popstate', resolve);
    });
    const deleteProfile = await syncBrowserProxy.whenCalled('signOut');
    assertFalse(deleteProfile);
  });

  test('SignOutDialogManagedProfileTurnOffSync', async function() {
    let accountControl = null;
    await syncBrowserProxy.whenCalled('getSyncStatus');
    simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      domain: 'example.com',
      syncSystemEnabled: true,
      statusAction: StatusAction.NO_ACTION,
    });

    assertFalse(!!peoplePage.shadowRoot!.querySelector('#dialog'));
    accountControl =
        peoplePage.shadowRoot!.querySelector('settings-sync-account-control')!;
    await waitBeforeNextRender(accountControl);
    const turnOffButton =
        accountControl.shadowRoot!.querySelector<HTMLElement>('#turn-off')!;
    turnOffButton.click();
    flush();

    await flushTasks();
    const signoutDialog =
        peoplePage.shadowRoot!.querySelector('settings-signout-dialog')!;
    assertTrue(signoutDialog.$.dialog.open);
    assertTrue(!!signoutDialog.shadowRoot!.querySelector('#deleteProfile'));

    const disconnectConfirm =
        signoutDialog.shadowRoot!.querySelector<HTMLElement>(
            '#disconnectConfirm');
    assertTrue(!!disconnectConfirm);
    assertFalse(disconnectConfirm.hidden);

    syncBrowserProxy.resetResolver('signOut');

    disconnectConfirm.click();

    await new Promise(function(resolve) {
      listenOnce(window, 'popstate', resolve);
    });
    const deleteProfile = await syncBrowserProxy.whenCalled('signOut');
    assertFalse(deleteProfile);
  });

  test('getProfileStatsCount', async function() {
    // Navigate to chrome://settings/signOut
    Router.getInstance().navigateTo(routes.SIGN_OUT);

    await flushTasks();
    const signoutDialog =
        peoplePage.shadowRoot!.querySelector('settings-signout-dialog')!;
    assertTrue(signoutDialog.$.dialog.open);

    // Assert the warning message is as expected.
    const warningMessage = signoutDialog.shadowRoot!.querySelector<HTMLElement>(
        '.delete-profile-warning')!;

    webUIListenerCallback('profile-stats-count-ready', 0);
    assertEquals(
        loadTimeData.getStringF(
            'deleteProfileWarningWithoutCounts', 'fakeUsername'),
        warningMessage.textContent.trim());

    webUIListenerCallback('profile-stats-count-ready', 1);
    assertEquals(
        loadTimeData.getStringF(
            'deleteProfileWarningWithCountsSingular', 'fakeUsername'),
        warningMessage.textContent.trim());

    webUIListenerCallback('profile-stats-count-ready', 2);
    assertEquals(
        loadTimeData.getStringF(
            'deleteProfileWarningWithCountsPlural', 2, 'fakeUsername'),
        warningMessage.textContent.trim());

    // Close the disconnect dialog.
    signoutDialog.$.disconnectConfirm.click();
    await new Promise(function(resolve) {
      listenOnce(window, 'popstate', resolve);
    });
  });

  test('NavigateDirectlyToSignOutURL', async function() {
    // Navigate to chrome://settings/signOut
    Router.getInstance().navigateTo(routes.SIGN_OUT);

    await flushTasks();
    assertTrue(
        peoplePage.shadowRoot!.querySelector(
                                  'settings-signout-dialog')!.$.dialog.open);
    await profileInfoBrowserProxy.whenCalled('getProfileStatsCount');
    // 'getProfileStatsCount' can be the first message sent to the
    // handler if the user navigates directly to
    // chrome://settings/signOut. if so, it should not cause a crash.
    new ProfileInfoBrowserProxyImpl().getProfileStatsCount();

    // Close the disconnect dialog.
    peoplePage.shadowRoot!.querySelector('settings-signout-dialog')!.$
        .disconnectConfirm.click();
    await new Promise(function(resolve) {
      listenOnce(window, 'popstate', resolve);
    });
  });

  test('Signout dialog suppressed when not signed in', async function() {
    await syncBrowserProxy.whenCalled('getSyncStatus');
    Router.getInstance().navigateTo(routes.SIGN_OUT);
    await flushTasks();
    assertTrue(
        peoplePage.shadowRoot!.querySelector(
                                  'settings-signout-dialog')!.$.dialog.open);

    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
    });

    await new Promise(function(resolve) {
      listenOnce(window, 'popstate', resolve);
    });

    Router.getInstance().navigateTo(routes.SIGN_OUT);

    await new Promise(function(resolve) {
      listenOnce(window, 'popstate', resolve);
    });
  });
});
// </if>

suite('SyncSettings', function() {
  setup(async function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);

    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();
  });

  teardown(function() {
    reset();
  });

  test('ShowCorrectSyncRow', function() {
    assertTrue(isChildVisible(peoplePage, '#sync-setup'));
    assertFalse(isChildVisible(peoplePage, '#sync-status'));
    assertFalse(isChildVisible(peoplePage, '#google-services'));

    // Make sures the subpage opens even when logged out or has errors.
    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.REAUTHENTICATE,
    });

    peoplePage.shadowRoot!.querySelector<HTMLElement>('#sync-setup')!.click();
    flush();

    assertEquals(Router.getInstance().getCurrentRoute(), routes.SYNC);
  });
});

// <if expr="not is_chromeos">
suite('PeoplePageAccountSettings', function() {
  setup(function() {
    loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: true});
    resetRouterForTesting();
    Router.getInstance().navigateTo(routes.PEOPLE);

    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);
  });

  teardown(function() {
    reset();
  });

  async function simulateSignedInState(
      state: SignedInState, accounts: StoredAccount[]) {
    await syncBrowserProxy.whenCalled('getSyncStatus');
    simulateSyncStatus({
      signedInState: state,
      syncSystemEnabled: true,
      hasError: false,
      statusAction: StatusAction.NO_ACTION,
    });
    await flush();

    await syncBrowserProxy.whenCalled('getStoredAccounts');
    simulateStoredAccounts(accounts);
    return flush();
  }

  test('ShowCorrectRowsSignedIn', async function() {
    await simulateSignedInState(
        SignedInState.SIGNED_IN, [{email: 'foo@foo.com'}]);

    // The account card and the profile should not exist. Instead, there is a
    // link row which leads to the account settings page.
    assertFalse(isChildVisible(peoplePage, 'settings-sync-account-control'));
    assertFalse(isChildVisible(peoplePage, '#profile-row'));
    assertTrue(isChildVisible(peoplePage, '#account-subpage-row'));

    // There is a link to the Google services, not to the sync settings.
    assertTrue(isChildVisible(peoplePage, '#google-services'));
    assertFalse(isChildVisible(peoplePage, '#sync-setup'));

    // The other rows are shown/hidden correctly.
    assertTrue(isChildVisible(peoplePage, '#edit-profile'));
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));
    assertTrue(isChildVisible(peoplePage, '#importDataDialogTrigger'));
  });

  test('ShowCorrectRowsSignedOut', async function() {
    await simulateSignedInState(SignedInState.SIGNED_OUT, []);

    // The first item should be an account card.
    assertTrue(isChildVisible(peoplePage, 'settings-sync-account-control'));
    assertFalse(isChildVisible(peoplePage, '#profile-row'));
    assertFalse(isChildVisible(peoplePage, '#account-subpage-row'));

    // There is a link to the Google services, not to the sync settings.
    assertTrue(isChildVisible(peoplePage, '#google-services'));
    assertFalse(isChildVisible(peoplePage, '#sync-setup'));

    // The other rows are shown/hidden correctly.
    assertTrue(isChildVisible(peoplePage, '#edit-profile'));
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));
    assertTrue(isChildVisible(peoplePage, '#importDataDialogTrigger'));
  });

  test('ShowCorrectRowsSyncing', async function() {
    await simulateSignedInState(
        SignedInState.SYNCING, [{email: 'foo@foo.com'}]);

    // The first item should be an account card.
    assertTrue(isChildVisible(peoplePage, 'settings-sync-account-control'));
    assertFalse(isChildVisible(peoplePage, '#profile-row'));
    assertFalse(isChildVisible(peoplePage, '#account-subpage-row'));

    // There is a link to the sync settings, not to the Google services.
    assertFalse(isChildVisible(peoplePage, '#google-services'));
    assertTrue(isChildVisible(peoplePage, '#sync-setup'));

    // The other rows are shown/hidden correctly.
    assertTrue(isChildVisible(peoplePage, '#edit-profile'));
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));
    assertTrue(isChildVisible(peoplePage, '#importDataDialogTrigger'));
  });

  test('ShowCorrectRowsSignInPending', async function() {
    await simulateSignedInState(
        SignedInState.SIGNED_IN_PAUSED, [{email: 'foo@foo.com'}]);

    assertTrue(isChildVisible(peoplePage, 'settings-sync-account-control'));
    assertFalse(isChildVisible(peoplePage, '#profile-row'));
    assertFalse(isChildVisible(peoplePage, '#account-subpage-row'));

    // There is a link to the Google services, not to the sync settings.
    assertTrue(isChildVisible(peoplePage, '#google-services'));
    assertFalse(isChildVisible(peoplePage, '#sync-setup'));

    // The other rows are shown/hidden correctly.
    assertTrue(isChildVisible(peoplePage, '#edit-profile'));
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));
    assertTrue(isChildVisible(peoplePage, '#importDataDialogTrigger'));
  });

  test('ShowCorrectRowsWebSignedIn', async function() {
    await simulateSignedInState(
        SignedInState.WEB_ONLY_SIGNED_IN, [{email: 'foo@foo.com'}]);

    // The first item should be an account card.
    assertTrue(isChildVisible(peoplePage, 'settings-sync-account-control'));

    // There is a link to the Google services, not to the sync settings.
    assertTrue(isChildVisible(peoplePage, '#google-services'));
    assertFalse(isChildVisible(peoplePage, '#sync-setup'));

    // The other rows are shown/hidden correctly.
    assertTrue(isChildVisible(peoplePage, '#edit-profile'));
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));
    assertTrue(isChildVisible(peoplePage, '#importDataDialogTrigger'));
  });

  test('ClickingAccountLinkRowLeadsToAccountSettings', async function() {
    await simulateSignedInState(
        SignedInState.SIGNED_IN, [{email: 'foo@foo.com'}]);

    peoplePage.shadowRoot!.querySelector<HTMLElement>(
                              '#account-subpage-row')!.click();
    assertEquals(routes.ACCOUNT, Router.getInstance().getCurrentRoute());
  });

  test('ClickingGoogleServicesLeadsToGoogleServicesPage', async function() {
    await simulateSignedInState(SignedInState.SIGNED_OUT, []);

    peoplePage.shadowRoot!.querySelector<HTMLElement>(
                              '#google-services')!.click();
    assertEquals(
        routes.GOOGLE_SERVICES, Router.getInstance().getCurrentRoute());
  });

  test('AccountLinkRowHasAccountInfo', async function() {
    const expectedAccount = {
      fullName: 'Test Name',
      email: 'test@email.com',
      avatarImage: 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAA' +
          'AAABAAEAAAICTAEAOw==',
    };
    await simulateSignedInState(SignedInState.SIGNED_IN, [expectedAccount]);

    const accountName =
        peoplePage.shadowRoot!.querySelector(
                                  '#account-name')!.textContent.trim();
    const accountEmail =
        peoplePage.shadowRoot!.querySelector(
                                  '#account-subtitle')!.textContent.trim();

    assertEquals(expectedAccount.fullName, accountName);
    assertEquals(expectedAccount.email, accountEmail);

    const bgImage =
        peoplePage.shadowRoot!.querySelector<HTMLElement>(
                                  '#profile-icon')!.style.backgroundImage;
    assertTrue(bgImage.includes(expectedAccount.avatarImage));
  });

  test('AccountRowSubtitleUpdatedForPassphraseError', async function() {
    const testEmail = 'test@email.com';
    await simulateSignedInState(SignedInState.SIGNED_IN, [{email: testEmail}]);

    // First, it shows the user's email.
    const accountSubtitle =
        peoplePage.shadowRoot!.querySelector('#account-subtitle')!;
    assertEquals(testEmail, accountSubtitle.textContent.trim());

    // When the passphrase needs to be entered, a message is displayed instead.
    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.ENTER_PASSPHRASE,
      statusText: 'Enter the passphrase for $1',
    });
    assertEquals(
        loadTimeData.substituteString(
            peoplePage.syncStatus!.statusText!, testEmail),
        accountSubtitle.textContent.trim());
  });

  test('RecordSigninPendingOfferedMetrics', async function() {
    syncBrowserProxy.resetResolver('recordSigninPendingOffered');

    // Signin pending offered recorded.
    await simulateSignedInState(
        SignedInState.SIGNED_IN_PAUSED, [{email: 'foo@foo.com'}]);
    assertEquals(
        1, syncBrowserProxy.getCallCount('recordSigninPendingOffered'));

    // Firing the same signin state again doesn't record twice.
    await simulateSignedInState(
        SignedInState.SIGNED_IN_PAUSED, [{email: 'foo@foo.com'}]);
    assertEquals(
        1, syncBrowserProxy.getCallCount('recordSigninPendingOffered'));

    // Nothing recorded when signing in.
    await simulateSignedInState(
        SignedInState.SIGNED_IN, [{email: 'foo@foo.com'}]);
    assertEquals(
        1, syncBrowserProxy.getCallCount('recordSigninPendingOffered'));

    // After getting in pending state again, the metric is recorded.
    await simulateSignedInState(
        SignedInState.SIGNED_IN_PAUSED, [{email: 'foo@foo.com'}]);
    assertEquals(
        2, syncBrowserProxy.getCallCount('recordSigninPendingOffered'));
  });
});
// </if>
