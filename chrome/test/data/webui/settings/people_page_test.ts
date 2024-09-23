// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
// <if expr="not is_chromeos">
import {listenOnce} from 'chrome://resources/js/util.js';
// </if>

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// <if expr="not is_chromeos">
import type {CrCheckboxElement} from 'chrome://settings/lazy_load.js';
// </if>

// <if expr="not chromeos_lacros">
import {loadTimeData} from 'chrome://settings/settings.js';
// </if>

import type {SettingsPeoplePageElement} from 'chrome://settings/settings.js';
import {pageVisibility, ProfileInfoBrowserProxyImpl, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
// <if expr="not is_chromeos">
import {assertLT} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

// </if>

import {simulateSyncStatus} from './sync_test_util.js';
// <if expr="not is_chromeos">
import {simulateStoredAccounts} from './sync_test_util.js';
// </if>

import {TestProfileInfoBrowserProxy} from './test_profile_info_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

let peoplePage: SettingsPeoplePageElement;
let profileInfoBrowserProxy: TestProfileInfoBrowserProxy;
let syncBrowserProxy: TestSyncBrowserProxy;

suite('ProfileInfoTests', function() {
  suiteSetup(function() {
    // <if expr="chromeos_ash">
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
    peoplePage.pageVisibility = pageVisibility || {};
    document.body.appendChild(peoplePage);

    await syncBrowserProxy.whenCalled('getSyncStatus');
    await profileInfoBrowserProxy.whenCalled('getProfileInfo');
    flush();
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('GetProfileInfo', function() {
    assertEquals(
        profileInfoBrowserProxy.fakeProfileInfo.name,
        peoplePage.shadowRoot!.querySelector<HTMLElement>(
                                  '#profile-name')!.textContent!.trim());
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
                                  '#profile-name')!.textContent!.trim());
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
    peoplePage.pageVisibility = pageVisibility || {};
    document.body.appendChild(peoplePage);
  });

  teardown(function() {
    peoplePage.remove();
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
  setup(async function() {
    loadTimeData.overrideValues({
      signinAllowed: true,
      turnOffSyncAllowedForManagedProfiles: false,
    });
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    peoplePage.pageVisibility = pageVisibility || {};
    document.body.appendChild(peoplePage);
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('Toast', function() {
    assertFalse(peoplePage.$.toast.open);
    webUIListenerCallback('sync-settings-saved');
    assertTrue(peoplePage.$.toast.open);
  });

  test('ShowCorrectRows', async function() {
    await syncBrowserProxy.whenCalled('getSyncStatus');
    simulateSyncStatus({
      syncSystemEnabled: true,
      statusAction: StatusAction.NO_ACTION,
    });
    flush();

    // The correct /manageProfile link row is shown.
    assertTrue(!!peoplePage.shadowRoot!.querySelector('#edit-profile'));
    assertFalse(!!peoplePage.shadowRoot!.querySelector('#profile-row'));

    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
      syncSystemEnabled: true,
      statusAction: StatusAction.NO_ACTION,
    });

    // The control element should exist when policy allows.
    const accountControl =
        peoplePage.shadowRoot!.querySelector('settings-sync-account-control')!;
    assertTrue(window.getComputedStyle(accountControl)['display'] !== 'none');

    // Control element doesn't exist when policy forbids sync.
    simulateSyncStatus({
      syncSystemEnabled: false,
      statusAction: StatusAction.NO_ACTION,
    });
    assertEquals('none', window.getComputedStyle(accountControl)['display']);

    const manageGoogleAccount =
        peoplePage.shadowRoot!.querySelector('#manage-google-account')!;

    // Do not show Google Account when stored accounts or sync status
    // could not be retrieved.
    simulateStoredAccounts(undefined);
    simulateSyncStatus(undefined);
    assertEquals(
        'none', window.getComputedStyle(manageGoogleAccount)['display']);

    simulateStoredAccounts([]);
    simulateSyncStatus(undefined);
    assertEquals(
        'none', window.getComputedStyle(manageGoogleAccount)['display']);

    simulateStoredAccounts(undefined);
    simulateSyncStatus({
      statusAction: StatusAction.NO_ACTION,
    });
    assertEquals(
        'none', window.getComputedStyle(manageGoogleAccount)['display']);

    simulateStoredAccounts([]);
    simulateSyncStatus({
      statusAction: StatusAction.NO_ACTION,
    });
    assertEquals(
        'none', window.getComputedStyle(manageGoogleAccount)['display']);

    // A stored account with sync off but no error should result in the
    // Google Account being shown.
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
      hasError: false,
      statusAction: StatusAction.NO_ACTION,
    });
    assertTrue(
        window.getComputedStyle(manageGoogleAccount)['display'] !== 'none');

    // A stored account with sync off and error should not result in the
    // Google Account being shown.
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
      hasError: true,
      statusAction: StatusAction.NO_ACTION,
    });
    assertEquals(
        'none', window.getComputedStyle(manageGoogleAccount)['display']);

    // A stored account with sync on but no error should result in the
    // Google Account being shown.
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      hasError: false,
      statusAction: StatusAction.NO_ACTION,
    });
    assertTrue(
        window.getComputedStyle(manageGoogleAccount)['display'] !== 'none');

    // A stored account with sync on but with error should not result in
    // the Google Account being shown.
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      hasError: true,
      statusAction: StatusAction.NO_ACTION,
    });
    assertEquals(
        'none', window.getComputedStyle(manageGoogleAccount)['display']);
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
    assertFalse(deleteProfileCheckbox!.hidden);

    assertLT(0, deleteProfileCheckbox!.clientHeight);

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

  // <if expr="chromeos_lacros">
  test('SignoutDialogLacrosMainProfile', async function() {
    loadTimeData.overrideValues({
      isSecondaryUser: false,
    });
    // Navigate to chrome://settings/signOut
    Router.getInstance().navigateTo(routes.SIGN_OUT);

    await flushTasks();
    const signoutDialog =
        peoplePage.shadowRoot!.querySelector('settings-signout-dialog')!;
    assertTrue(signoutDialog.$.dialog.open);
    // Delete profile is not allowed for Lacros main profile.
    assertFalse(!!signoutDialog.shadowRoot!.querySelector('#deleteProfile'));
  });
  // </if>

  test('SignOutDialogManagedProfileTurnOffSyncDisallowed', async function() {
    let accountControl = null;
    await syncBrowserProxy.whenCalled('getSyncStatus');
    loadTimeData.overrideValues({
      turnOffSyncAllowedForManagedProfiles: false,
    });
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
    assertFalse(!!signoutDialog.shadowRoot!.querySelector('#deleteProfile'));

    const disconnectManagedProfileConfirm =
        signoutDialog.shadowRoot!.querySelector<HTMLElement>(
            '#disconnectManagedProfileConfirm');
    assertTrue(!!disconnectManagedProfileConfirm);
    assertFalse(disconnectManagedProfileConfirm!.hidden);

    syncBrowserProxy.resetResolver('signOut');

    disconnectManagedProfileConfirm!.click();

    await new Promise(function(resolve) {
      listenOnce(window, 'popstate', resolve);
    });
    const deleteProfile = await syncBrowserProxy.whenCalled('signOut');
    assertTrue(deleteProfile);
  });

  test('SignOutDialogManagedProfileTurnOffSyncAllowed', async function() {
    let accountControl = null;
    await syncBrowserProxy.whenCalled('getSyncStatus');
    loadTimeData.overrideValues({
      turnOffSyncAllowedForManagedProfiles: true,
    });
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
    assertFalse(disconnectConfirm!.hidden);

    syncBrowserProxy.resetResolver('signOut');

    disconnectConfirm!.click();

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
        warningMessage.textContent!.trim());

    webUIListenerCallback('profile-stats-count-ready', 1);
    assertEquals(
        loadTimeData.getStringF(
            'deleteProfileWarningWithCountsSingular', 'fakeUsername'),
        warningMessage.textContent!.trim());

    webUIListenerCallback('profile-stats-count-ready', 2);
    assertEquals(
        loadTimeData.getStringF(
            'deleteProfileWarningWithCountsPlural', 2, 'fakeUsername'),
        warningMessage.textContent!.trim());

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
    peoplePage.pageVisibility = pageVisibility || {};
    document.body.appendChild(peoplePage);

    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('ShowCorrectSyncRow', function() {
    assertTrue(!!peoplePage.shadowRoot!.querySelector('#sync-setup'));
    assertFalse(!!peoplePage.shadowRoot!.querySelector('#sync-status'));

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
