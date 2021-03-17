// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {isChromeOS, webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {listenOnce} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {pageVisibility, ProfileInfoBrowserProxyImpl, Router, routes, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {simulateStoredAccounts, simulateSyncStatus} from 'chrome://test/settings/sync_test_util.js';
import {TestProfileInfoBrowserProxy} from 'chrome://test/settings/test_profile_info_browser_proxy.js';
import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {waitBeforeNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

/** @implements {settings.PeopleBrowserProxy} */
class TestPeopleBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'openURL',
    ]);
  }

  /** @override */
  openURL(url) {
    this.methodCalled('openURL', url);
  }
}

/** @type {?SettingsPeoplePageElement} */
let peoplePage = null;
/** @type {?ProfileInfoBrowserProxy} */
let profileInfoBrowserProxy = null;
/** @type {?SyncBrowserProxy} */
let syncBrowserProxy = null;

suite('ProfileInfoTests', function() {
  suiteSetup(function() {
    if (isChromeOS) {
      loadTimeData.overrideValues({
        // Account Manager is tested in the Chrome OS-specific section below.
        isAccountManagerEnabled: false,
      });
    }
  });

  setup(async function() {
    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;

    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

    PolymerTest.clearBody();
    peoplePage = document.createElement('settings-people-page');
    peoplePage.pageVisibility = pageVisibility;
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
        peoplePage.$$('#profile-name').textContent.trim());
    const bg = peoplePage.$$('#profile-icon').style.backgroundImage;
    assertTrue(bg.includes(profileInfoBrowserProxy.fakeProfileInfo.iconUrl));

    const iconDataUrl = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA' +
        'LAAAAAABAAEAAAICTAEAOw==';
    webUIListenerCallback(
        'profile-info-changed', {name: 'pushedName', iconUrl: iconDataUrl});

    flush();
    assertEquals(
        'pushedName', peoplePage.$$('#profile-name').textContent.trim());
    const newBg = peoplePage.$$('#profile-icon').style.backgroundImage;
    assertTrue(newBg.includes(iconDataUrl));
  });
});

