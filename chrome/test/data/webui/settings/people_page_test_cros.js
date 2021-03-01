// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AccountManagerBrowserProxyImpl, pageVisibility, ProfileInfoBrowserProxyImpl, Router, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {simulateSyncStatus} from 'chrome://test/settings/sync_test_util.m.js';
import {TestProfileInfoBrowserProxy} from 'chrome://test/settings/test_profile_info_browser_proxy.m.js';
import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.m.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// clang-format on

/** @implements {AccountManagerBrowserProxy} */
class TestAccountManagerBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAccounts',
      'addAccount',
      'reauthenticateAccount',
      'removeAccount',
      'showWelcomeDialogIfRequired',
    ]);
  }

  /** @override */
  getAccounts() {
    this.methodCalled('getAccounts');
    return Promise.resolve([{
      id: '123',
      accountType: 1,
      isDeviceAccount: false,
      isSignedIn: true,
      unmigrated: false,
      fullName: 'Primary Account',
      email: 'user@gmail.com',
      pic: 'data:image/png;base64,primaryAccountPicData',
    }]);
  }

  /** @override */
  addAccount() {
    this.methodCalled('addAccount');
  }

  /** @override */
  reauthenticateAccount(account_email) {
    this.methodCalled('reauthenticateAccount', account_email);
  }

  /** @override */
  removeAccount(account) {
    this.methodCalled('removeAccount', account);
  }

  /** @override */
  showWelcomeDialogIfRequired() {
    this.methodCalled('showWelcomeDialogIfRequired');
  }
}

/** @type {?AccountManagerBrowserProxy} */
let accountManagerBrowserProxy = null;

// Preferences should exist for embedded 'personalization_options.html'.
// We don't perform tests on them.
const DEFAULT_PREFS = {
  profile: {password_manager_leak_detection: {value: true}},
  signin: {
    allowed_on_next_startup:
        {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true}
  },
  safebrowsing:
      {enabled: {value: true}, scout_reporting_enabled: {value: true}},
};

/** @type {?SettingsPeoplePageElement} */
let peoplePage = null;

/** @type {?ProfileInfoBrowserProxy} */
let profileInfoBrowserProxy = null;

/** @type {?SyncBrowserProxy} */
let syncBrowserProxy = null;

suite('Chrome OS', function() {
  suiteSetup(function() {
    loadTimeData.overrideValues({
      // Simulate ChromeOSAccountManager (Google Accounts support).
      isAccountManagerEnabled: true,
    });
  });

  setup(async function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;

    accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.instance_ = accountManagerBrowserProxy;

    PolymerTest.clearBody();
    peoplePage = document.createElement('settings-people-page');
    peoplePage.prefs = DEFAULT_PREFS;
    peoplePage.pageVisibility = pageVisibility;
    document.body.appendChild(peoplePage);

    await accountManagerBrowserProxy.whenCalled('getAccounts');
    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('GAIA name and picture', async () => {
    chai.assert.include(
        peoplePage.$$('#profile-icon').style.backgroundImage,
        'data:image/png;base64,primaryAccountPicData');
    assertEquals(
        'Primary Account', peoplePage.$$('#profile-name').textContent.trim());
  });

  test('profile row is actionable', () => {
    // Simulate a signed-in user.
    simulateSyncStatus({
      signedIn: true,
    });

    // Profile row opens account manager, so the row is actionable.
    const profileRow = peoplePage.$$('#profile-row');
    assertTrue(!!profileRow);
    assertTrue(profileRow.hasAttribute('actionable'));
    const subpageArrow = peoplePage.$$('#profile-subpage-arrow');
    assertTrue(!!subpageArrow);
    assertFalse(subpageArrow.hidden);
  });
});

suite('Chrome OS with account manager disabled', function() {
  suiteSetup(function() {
    loadTimeData.overrideValues({
      // Disable ChromeOSAccountManager (Google Accounts support).
      isAccountManagerEnabled: false,
    });
  });

  setup(async function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;

    PolymerTest.clearBody();
    peoplePage = document.createElement('settings-people-page');
    peoplePage.prefs = DEFAULT_PREFS;
    peoplePage.pageVisibility = pageVisibility;
    document.body.appendChild(peoplePage);

    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('profile row is not actionable', () => {
    // Simulate a signed-in user.
    simulateSyncStatus({
      signedIn: true,
    });

    // Account manager isn't available, so the row isn't actionable.
    const profileIcon = peoplePage.$$('#profile-icon');
    assertTrue(!!profileIcon);
    assertFalse(profileIcon.hasAttribute('actionable'));
    const profileRow = peoplePage.$$('#profile-row');
    assertTrue(!!profileRow);
    assertFalse(profileRow.hasAttribute('actionable'));
    const subpageArrow = peoplePage.$$('#profile-subpage-arrow');
    assertTrue(!!subpageArrow);
    assertTrue(subpageArrow.hidden);

    // Clicking on profile icon doesn't navigate to a new route.
    const oldRoute = Router.getInstance().getCurrentRoute();
    profileIcon.click();
    assertEquals(oldRoute, Router.getInstance().getCurrentRoute());
  });
});

suite('Chrome OS with UseBrowserSyncConsent', function() {
  suiteSetup(function() {
    loadTimeData.overrideValues({
      splitSettingsSyncEnabled: true,
      useBrowserSyncConsent: true,
    });
  });

  setup(async function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.instance_ = profileInfoBrowserProxy;

    PolymerTest.clearBody();
    peoplePage = document.createElement('settings-people-page');
    peoplePage.prefs = DEFAULT_PREFS;
    peoplePage.pageVisibility = pageVisibility;
    document.body.appendChild(peoplePage);

    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('Sync account control is shown', () => {
    simulateSyncStatus({
      syncSystemEnabled: true,
    });

    // Account control is visible.
    const accountControl = peoplePage.$$('settings-sync-account-control');
    assertNotEquals('none', window.getComputedStyle(accountControl).display);

    // Profile row items are not available.
    assertFalse(!!peoplePage.$$('#profile-icon'));
    assertFalse(!!peoplePage.$$('#profile-row'));
  });
});
