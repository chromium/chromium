// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// #import {Router, PageStatus, pageVisibility, routes, AccountManagerBrowserProxyImpl, SyncBrowserProxyImpl, ProfileInfoBrowserProxyImpl, ProfileInfoBrowserProxy} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestProfileInfoBrowserProxy} from 'chrome://test/settings/test_profile_info_browser_proxy.js';
// #import {TestSyncBrowserProxy} from './test_os_sync_browser_proxy.m.js';
// #import {FakeQuickUnlockPrivate} from './fake_quick_unlock_private.m.js';
// #import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';

// clang-format on

cr.define('settings_people_page', function() {
  let quickUnlockPrivateApi = null;

  /** @implements {settings.AccountManagerBrowserProxy} */
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

      return Promise.resolve([
        {
          id: '123',
          accountType: 1,
          isDeviceAccount: true,
          isSignedIn: true,
          unmigrated: false,
          fullName: 'Primary Account',
          pic: 'data:image/png;base64,primaryAccountPicData',
          email: 'primary@gmail.com',
        },
        {
          id: '456',
          accountType: 1,
          isDeviceAccount: false,
          isSignedIn: true,
          unmigrated: false,
          fullName: 'Secondary Account 1',
          email: 'user1@example.com',
          pic: '',
        },
        {
          id: '789',
          accountType: 1,
          isDeviceAccount: false,
          isSignedIn: false,
          unmigrated: false,
          fullName: 'Secondary Account 2',
          email: 'user2@example.com',
          pic: '',
        },
      ]);
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

  suite('PeoplePageTests', function() {
    /** @type {SettingsPeoplePageElement} */
    let peoplePage = null;
    /** @type {settings.ProfileInfoBrowserProxy} */
    let browserProxy = null;
    /** @type {settings.SyncBrowserProxy} */
    let syncBrowserProxy = null;
    /** @type {settings.AccountManagerBrowserProxy} */
    let accountManagerBrowserProxy = null;

    setup(function() {
      browserProxy = new TestProfileInfoBrowserProxy();
      settings.ProfileInfoBrowserProxyImpl.instance_ = browserProxy;

      syncBrowserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = syncBrowserProxy;

      accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
      settings.AccountManagerBrowserProxyImpl.instance_ =
          accountManagerBrowserProxy;

      PolymerTest.clearBody();
    });

    teardown(function() {
      peoplePage.remove();
      settings.Router.getInstance().resetRouteForTesting();
    });

    test('Profile name and picture, account manager disabled', async () => {
      loadTimeData.overrideValues({
        isAccountManagerEnabled: false,
      });
      peoplePage = document.createElement('os-settings-people-page');
      peoplePage.pageVisibility = settings.pageVisibility;
      document.body.appendChild(peoplePage);

      await browserProxy.whenCalled('getProfileInfo');
      await syncBrowserProxy.whenCalled('getSyncStatus');
      Polymer.dom.flush();

      // Get page elements.
      const profileIconEl = assert(peoplePage.$$('#profile-icon'));
      const profileRowEl = assert(peoplePage.$$('#profile-row'));
      const profileNameEl = assert(peoplePage.$$('#profile-name'));

      assertEquals(
          browserProxy.fakeProfileInfo.name, profileNameEl.textContent.trim());
      const bg = profileIconEl.style.backgroundImage;
      assertTrue(bg.includes(browserProxy.fakeProfileInfo.iconUrl));
      assertEquals(
          'fakeUsername', peoplePage.$$('#profile-label').textContent.trim());

      const iconDataUrl = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA' +
          'LAAAAAABAAEAAAICTAEAOw==';
      cr.webUIListenerCallback(
          'profile-info-changed', {name: 'pushedName', iconUrl: iconDataUrl});

      Polymer.dom.flush();
      assertEquals('pushedName', profileNameEl.textContent.trim());
      const newBg = profileIconEl.style.backgroundImage;
      assertTrue(newBg.includes(iconDataUrl));

      // Profile row items aren't actionable.
      assertFalse(profileIconEl.hasAttribute('actionable'));
      assertFalse(profileRowEl.hasAttribute('actionable'));

      // Sub-page trigger is hidden.
      assertTrue(peoplePage.$$('#account-manager-subpage-trigger').hidden);
    });

    test('parental controls page is shown when enabled', () => {
      loadTimeData.overrideValues({
        // Simulate parental controls.
        showParentalControls: true,
      });

      peoplePage = document.createElement('os-settings-people-page');
      document.body.appendChild(peoplePage);
      Polymer.dom.flush();

      // Setup button is shown and enabled.
      assert(peoplePage.$$('settings-parental-controls-page'));
    });

    test('Deep link to parental controls page', async () => {
      loadTimeData.overrideValues({
        // Simulate parental controls.
        showParentalControls: true,
        isDeepLinkingEnabled: true,
      });

      peoplePage = document.createElement('os-settings-people-page');
      document.body.appendChild(peoplePage);
      Polymer.dom.flush();

      const params = new URLSearchParams;
      params.append('settingId', '315');
      settings.Router.getInstance().navigateTo(
          settings.routes.OS_PEOPLE, params);

      const deepLinkElement =
          peoplePage.$$('settings-parental-controls-page').$$('#setupButton');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Setup button should be focused for settingId=315.');
    });

    test('Deep link to guest browsing on users page', async () => {
      loadTimeData.overrideValues({isDeepLinkingEnabled: true});

      peoplePage = document.createElement('os-settings-people-page');
      document.body.appendChild(peoplePage);
      Polymer.dom.flush();

      if (peoplePage.isAccountManagementFlowsV2Enabled_) {
        return;
      }

      const params = new URLSearchParams;
      params.append('settingId', '305');
      settings.Router.getInstance().navigateTo(
          settings.routes.ACCOUNTS, params);

      Polymer.dom.flush();

      await test_util.waitAfterNextRender(peoplePage);
      assertEquals(
          peoplePage.$$('settings-users-page')
              .$$('#allowGuestBrowsing')
              .$$('cr-toggle'),
          getDeepActiveElement(),
          'Allow guest browsing should be focused for settingId=305.');
    });

    test('Deep link to encryption options on old sync page', async () => {
      loadTimeData.overrideValues({
        isDeepLinkingEnabled: true,
      });

      peoplePage = document.createElement('os-settings-people-page');
      document.body.appendChild(peoplePage);
      Polymer.dom.flush();

      // Load the sync page.
      settings.Router.getInstance().navigateTo(settings.routes.SYNC);
      Polymer.dom.flush();
      await test_util.waitAfterNextRender(peoplePage);

      // Make the sync page configurable.
      const syncPage = peoplePage.$$('settings-sync-page');
      assert(syncPage);
      syncPage.syncPrefs = {
        encryptAllDataAllowed: true,
        passphraseRequired: false,
      };
      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.CONFIGURE);
      assertFalse(syncPage.$$('#' + settings.PageStatus.CONFIGURE).hidden);
      assertTrue(syncPage.$$('#' + settings.PageStatus.SPINNER).hidden);

      // Try the deep link.
      const params = new URLSearchParams;
      params.append('settingId', '316');
      settings.Router.getInstance().navigateTo(settings.routes.SYNC, params);

      // Flush to make sure the dropdown expands.
      Polymer.dom.flush();
      const deepLinkElement = syncPage.$$('settings-sync-encryption-options')
                                  .$$('#encryptionRadioGroup')
                                  .buttons_[0]
                                  .$$('#button');
      assert(deepLinkElement);

      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Encryption option should be focused for settingId=316.');
    });

    test('GAIA name and picture, account manager enabled', async () => {
      const fakeOsProfileName = 'Currently signed in as username';
      loadTimeData.overrideValues({
        isAccountManagerEnabled: true,
        // settings-account-manager requires this to have a value.
        secondaryGoogleAccountSigninAllowed: true,
        osProfileName: fakeOsProfileName,
      });
      peoplePage = document.createElement('os-settings-people-page');
      peoplePage.pageVisibility = settings.pageVisibility;
      document.body.appendChild(peoplePage);

      await accountManagerBrowserProxy.whenCalled('getAccounts');
      await syncBrowserProxy.whenCalled('getSyncStatus');
      Polymer.dom.flush();

      // Get page elements.
      const profileIconEl = assert(peoplePage.$$('#profile-icon'));
      const profileRowEl = assert(peoplePage.$$('#profile-row'));
      const profileNameEl = assert(peoplePage.$$('#profile-name'));

      chai.assert.include(
          profileIconEl.style.backgroundImage,
          'data:image/png;base64,primaryAccountPicData');
      if (peoplePage.isAccountManagementFlowsV2Enabled_) {
        assertEquals(fakeOsProfileName, profileNameEl.textContent.trim());
      } else {
        assertEquals('Primary Account', profileNameEl.textContent.trim());
      }

      // Rather than trying to mock cr.sendWithPromise('getPluralString', ...)
      // just force an update.
      await peoplePage.updateAccounts_();
      if (peoplePage.isAccountManagementFlowsV2Enabled_) {
        assertEquals(
            '3 Google Accounts',
            peoplePage.$$('#profile-label').textContent.trim());
      } else {
        assertEquals(
            'primary@gmail.com, +2 more accounts',
            peoplePage.$$('#profile-label').textContent.trim());
      }

      // Profile row items are actionable.
      assertTrue(profileIconEl.hasAttribute('actionable'));
      assertTrue(profileRowEl.hasAttribute('actionable'));

      // Sub-page trigger is shown.
      const subpageTrigger = peoplePage.$$('#account-manager-subpage-trigger');
      assertFalse(subpageTrigger.hidden);

      // Sub-page trigger navigates to Google account manager.
      subpageTrigger.click();
      assertEquals(
          settings.Router.getInstance().getCurrentRoute(),
          settings.routes.ACCOUNT_MANAGER);
    });

    test('Fingerprint dialog closes when token expires', async () => {
      loadTimeData.overrideValues({
        fingerprintUnlockEnabled: true,
      });

      peoplePage = document.createElement('os-settings-people-page');
      document.body.appendChild(peoplePage);

      if (peoplePage.isAccountManagementFlowsV2Enabled_) {
        return;
      }

      await accountManagerBrowserProxy.whenCalled('getAccounts');
      await syncBrowserProxy.whenCalled('getSyncStatus');
      quickUnlockPrivateApi = new settings.FakeQuickUnlockPrivate();
      peoplePage.authToken_ = quickUnlockPrivateApi.getFakeToken();

      settings.Router.getInstance().navigateTo(settings.routes.LOCK_SCREEN);
      Polymer.dom.flush();

      const subpageTrigger = peoplePage.$$('#lock-screen-subpage-trigger');
      // Sub-page trigger navigates to the lock screen page.
      subpageTrigger.click();
      Polymer.dom.flush();

      assertEquals(
          settings.Router.getInstance().getCurrentRoute(),
          settings.routes.LOCK_SCREEN);
      const lockScreenPage = assert(peoplePage.$$('#lock-screen'));

      // Password dialog should not open because the authToken_ is set.
      assertFalse(peoplePage.showPasswordPromptDialog_);

      const editFingerprintsTrigger = lockScreenPage.$$('#editFingerprints');
      editFingerprintsTrigger.click();
      Polymer.dom.flush();

      assertEquals(
          settings.Router.getInstance().getCurrentRoute(),
          settings.routes.FINGERPRINT);
      assertFalse(peoplePage.showPasswordPromptDialog_);

      const fingerprintTrigger =
          peoplePage.$$('#fingerprint-list').$$('#addFingerprint');
      fingerprintTrigger.click();

      // Invalidate the auth token by firing an event.
      assertFalse(peoplePage.authToken_ === undefined);
      const event = new CustomEvent('invalidate-auth-token-requested');
      lockScreenPage.dispatchEvent(event);
      assertTrue(peoplePage.authToken_ === undefined);

      assertEquals(
          settings.Router.getInstance().getCurrentRoute(),
          settings.routes.FINGERPRINT);
      assertTrue(peoplePage.showPasswordPromptDialog_);
    });
  });

  // #cr_define_end
  return {};
});
