// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';
import 'chrome://settings/settings.js';

import type {AccountManagerBrowserProxy, CrLinkRowElement, SettingsPeoplePageElement} from 'chrome://settings/settings.js';
import {AccountManagerBrowserProxyImpl, loadTimeData, PrefService, PrefsBrowserProxy, ProfileInfoBrowserProxyImpl, Router, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {simulateSyncStatus} from './sync_test_util.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
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

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: 'signin.allowed_on_next_startup',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'profile.password_manager_leak_detection',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'safebrowsing.enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'safebrowsing.scout_reporting_enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
  ];
}

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
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    const prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    syncBrowserProxy = new TestSyncBrowserProxy();
    // The profile row is only available when the account page link row is not.
    syncBrowserProxy.testSyncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstance(accountManagerBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);

    await accountManagerBrowserProxy.whenCalled('getAccounts');
    await syncBrowserProxy.whenCalled('getSyncStatus');
    await microtasksFinished();
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('GAIA name and picture', () => {
    assertTrue(
        peoplePage.shadowRoot.querySelector<HTMLElement>('#profile-icon')!.style
            .backgroundImage.includes(
                'data:image/png;base64,primaryAccountPicData'));
    assertEquals(
        'Primary Account',
        peoplePage.shadowRoot.querySelector(
                                 '#profile-name')!.textContent.trim());
  });

  test('profile row is actionable', () => {
    // Profile row opens account manager, so the row is actionable.
    const profileRow = peoplePage.shadowRoot.querySelector('#profile-row');
    assertTrue(!!profileRow);
    assertTrue(profileRow.hasAttribute('actionable'));
    const subpageArrow = peoplePage.shadowRoot.querySelector<HTMLElement>(
        '#profile-subpage-arrow');
    assertTrue(!!subpageArrow);
    assertFalse(subpageArrow.hidden);
  });

  test('SyncSetupSubLabelUpdatedForPassphraseError', async () => {
    await simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
      statusText:
          'To use and save Chromium data in your Google Account, enter your passphrase',
    });

    const syncSetupRow =
        peoplePage.shadowRoot.querySelector<CrLinkRowElement>('#sync-setup')!;
    assertEquals(peoplePage.syncStatus!.statusText, syncSetupRow.subLabel);
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
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    const prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    syncBrowserProxy = new TestSyncBrowserProxy();
    // The profile row is only available when the account page link row is not.
    syncBrowserProxy.testSyncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);

    await syncBrowserProxy.whenCalled('getSyncStatus');
    await microtasksFinished();
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('profile row is not actionable', () => {
    // Account manager isn't available, so the row isn't actionable.
    const profileIcon =
        peoplePage.shadowRoot.querySelector<HTMLElement>('#profile-icon');
    assertTrue(!!profileIcon);
    assertFalse(profileIcon.hasAttribute('actionable'));
    const profileRow = peoplePage.shadowRoot.querySelector('#profile-row');
    assertTrue(!!profileRow);
    assertFalse(profileRow.hasAttribute('actionable'));
    const subpageArrow = peoplePage.shadowRoot.querySelector<HTMLElement>(
        '#profile-subpage-arrow');
    assertTrue(!!subpageArrow);
    assertTrue(subpageArrow.hidden);

    // Clicking on profile icon doesn't navigate to a new route.
    const oldRoute = Router.getInstance().getCurrentRoute();
    profileIcon.click();
    assertEquals(oldRoute, Router.getInstance().getCurrentRoute());
  });
});

suite('Chrome OS with replaceSyncPromosWithSignInPromos enabled', function() {
  suiteSetup(function() {
    loadTimeData.overrideValues({
      isAccountManagerEnabled: true,
      replaceSyncPromosWithSignInPromos: true,
    });
  });

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    const prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstance(accountManagerBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);

    await accountManagerBrowserProxy.whenCalled('getAccounts');
    await syncBrowserProxy.whenCalled('getSyncStatus');
    await microtasksFinished();
  });

  teardown(function() {
    peoplePage.remove();
  });

  test('SyncSetupRowSublabel_PassphraseError', async () => {
    await simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      hasError: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
      statusText:
          'To use and save Chromium data in your Google Account, enter your passphrase',
    });

    const syncSetupRow =
        peoplePage.shadowRoot.querySelector<CrLinkRowElement>('#sync-setup')!;
    assertEquals(peoplePage.syncStatus!.statusText, syncSetupRow.subLabel);
  });
});
