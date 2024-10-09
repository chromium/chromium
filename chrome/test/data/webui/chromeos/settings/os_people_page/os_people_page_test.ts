// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxy, AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrIconButtonElement, CrRadioGroupElement, OsSettingsPeoplePageElement, OsSettingsRoutes, PageStatus, ProfileInfoBrowserProxy, ProfileInfoBrowserProxyImpl, Router, routes, settingMojom, SyncBrowserProxy, SyncBrowserProxyImpl} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestSyncBrowserProxy} from '../test_os_sync_browser_proxy.js';

import {TestAccountManagerBrowserProxy} from './test_account_manager_browser_proxy.js';
import {TestProfileInfoBrowserProxy} from './test_profile_info_browser_proxy.js';

interface SubpageTriggerData {
  triggerSelector: string;
  routeName: keyof OsSettingsRoutes;
}

suite('<os-settings-people-page>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  const isGraduationFlagEnabled =
      loadTimeData.getBoolean('isGraduationFlagEnabled');
  let peoplePage: OsSettingsPeoplePageElement;
  let browserProxy: ProfileInfoBrowserProxy&TestProfileInfoBrowserProxy;
  let syncBrowserProxy: SyncBrowserProxy&TestSyncBrowserProxy;
  let accountManagerBrowserProxy: AccountManagerBrowserProxy&
      TestAccountManagerBrowserProxy;

  function createPage(): void {
    peoplePage = document.createElement('os-settings-people-page');
    document.body.appendChild(peoplePage);
    flush();
  }

  setup(() => {
    browserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(browserProxy);

    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(
        accountManagerBrowserProxy);

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
  });

  teardown(() => {
    peoplePage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  if (!isRevampWayfindingEnabled) {
    test('Profile name and picture, account manager disabled', async () => {
      loadTimeData.overrideValues({
        isAccountManagerEnabled: false,
      });
      createPage();

      await browserProxy.whenCalled('getProfileInfo');
      await syncBrowserProxy.whenCalled('getSyncStatus');
      flush();

      // Get page elements.
      const profileIconEl =
          peoplePage.shadowRoot!.querySelector<HTMLElement>('#profile-icon');
      assertTrue(!!profileIconEl);
      const profileRowEl = peoplePage.shadowRoot!.querySelector('#profile-row');
      assertTrue(!!profileRowEl);
      const profileNameEl =
          peoplePage.shadowRoot!.querySelector('#profile-name');
      assertTrue(!!profileNameEl);

      assertEquals(
          browserProxy.fakeProfileInfo.name, profileNameEl.textContent!.trim());
      const bg = profileIconEl.style.backgroundImage;
      assertTrue(bg.includes(browserProxy.fakeProfileInfo.iconUrl));
      const profileLabelEl =
          peoplePage.shadowRoot!.querySelector('#profile-label');
      assertTrue(!!profileLabelEl);
      assertEquals('fakeUsername', profileLabelEl.textContent!.trim());

      const iconDataUrl = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEA' +
          'LAAAAAABAAEAAAICTAEAOw==';
      webUIListenerCallback(
          'profile-info-changed', {name: 'pushedName', iconUrl: iconDataUrl});

      flush();
      assertEquals('pushedName', profileNameEl.textContent!.trim());
      const newBg = profileIconEl.style.backgroundImage;
      assertTrue(newBg.includes(iconDataUrl));

      // Profile row items aren't actionable.
      assertFalse(profileIconEl.hasAttribute('actionable'));
      assertFalse(profileRowEl.hasAttribute('actionable'));

      // Sub-page trigger is hidden.
      const element = peoplePage.shadowRoot!.querySelector<CrIconButtonElement>(
          '#accountManagerSubpageTrigger');
      assertTrue(!!element);
      assertTrue(element.hidden);
    });

    test('parental controls page is shown when enabled', () => {
      loadTimeData.overrideValues({
        // Simulate parental controls.
        showParentalControls: true,
      });
      createPage();

      // Setup button is shown and enabled.
      assert(peoplePage.shadowRoot!.querySelector(
          'settings-parental-controls-page'));
    });

    test('Deep link to parental controls page', async () => {
      loadTimeData.overrideValues({
        // Simulate parental controls.
        showParentalControls: true,
      });
      createPage();

      const params = new URLSearchParams();
      params.append(
          'settingId', settingMojom.Setting.kSetUpParentalControls.toString());
      Router.getInstance().navigateTo(routes.OS_PEOPLE, params);

      const element = peoplePage.shadowRoot!.querySelector(
          'settings-parental-controls-page');
      assertTrue(!!element);
      const deepLinkElement =
          element.shadowRoot!.querySelector<HTMLElement>('#setupButton');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Setup button should be focused for settingId=315.');
    });

    test('Deep link to encryption options on old sync page', async () => {
      createPage();

      // Load the sync page.
      Router.getInstance().navigateTo(routes.SYNC);
      flush();
      await waitAfterNextRender(peoplePage);

      // Make the sync page configurable.
      const syncPage =
          peoplePage.shadowRoot!.querySelector('os-settings-sync-subpage');
      assertTrue(!!syncPage);
      syncPage.syncPrefs = {
        customPassphraseAllowed: true,
        passphraseRequired: false,
        appsManaged: false,
        appsRegistered: false,
        appsSynced: false,
        autofillManaged: false,
        autofillRegistered: false,
        autofillSynced: false,
        bookmarksManaged: false,
        bookmarksRegistered: false,
        bookmarksSynced: false,
        cookiesManaged: false,
        cookiesRegistered: false,
        cookiesSynced: false,
        encryptAllData: false,
        extensionsManaged: false,
        extensionsRegistered: false,
        extensionsSynced: false,
        passwordsManaged: false,
        passwordsRegistered: false,
        passwordsSynced: false,
        paymentsManaged: false,
        paymentsRegistered: false,
        paymentsSynced: false,
        preferencesManaged: false,
        preferencesRegistered: false,
        preferencesSynced: false,
        productComparisonManaged: false,
        productComparisonRegistered: false,
        productComparisonSynced: false,
        readingListManaged: false,
        readingListRegistered: false,
        readingListSynced: false,
        savedTabGroupsManaged: false,
        savedTabGroupsRegistered: false,
        savedTabGroupsSynced: false,
        syncAllDataTypes: false,
        tabsManaged: false,
        tabsRegistered: false,
        tabsSynced: false,
        themesManaged: false,
        themesRegistered: false,
        themesSynced: false,
        trustedVaultKeysRequired: false,
        typedUrlsManaged: false,
        typedUrlsRegistered: false,
        typedUrlsSynced: false,
        wifiConfigurationsManaged: false,
        wifiConfigurationsRegistered: false,
        wifiConfigurationsSynced: false,
      };
      webUIListenerCallback('page-status-changed', PageStatus.CONFIGURE);
      const configureElement = syncPage.shadowRoot!.querySelector<HTMLElement>(
          '#' + PageStatus.CONFIGURE);
      assertTrue(!!configureElement);
      assertFalse(configureElement.hidden);
      const spinnerElement = syncPage.shadowRoot!.querySelector<HTMLElement>(
          '#' + PageStatus.SPINNER);
      assertTrue(!!spinnerElement);
      assertTrue(spinnerElement.hidden);

      // Try the deep link.
      const params = new URLSearchParams();
      params.append(
          'settingId',
          settingMojom.Setting.kNonSplitSyncEncryptionOptions.toString());
      Router.getInstance().navigateTo(routes.SYNC, params);

      // Flush to make sure the dropdown expands.
      flush();
      const element = syncPage.shadowRoot!.querySelector(
          'os-settings-sync-encryption-options');
      assertTrue(!!element);
      const radioGroupElement =
          element.shadowRoot!.querySelector<CrRadioGroupElement>(
              '#encryptionRadioGroup');
      assertTrue(!!radioGroupElement);
      const radioButton = radioGroupElement.get('buttons_')[0];
      assertTrue(!!radioButton);
      const deepLinkElement = radioButton.shadowRoot!.querySelector('#button');
      assert(deepLinkElement);

      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Encryption option should be focused for settingId=316.');
    });

    test('GAIA name and picture, account manager enabled', async () => {
      const fakeOsProfileName = 'Currently signed in as username';
      loadTimeData.overrideValues({
        isAccountManagerEnabled: true,
        // settings-account-manager-subpage requires this to have a value.
        secondaryGoogleAccountSigninAllowed: true,
        osProfileName: fakeOsProfileName,
      });
      createPage();

      await accountManagerBrowserProxy.whenCalled('getAccounts');
      await syncBrowserProxy.whenCalled('getSyncStatus');
      flush();

      // Get page elements.
      const profileIconEl =
          peoplePage.shadowRoot!.querySelector<HTMLElement>('#profile-icon');
      assertTrue(!!profileIconEl);
      const profileRowEl = peoplePage.shadowRoot!.querySelector('#profile-row');
      assertTrue(!!profileRowEl);
      const profileNameEl =
          peoplePage.shadowRoot!.querySelector('#profile-name');
      assertTrue(!!profileNameEl);

      assertStringContains(
          profileIconEl.style.backgroundImage,
          'data:image/png;base64,primaryAccountPicData');
      assertEquals(fakeOsProfileName, profileNameEl.textContent!.trim());

      // Rather than trying to mock sendWithPromise('getPluralString', ...)
      // just force an update.
      await peoplePage['updateAccounts_']();
      const profileLabelEl =
          peoplePage.shadowRoot!.querySelector('#profile-label');
      assertTrue(!!profileLabelEl);
      assertEquals('4 Google Accounts', profileLabelEl.textContent!.trim());

      // Profile row items are actionable.
      assertTrue(profileIconEl.hasAttribute('actionable'));
      assertTrue(profileRowEl.hasAttribute('actionable'));

      // Sub-page trigger is shown.
      const subpageTrigger =
          peoplePage.shadowRoot!.querySelector<CrIconButtonElement>(
              '#accountManagerSubpageTrigger');
      assertTrue(!!subpageTrigger);
      assertFalse(subpageTrigger.hidden);

      // Sub-page trigger navigates to Google account manager.
      subpageTrigger.click();
      assertEquals(routes.ACCOUNT_MANAGER, Router.getInstance().currentRoute);
    });

    const subpageTriggerData: SubpageTriggerData[] = [
      {
        triggerSelector: '#syncSetupRow',
        routeName: 'SYNC',
      },
      {
        triggerSelector: '#accountManagerSubpageTrigger',
        routeName: 'ACCOUNT_MANAGER',
      },
    ];
    subpageTriggerData.forEach(({triggerSelector, routeName}) => {
      test(
          `Row for ${routeName} is focused when returning from subpage`,
          async () => {
            loadTimeData.overrideValues({
              isAccountManagerEnabled: true,
              // settings-account-manager-subpage requires this to have a value.
              secondaryGoogleAccountSigninAllowed: true,
              osProfileName: 'Currently signed in as Walter White',
            });
            createPage();

            await accountManagerBrowserProxy.whenCalled('getAccounts');
            await syncBrowserProxy.whenCalled('getSyncStatus');
            flush();

            const subpageTrigger =
                peoplePage.shadowRoot!.querySelector<HTMLElement>(
                    triggerSelector);
            assertTrue(!!subpageTrigger);

            // Sub-page trigger navigates to subpage for route
            subpageTrigger.click();
            assertEquals(routes[routeName], Router.getInstance().currentRoute);

            // Navigate back
            const popStateEventPromise = eventToPromise('popstate', window);
            Router.getInstance().navigateToPreviousRoute();
            await popStateEventPromise;
            await waitAfterNextRender(peoplePage);

            assertEquals(
                subpageTrigger, peoplePage.shadowRoot!.activeElement,
                `${triggerSelector} should be focused.`);
          });
    });
  } else {
    test('account manager settings card is visible', async () => {
      createPage();

      await accountManagerBrowserProxy.whenCalled('getAccounts');
      await syncBrowserProxy.whenCalled('getSyncStatus');
      flush();

      const accountManagerSettingsCard =
          peoplePage.shadowRoot!.querySelector('account-manager-settings-card');

      assertTrue(isVisible(accountManagerSettingsCard));
    });

    test('additional accounts settings card is visible', async () => {
      createPage();

      await accountManagerBrowserProxy.whenCalled('getAccounts');
      await syncBrowserProxy.whenCalled('getSyncStatus');
      flush();

      const additionalAccountsSettingsCard =
          peoplePage.shadowRoot!.querySelector(
              'additional-accounts-settings-card');

      assertTrue(isVisible(additionalAccountsSettingsCard));
    });

    test(
        'parental controls settings card is visible when showParentalControls is enabled',
        async () => {
          loadTimeData.overrideValues({
            // Simulate parental controls.
            showParentalControls: true,
          });

          createPage();

          await accountManagerBrowserProxy.whenCalled('getAccounts');
          await syncBrowserProxy.whenCalled('getSyncStatus');
          flush();

          const parentalControlsSettingsCard =
              peoplePage.shadowRoot!.querySelector(
                  'parental-controls-settings-card');

          assertTrue(isVisible(parentalControlsSettingsCard));
        });

    test(
        'parental controls settings card is not shown when showParentalControls is disabled',
        async () => {
          loadTimeData.overrideValues({
            // Simulate parental controls.
            showParentalControls: false,
          });

          createPage();

          await accountManagerBrowserProxy.whenCalled('getAccounts');
          await syncBrowserProxy.whenCalled('getSyncStatus');
          flush();

          const parentalControlsSettingsCard =
              peoplePage.shadowRoot!.querySelector(
                  'parental-controls-settings-card');

          assertNull(parentalControlsSettingsCard);
        });

    if (isGraduationFlagEnabled) {
      test(
          'Graduation settings card is shown when app is enabled', async () => {
            loadTimeData.overrideValues({
              isGraduationAppEnabled: true,
            });

            createPage();
            await accountManagerBrowserProxy.whenCalled('getAccounts');
            await syncBrowserProxy.whenCalled('getSyncStatus');
            flush();

            const graduationSettingsCard = peoplePage.shadowRoot!.querySelector(
                'graduation-settings-card');
            assertTrue(isVisible(graduationSettingsCard));
          });

      test(
          'Graduation settings card is not shown when app is disabled',
          async () => {
            loadTimeData.overrideValues({
              isGraduationAppEnabled: false,
            });

            createPage();
            await accountManagerBrowserProxy.whenCalled('getAccounts');
            await syncBrowserProxy.whenCalled('getSyncStatus');
            flush();

            const graduationSettingsCard = peoplePage.shadowRoot!.querySelector(
                'graduation-settings-card');
            assertNull(graduationSettingsCard);
          });
    } else {
      test(
          'Graduation settings card is not shown when feature is disabled',
          async () => {
            createPage();
            await accountManagerBrowserProxy.whenCalled('getAccounts');
            await syncBrowserProxy.whenCalled('getSyncStatus');
            flush();

            const graduationSettingsCard = peoplePage.shadowRoot!.querySelector(
                'graduation-settings-card');
            assertNull(graduationSettingsCard);
          });
    }
  }
});
