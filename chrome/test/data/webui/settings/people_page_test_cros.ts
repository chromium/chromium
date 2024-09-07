// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {AccountManagerBrowserProxy, SettingsPeoplePageElement} from 'chrome://settings/settings.js';
import {AccountManagerBrowserProxyImpl, loadTimeData, pageVisibility, ProfileInfoBrowserProxyImpl, Router, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {simulateSyncStatus} from './sync_test_util.js';
import {TestProfileInfoBrowserProxy} from './test_profile_info_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

class TestAccountManagerBrowserProxy extends TestBrowserProxy implements
    AccountManagerBrowserProxy {
  constructor() {
    super([
      'getAccounts',
    ]);
  }

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
}

let accountManagerBrowserProxy: TestAccountManagerBrowserProxy;

// Preferences should exist for embedded 'personalization_options.html'.
// We don't perform tests on them.
const DEFAULT_PREFS = {
  profile: {password_manager_leak_detection: {value: true}},
  signin: {
    allowed_on_next_startup:
        {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
  },
  safebrowsing:
      {enabled: {value: true}, scout_reporting_enabled: {value: true}},
};

let peoplePage: SettingsPeoplePageElement;
let profileInfoBrowserProxy: TestProfileInfoBrowserProxy;
let syncBrowserProxy: TestSyncBrowserProxy;

suite('Chrome OS', function() {
  suiteSetup(function() {
    loadTimeData.overrideValues({
      // Simulate ChromeOSAccountManager (Google Accounts support).
      isAccountManagerEnabled: true,
    });
  });

  setup(async function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstance(accountManagerBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    peoplePage.prefs = DEFAULT_PREFS;
    peoplePage.pageVisibility = pageVisibility || {};
    document.body.appendChild(peoplePage);

    await accountManagerBrowserProxy.whenCalled('getAccounts');
    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('GAIA name and picture', async () => {
    assertTrue(
        peoplePage.shadowRoot!.querySelector<HTMLElement>('#profile-icon')!
            .style.backgroundImage.includes(
                'data:image/png;base64,primaryAccountPicData'));
    assertEquals(
        'Primary Account',
        peoplePage.shadowRoot!.querySelector(
                                  '#profile-name')!.textContent!.trim());
  });

  test('profile row is actionable', () => {
    // Simulate a signed-in user.
    simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    });

    // Profile row opens account manager, so the row is actionable.
    const profileRow = peoplePage.shadowRoot!.querySelector('#profile-row');
    assertTrue(!!profileRow);
    assertTrue(profileRow!.hasAttribute('actionable'));
    const subpageArrow = peoplePage.shadowRoot!.querySelector<HTMLElement>(
        '#profile-subpage-arrow');
    assertTrue(!!subpageArrow);
    assertFalse(subpageArrow!.hidden);
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
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    peoplePage.prefs = DEFAULT_PREFS;
    peoplePage.pageVisibility = pageVisibility || {};
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
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    });

    // Account manager isn't available, so the row isn't actionable.
    const profileIcon =
        peoplePage.shadowRoot!.querySelector<HTMLElement>('#profile-icon');
    assertTrue(!!profileIcon);
    assertFalse(profileIcon!.hasAttribute('actionable'));
    const profileRow = peoplePage.shadowRoot!.querySelector('#profile-row');
    assertTrue(!!profileRow);
    assertFalse(profileRow!.hasAttribute('actionable'));
    const subpageArrow = peoplePage.shadowRoot!.querySelector<HTMLElement>(
        '#profile-subpage-arrow');
    assertTrue(!!subpageArrow!);
    assertTrue(subpageArrow!.hidden);

    // Clicking on profile icon doesn't navigate to a new route.
    const oldRoute = Router.getInstance().getCurrentRoute();
    profileIcon!.click();
    assertEquals(oldRoute, Router.getInstance().getCurrentRoute());
  });
});