if (!isChromeOS) {
  suite('SigninDisallowedTests', function() {
    setup(function() {
      loadTimeData.overrideValues({signinAllowed: false});

      syncBrowserProxy = new TestSyncBrowserProxy();
      SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

      profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
      ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;

      PolymerTest.clearBody();
      peoplePage = document.createElement('settings-people-page');
      peoplePage.pageVisibility = pageVisibility;
      document.body.appendChild(peoplePage);
    });

    teardown(function() {
      peoplePage.remove();
    });

    test('ShowCorrectRows', async function() {
      await syncBrowserProxy.whenCalled('getSyncStatus');
      flush();

      // The correct /manageProfile link row is shown.
      assertFalse(!!peoplePage.$$('#edit-profile'));
      assertTrue(!!peoplePage.$$('#profile-row'));

      // Control element doesn't exist when policy forbids sync.
      simulateSyncStatus({
        signedIn: false,
        syncSystemEnabled: true,
      });
      assertFalse(!!peoplePage.$$('settings-sync-account-control'));
    });
  });

  suite('SyncStatusTests', function() {
    setup(async function() {
      loadTimeData.overrideValues({signinAllowed: true});
      syncBrowserProxy = new TestSyncBrowserProxy();
      SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

      profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
      ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;

      PolymerTest.clearBody();
      peoplePage = document.createElement('settings-people-page');
      peoplePage.pageVisibility = pageVisibility;
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
      });
      flush();

      // The correct /manageProfile link row is shown.
      assertTrue(!!peoplePage.$$('#edit-profile'));
      assertFalse(!!peoplePage.$$('#profile-row'));

      simulateSyncStatus({
        signedIn: false,
        syncSystemEnabled: true,
      });

      // The control element should exist when policy allows.
      const accountControl = peoplePage.$$('settings-sync-account-control');
      assertTrue(window.getComputedStyle(accountControl)['display'] !== 'none');

      // Control element doesn't exist when policy forbids sync.
      simulateSyncStatus({
        syncSystemEnabled: false,
      });
      assertEquals('none', window.getComputedStyle(accountControl)['display']);

      const manageGoogleAccount = peoplePage.$$('#manage-google-account');

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
      simulateSyncStatus({});
      assertEquals(
          'none', window.getComputedStyle(manageGoogleAccount)['display']);

      simulateStoredAccounts([]);
      simulateSyncStatus({});
      assertEquals(
          'none', window.getComputedStyle(manageGoogleAccount)['display']);

      // A stored account with sync off but no error should result in the
      // Google Account being shown.
      simulateStoredAccounts([{email: 'foo@foo.com'}]);
      simulateSyncStatus({
        signedIn: false,
        hasError: false,
      });
      assertTrue(
          window.getComputedStyle(manageGoogleAccount)['display'] !== 'none');

      // A stored account with sync off and error should not result in the
      // Google Account being shown.
      simulateStoredAccounts([{email: 'foo@foo.com'}]);
      simulateSyncStatus({
        signedIn: false,
        hasError: true,
      });
      assertEquals(
          'none', window.getComputedStyle(manageGoogleAccount)['display']);

      // A stored account with sync on but no error should result in the
      // Google Account being shown.
      simulateStoredAccounts([{email: 'foo@foo.com'}]);
      simulateSyncStatus({
        signedIn: true,
        hasError: false,
      });
      assertTrue(
          window.getComputedStyle(manageGoogleAccount)['display'] !== 'none');

      // A stored account with sync on but with error should not result in
      // the Google Account being shown.
      simulateStoredAccounts([{email: 'foo@foo.com'}]);
      simulateSyncStatus({
        signedIn: true,
        hasError: true,
      });
      assertEquals(
          'none', window.getComputedStyle(manageGoogleAccount)['display']);
    });

    test('SignOutNavigationNormalProfile', async function() {
      // Navigate to chrome://settings/signOut
      Router.getInstance().navigateTo(routes.SIGN_OUT);

      await new Promise(function(resolve) {
        peoplePage.async(resolve);
      });
      const signoutDialog = peoplePage.$$('settings-signout-dialog');
      assertTrue(signoutDialog.$$('#dialog').open);
      assertFalse(signoutDialog.$$('#deleteProfile').hidden);

      const deleteProfileCheckbox = signoutDialog.$$('#deleteProfile');
      assertTrue(!!deleteProfileCheckbox);
      assertLT(0, deleteProfileCheckbox.clientHeight);

      const disconnectConfirm = signoutDialog.$$('#disconnectConfirm');
      assertTrue(!!disconnectConfirm);
      assertFalse(disconnectConfirm.hidden);

      disconnectConfirm.click();

      await new Promise(function(resolve) {
        listenOnce(window, 'popstate', resolve);
      });
      const deleteProfile = await syncBrowserProxy.whenCalled('signOut');
      assertFalse(deleteProfile);
    });

    test('SignOutDialogManagedProfile', async function() {
      let accountControl = null;
      await syncBrowserProxy.whenCalled('getSyncStatus');
      simulateSyncStatus({
        signedIn: true,
        domain: 'example.com',
        syncSystemEnabled: true,
      });

      assertFalse(!!peoplePage.$$('#dialog'));
      accountControl = peoplePage.$$('settings-sync-account-control');
      await waitBeforeNextRender(accountControl);
      const turnOffButton = accountControl.$$('#turn-off');
      turnOffButton.click();
      flush();

      await new Promise(function(resolve) {
        peoplePage.async(resolve);
      });
      const signoutDialog = peoplePage.$$('settings-signout-dialog');
      assertTrue(signoutDialog.$$('#dialog').open);
      assertFalse(!!signoutDialog.$$('#deleteProfile'));

      const disconnectManagedProfileConfirm =
          signoutDialog.$$('#disconnectManagedProfileConfirm');
      assertTrue(!!disconnectManagedProfileConfirm);
      assertFalse(disconnectManagedProfileConfirm.hidden);

      syncBrowserProxy.resetResolver('signOut');


      disconnectManagedProfileConfirm.click();

      await new Promise(function(resolve) {
        listenOnce(window, 'popstate', resolve);
      });
      const deleteProfile = await syncBrowserProxy.whenCalled('signOut');
      assertTrue(deleteProfile);
    });

    test('getProfileStatsCount', async function() {
      // Navigate to chrome://settings/signOut
      Router.getInstance().navigateTo(routes.SIGN_OUT);

      await new Promise(function(resolve) {
        peoplePage.async(resolve);
      });
      flush();
      const signoutDialog = peoplePage.$$('settings-signout-dialog');
      assertTrue(signoutDialog.$$('#dialog').open);

      // Assert the warning message is as expected.
      const warningMessage = signoutDialog.$$('.delete-profile-warning');

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
      signoutDialog.$$('#disconnectConfirm').click();
      await new Promise(function(resolve) {
        listenOnce(window, 'popstate', resolve);
      });
    });

    test('NavigateDirectlyToSignOutURL', async function() {
      // Navigate to chrome://settings/signOut
      Router.getInstance().navigateTo(routes.SIGN_OUT);

      await new Promise(function(resolve) {
        peoplePage.async(resolve);
      });
      assertTrue(peoplePage.$$('settings-signout-dialog').$$('#dialog').open);
      await profileInfoBrowserProxy.whenCalled('getProfileStatsCount');
      // 'getProfileStatsCount' can be the first message sent to the
      // handler if the user navigates directly to
      // chrome://settings/signOut. if so, it should not cause a crash.
      new ProfileInfoBrowserProxyImpl().getProfileStatsCount();

      // Close the disconnect dialog.
      peoplePage.$$('settings-signout-dialog').$$('#disconnectConfirm').click();
      await new Promise(function(resolve) {
        listenOnce(window, 'popstate', resolve);
      });
    });

    test('Signout dialog suppressed when not signed in', async function() {
      await syncBrowserProxy.whenCalled('getSyncStatus');
      Router.getInstance().navigateTo(routes.SIGN_OUT);
      await new Promise(function(resolve) {
        peoplePage.async(resolve);
      });
      assertTrue(peoplePage.$$('settings-signout-dialog').$$('#dialog').open);

      simulateSyncStatus({
        signedIn: false,
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
}

suite('SyncSettings', function() {
  setup(async function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;

    PolymerTest.clearBody();
    peoplePage = document.createElement('settings-people-page');
    peoplePage.pageVisibility = pageVisibility;
    document.body.appendChild(peoplePage);

    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('ShowCorrectSyncRow', function() {
    assertTrue(!!peoplePage.$$('#sync-setup'));
    assertFalse(!!peoplePage.$$('#sync-status'));

    // Make sures the subpage opens even when logged out or has errors.
    simulateSyncStatus({
      signedIn: false,
      statusAction: StatusAction.REAUTHENTICATE,
    });

    peoplePage.$$('#sync-setup').click();
    flush();

    assertEquals(Router.getInstance().getCurrentRoute(), routes.SYNC);
  });
});
