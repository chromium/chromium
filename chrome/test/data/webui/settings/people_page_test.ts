// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import type {CrLinkRowElement, SettingsPeoplePageElement} from 'chrome://settings/settings.js';
import {PrefService, PrefsBrowserProxy, ProfileInfoBrowserProxyImpl, resetRouterForTesting, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {simulateSyncStatus} from './sync_test_util.js';
import {TestProfileInfoBrowserProxy} from './test_profile_info_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// <if expr="is_chromeos">
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import type {Account, AccountManagerBrowserProxy} from 'chrome://settings/settings.js';
import {AccountManagerBrowserProxyImpl} from 'chrome://settings/settings.js';
// </if>

// <if expr="not is_chromeos">
import {listenOnce} from 'chrome://resources/js/util.js';
import type {CrCheckboxElement} from 'chrome://settings/lazy_load.js';
import {assertLT} from 'chrome://webui-test/chai_assert.js';
import type {StoredAccount} from 'chrome://settings/settings.js';

import {simulateStoredAccounts} from './sync_test_util.js';
// </if>

// clang-format on

// <if expr="is_chromeos">
type TestAccount = Partial<Account>;
// </if>
// <if expr="not is_chromeos">
type TestAccount = StoredAccount;
// </if>

// <if expr="is_chromeos">
class TestAccountManagerBrowserProxy extends TestBrowserProxy implements
    AccountManagerBrowserProxy {
  private accounts_: Account[] = [];

  constructor() {
    super([
      'getAccounts',
    ]);
  }

  setAccounts(accounts: Account[]) {
    this.accounts_ = accounts;
  }

  getAccounts() {
    this.methodCalled('getAccounts');
    return Promise.resolve(this.accounts_);
  }
}
let accountManagerBrowserProxy: TestAccountManagerBrowserProxy;
// </if>

let peoplePage: SettingsPeoplePageElement;
let profileInfoBrowserProxy: TestProfileInfoBrowserProxy;
let syncBrowserProxy: TestSyncBrowserProxy;

function reset() {
  peoplePage.remove();
  loadTimeData.overrideValues({
    signinAllowed: true,
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

  function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
    return [
      {
        key: 'signin.allowed_on_next_startup',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      {
        key: 'import_dialog_bookmarks',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      {
        key: 'spellcheck.dictionaries',
        type: chrome.settingsPrivate.PrefType.LIST,
        value: ['en-US'],
      },
      {
        key: 'spellcheck.use_spelling_service',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      {
        key: 'search.suggest_enabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      {
        key: 'url_keyed_anonymized_data_collection.enabled',
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

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    const prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = {
      syncSystemEnabled: false,
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
    };
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);

    await syncBrowserProxy.whenCalled('getSyncStatus');
    await profileInfoBrowserProxy.whenCalled('getProfileInfo');
    await microtasksFinished();
  });

  teardown(function() {
    reset();
  });

  test('GetProfileInfo', async function() {
    assertEquals(
        profileInfoBrowserProxy.fakeProfileInfo.name,
        peoplePage.shadowRoot.querySelector<HTMLElement>(
                                 '#profile-name')!.textContent.trim());
    const bg =
        peoplePage.shadowRoot.querySelector<HTMLElement>(
                                 '#profile-icon')!.style.backgroundImage;
    assertTrue(bg.includes(profileInfoBrowserProxy.fakeProfileInfo.iconUrl));

    const iconDataUrl = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA' +
        'LAAAAAABAAEAAAICTAEAOw==';
    webUIListenerCallback(
        'profile-info-changed', {name: 'pushedName', iconUrl: iconDataUrl});

    await microtasksFinished();
    assertEquals(
        'pushedName',
        peoplePage.shadowRoot.querySelector<HTMLElement>(
                                 '#profile-name')!.textContent.trim());
    const newBg =
        peoplePage.shadowRoot.querySelector<HTMLElement>(
                                 '#profile-icon')!.style.backgroundImage;
    assertTrue(newBg.includes(iconDataUrl));
  });
});

// <if expr="not is_chromeos">
suite('SigninDisallowedTests', function() {
  setup(function() {
    loadTimeData.overrideValues({signinAllowed: false});

    syncBrowserProxy = new TestSyncBrowserProxy();
    syncBrowserProxy.testSyncStatus = {
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
    };
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
    await microtasksFinished();

    // The correct /manageProfile link row is shown.
    assertFalse(!!peoplePage.shadowRoot.querySelector('#edit-profile'));
    assertTrue(!!peoplePage.shadowRoot.querySelector('#profile-row'));

    // Control element doesn't exist when policy forbids signin.
    assertFalse(
        !!peoplePage.shadowRoot.querySelector('settings-sync-account-control'));
  });
});

// TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
// deleted from the codebase. See ConsentLevel::kSync documentation for
// details.
suite('SignoutDialogTests', function() {
  setup(async function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    // The WebUI signout dialog is only reachable for syncing users.
    syncBrowserProxy.testSyncStatus = {
      signedInState: SignedInState.SYNCING,
      signedInUsername: 'fakeUsername',
      statusAction: StatusAction.NO_ACTION,
    };
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);
    await microtasksFinished();
  });

  teardown(function() {
    reset();
  });

  test('SignOutNavigationNormalProfile', async function() {
    // Navigate to chrome://settings/signOut
    Router.getInstance().navigateTo(routes.SIGN_OUT);

    await microtasksFinished();
    const signoutDialog =
        peoplePage.shadowRoot.querySelector('settings-signout-dialog')!;
    assertTrue(signoutDialog.$.dialog.open);
    const deleteProfileCheckbox =
        signoutDialog.shadowRoot.querySelector<CrCheckboxElement>(
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
    await simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      domain: 'example.com',
      syncSystemEnabled: true,
      statusAction: StatusAction.NO_ACTION,
    });

    assertFalse(!!peoplePage.shadowRoot.querySelector('#dialog'));
    accountControl =
        peoplePage.shadowRoot.querySelector('settings-sync-account-control')!;
    await syncBrowserProxy.whenCalled('getStoredAccounts');
    await microtasksFinished();
    const turnOffButton =
        accountControl.shadowRoot.querySelector<HTMLElement>('#turn-off')!;
    turnOffButton.click();
    await microtasksFinished();

    const signoutDialog =
        peoplePage.shadowRoot.querySelector('settings-signout-dialog')!;
    assertTrue(signoutDialog.$.dialog.open);
    assertTrue(!!signoutDialog.shadowRoot.querySelector('#deleteProfile'));

    const disconnectConfirm =
        signoutDialog.shadowRoot.querySelector<HTMLElement>(
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

  test('SignOutDialogManagedProfileHtmlEscaping', async function() {
    loadTimeData.overrideValues({
      syncDisconnectManagedProfileExplanation: 'Explanation $1',
    });

    await syncBrowserProxy.whenCalled('getSyncStatus');
    await simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      domain: 'example.com<a href="http://example.com">link</a>',
      syncSystemEnabled: true,
      statusAction: StatusAction.NO_ACTION,
    });

    Router.getInstance().navigateTo(routes.SIGN_OUT);
    await microtasksFinished();

    const signoutDialog =
        peoplePage.shadowRoot.querySelector('settings-signout-dialog');
    assertTrue(!!signoutDialog);
    assertTrue(signoutDialog.$.dialog.open);

    const dialogBody = signoutDialog.shadowRoot.querySelector('[slot=body]');
    assertTrue(!!dialogBody);
    assertEquals(
        'Explanation example.com<a href="http://example.com">link</a>',
        dialogBody.textContent);
    assertFalse(!!dialogBody.querySelector('a'));
  });

  test('getProfileStatsCount', async function() {
    // Navigate to chrome://settings/signOut
    Router.getInstance().navigateTo(routes.SIGN_OUT);

    await microtasksFinished();
    const signoutDialog =
        peoplePage.shadowRoot.querySelector('settings-signout-dialog')!;
    assertTrue(signoutDialog.$.dialog.open);

    // Assert the warning message is as expected.
    const warningMessage = signoutDialog.shadowRoot.querySelector<HTMLElement>(
        '.delete-profile-warning')!;

    webUIListenerCallback('profile-stats-count-ready', 0);
    await microtasksFinished();
    assertEquals(
        loadTimeData.getStringF(
            'deleteProfileWarningWithoutCounts', 'fakeUsername'),
        warningMessage.textContent.trim());

    webUIListenerCallback('profile-stats-count-ready', 1);
    await microtasksFinished();
    assertEquals(
        loadTimeData.getStringF(
            'deleteProfileWarningWithCountsSingular', 'fakeUsername'),
        warningMessage.textContent.trim());

    webUIListenerCallback('profile-stats-count-ready', 2);
    await microtasksFinished();
    assertEquals(
        loadTimeData.getStringF(
            'deleteProfileWarningWithCountsPlural', 2, 'fakeUsername'),
        warningMessage.textContent.trim());
  });

  test('NavigateDirectlyToSignOutURL', async function() {
    // Navigate to chrome://settings/signOut
    Router.getInstance().navigateTo(routes.SIGN_OUT);

    await microtasksFinished();
    assertTrue(
        peoplePage.shadowRoot.querySelector(
                                 'settings-signout-dialog')!.$.dialog.open);
    await profileInfoBrowserProxy.whenCalled('getProfileStatsCount');
    // 'getProfileStatsCount' can be the first message sent to the
    // handler if the user navigates directly to
    // chrome://settings/signOut. if so, it should not cause a crash.
    new ProfileInfoBrowserProxyImpl().getProfileStatsCount();
  });

  test('Signout dialog suppressed when not signed in', async function() {
    await syncBrowserProxy.whenCalled('getSyncStatus');
    Router.getInstance().navigateTo(routes.SIGN_OUT);
    await microtasksFinished();
    assertTrue(
        peoplePage.shadowRoot.querySelector(
                                 'settings-signout-dialog')!.$.dialog.open);

    let whenPopstate = new Promise(function(resolve) {
      listenOnce(window, 'popstate', resolve);
    });
    await simulateSyncStatus({
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
    });
    await whenPopstate;

    whenPopstate = new Promise(function(resolve) {
      listenOnce(window, 'popstate', resolve);
    });
    Router.getInstance().navigateTo(routes.SIGN_OUT);
    await whenPopstate;
  });
});
// </if>

// TODO(crbug.com/40066949): Remove once kSync becomes unreachable or is
// deleted from the codebase. See ConsentLevel::kSync documentation for
// details.
suite('SyncSettings', function() {
  setup(async function() {
    syncBrowserProxy = new TestSyncBrowserProxy();
    // The sync settings only exist for syncing users.
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

    await simulateSyncStatus({
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    });
    await microtasksFinished();
  });

  teardown(function() {
    reset();
  });

  // <if expr="not is_chromeos">
  test('Toast', async function() {
    assertFalse(peoplePage.$.toast.open);
    webUIListenerCallback('sync-settings-saved');
    await microtasksFinished();
    assertTrue(peoplePage.$.toast.open);
  });
  // </if>

  test('ShowCorrectSyncRow', async function() {
    assertTrue(isChildVisible(peoplePage, '#sync-setup'));
    assertFalse(isChildVisible(peoplePage, '#sync-status'));
    assertFalse(isChildVisible(peoplePage, '#google-services'));

    // Make sures the subpage opens even when logged out or has errors.
    await simulateSyncStatus({
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.REAUTHENTICATE,
    });

    peoplePage.shadowRoot.querySelector<HTMLElement>('#sync-setup')!.click();
    await microtasksFinished();

    assertEquals(Router.getInstance().getCurrentRoute(), routes.SYNC);
  });
});

suite('PeoplePageAccountSettings', function() {
  setup(async function() {
    // <if expr="is_chromeos">
    loadTimeData.overrideValues({
      replaceSyncPromosWithSignInPromos: true,
      isAccountManagerEnabled: true,
    });
    // </if>
    resetRouterForTesting();
    Router.getInstance().navigateTo(routes.PEOPLE);

    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    profileInfoBrowserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(profileInfoBrowserProxy);

    // <if expr="is_chromeos">
    accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstance(accountManagerBrowserProxy);
    // </if>

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    peoplePage = document.createElement('settings-people-page');
    document.body.appendChild(peoplePage);
    await microtasksFinished();
  });

  teardown(function() {
    reset();
  });

  async function simulateSignedInState(
      state: SignedInState, accounts: TestAccount[]) {
    await syncBrowserProxy.whenCalled('getSyncStatus');
    await simulateSyncStatus({
      signedInState: state,
      syncSystemEnabled: true,
      hasError: false,
      statusAction: StatusAction.NO_ACTION,
    });

    // <if expr="not is_chromeos">
    await syncBrowserProxy.whenCalled('getStoredAccounts');
    simulateStoredAccounts(accounts);
    // </if>

    // <if expr="is_chromeos">
    await accountManagerBrowserProxy.whenCalled('getAccounts');
    accountManagerBrowserProxy.setAccounts(accounts as Account[]);
    webUIListenerCallback('accounts-changed');
    // </if>

    await microtasksFinished();
  }

  test('ShowCorrectRowsSignedIn', async function() {
    await simulateSignedInState(
        SignedInState.SIGNED_IN, [{email: 'foo@foo.com'}]);

    // <if expr="not is_chromeos">
    // The account card and the profile should not exist. Instead, there is a
    // link row which leads to the account settings page.
    assertFalse(isChildVisible(peoplePage, 'settings-sync-account-control'));
    // </if>
    assertFalse(isChildVisible(peoplePage, '#profile-row'));
    assertTrue(isChildVisible(peoplePage, '#account-subpage-row'));

    // There is a link to the Google services, not to the sync settings.
    assertTrue(isChildVisible(peoplePage, '#google-services'));
    assertFalse(isChildVisible(peoplePage, '#sync-setup'));

    // <if expr="not is_chromeos">
    // The other rows are shown/hidden correctly.
    assertTrue(isChildVisible(peoplePage, '#edit-profile'));
    assertFalse(isChildVisible(peoplePage, '#manage-google-account'));
    assertTrue(isChildVisible(peoplePage, '#importDataDialogTrigger'));
    // </if>
  });

  test('ShowCorrectRowsSyncing', async function() {
    await simulateSignedInState(
        SignedInState.SYNCING, [{email: 'foo@foo.com'}]);

    // <if expr="not is_chromeos">
    // The first item should be an account card.
    assertTrue(isChildVisible(peoplePage, 'settings-sync-account-control'));
    assertFalse(isChildVisible(peoplePage, '#profile-row'));
    // </if>

    // <if expr="is_chromeos">
    assertTrue(isChildVisible(peoplePage, '#profile-row'));
    // </if>

    assertFalse(isChildVisible(peoplePage, '#account-subpage-row'));
    // There is a link to the sync settings, not to the Google services.
    assertFalse(isChildVisible(peoplePage, '#google-services'));
    assertTrue(isChildVisible(peoplePage, '#sync-setup'));

    // <if expr="not is_chromeos">
    // The other rows are shown correctly.
    assertTrue(isChildVisible(peoplePage, '#edit-profile'));
    assertTrue(isChildVisible(peoplePage, '#manage-google-account'));
    assertTrue(isChildVisible(peoplePage, '#importDataDialogTrigger'));
    // </if>
  });

  test('ClickingAccountLinkRowLeadsToAccountSettings', async function() {
    await simulateSignedInState(
        SignedInState.SIGNED_IN, [{email: 'foo@foo.com'}]);

    peoplePage.shadowRoot.querySelector<HTMLElement>(
                             '#account-subpage-row')!.click();
    assertEquals(routes.ACCOUNT, Router.getInstance().getCurrentRoute());
  });

  test('ClickingGoogleServicesLeadsToGoogleServicesPage', async function() {
    await simulateSignedInState(SignedInState.SIGNED_OUT, []);

    peoplePage.shadowRoot.querySelector<HTMLElement>(
                             '#google-services')!.click();
    assertEquals(
        routes.GOOGLE_SERVICES, Router.getInstance().getCurrentRoute());
  });

  test('AccountLinkRowHasAccountInfo', async function() {
    const image = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAA' +
        'AAABAAEAAAICTAEAOw==';
    const expectedAccount = {
      fullName: 'Test Name',
      email: 'test@email.com',
      // <if expr="not is_chromeos">
      avatarImage: image,
      // </if>
      // <if expr="is_chromeos">
      pic: image,
      // </if>
    };
    await simulateSignedInState(SignedInState.SIGNED_IN, [expectedAccount]);

    const accountRow = peoplePage.shadowRoot.querySelector<CrLinkRowElement>(
        '#account-subpage-row')!;

    assertEquals(expectedAccount.fullName, accountRow.label);
    assertEquals(expectedAccount.email, accountRow.subLabel);

    const bgImage =
        peoplePage.shadowRoot.querySelector<HTMLElement>(
                                 '#profile-icon')!.style.backgroundImage;
    assertTrue(bgImage.includes(image));
  });

  test('AccountRowSubtitleUpdatedForPassphraseError', async function() {
    const testEmail = 'test@email.com';
    await simulateSignedInState(SignedInState.SIGNED_IN, [{email: testEmail}]);

    // First, it shows the user's email.
    const accountRow = peoplePage.shadowRoot.querySelector<CrLinkRowElement>(
        '#account-subpage-row')!;
    assertEquals(testEmail, accountRow.subLabel);

    // When the passphrase needs to be entered, a message is displayed instead.
    await simulateSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.ENTER_PASSPHRASE,
      statusText: 'Enter the passphrase for $1',
    });
    assertEquals(
        loadTimeData.substituteString(
            peoplePage.syncStatus!.statusText!, testEmail),
        accountRow.subLabel);
  });

  test('AccountRowSubtitleUpdatedForBookmarksLimitError_AccountSettings',
       async function() {
         const testEmail = 'test@email.com';
         await simulateSignedInState(SignedInState.SIGNED_IN, [{email: testEmail}]);

         // First, it shows the user's email.
         const accountRow =
             peoplePage.shadowRoot.querySelector<CrLinkRowElement>(
                 '#account-subpage-row')!;
         assertEquals(testEmail, accountRow.subLabel);

         const bookmarksLimitError =
             'To save bookmarks in your account, delete your unused bookmarks';
         await simulateSyncStatus({
           signedInState: SignedInState.SIGNED_IN,
           statusAction: StatusAction.SHOW_BOOKMARKS_LIMIT_HELP_ARTICLE,
           statusText: bookmarksLimitError,
         });
         assertEquals(bookmarksLimitError, accountRow.subLabel);
  });

  // <if expr="not is_chromeos">
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
  // </if>
});
