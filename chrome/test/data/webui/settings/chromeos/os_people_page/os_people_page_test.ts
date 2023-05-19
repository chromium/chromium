// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {AccountManagerBrowserProxy, AccountManagerBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {createPageAvailabilityForTesting, OsSettingsPeoplePageElement, PageStatus, ProfileInfoBrowserProxy, ProfileInfoBrowserProxyImpl, Router, routes, settingMojom, SyncBrowserProxy, SyncBrowserProxyImpl} from 'chrome://os-settings/os_settings.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestSyncBrowserProxy} from '../test_os_sync_browser_proxy.js';
import {TestProfileInfoBrowserProxy} from '../test_profile_info_browser_proxy.js';

import {TestAccountManagerBrowserProxy} from './test_account_manager_browser_proxy.js';

suite('<os-settings-people-page>', () => {
  let peoplePage: OsSettingsPeoplePageElement;
  let browserProxy: ProfileInfoBrowserProxy&TestProfileInfoBrowserProxy;
  let syncBrowserProxy: SyncBrowserProxy&TestSyncBrowserProxy;
  let accountManagerBrowserProxy: AccountManagerBrowserProxy&
      TestAccountManagerBrowserProxy;

  setup(() => {
    browserProxy = new TestProfileInfoBrowserProxy();
    ProfileInfoBrowserProxyImpl.setInstance(browserProxy);

    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    accountManagerBrowserProxy = new TestAccountManagerBrowserProxy();
    AccountManagerBrowserProxyImpl.setInstanceForTesting(
        accountManagerBrowserProxy);
  });

  teardown(() => {
    peoplePage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Profile name and picture, account manager disabled', async () => {
    loadTimeData.overrideValues({
      isAccountManagerEnabled: false,
    });
    peoplePage = document.createElement('os-settings-people-page');
    peoplePage.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(peoplePage);

    await browserProxy.whenCalled('getProfileInfo');
    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();

    // Get page elements.
    const profileIconEl =
        peoplePage.shadowRoot!.querySelector<HTMLElement>('#profile-icon');
    assertTrue(!!profileIconEl);
    const profileRowEl = peoplePage.shadowRoot!.querySelector('#profile-row');
    assertTrue(!!profileRowEl);
    const profileNameEl = peoplePage.shadowRoot!.querySelector('#profile-name');
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
        '#account-manager-subpage-trigger');
    assertTrue(!!element);
    assertTrue(element.hidden);
  });

  test('parental controls page is shown when enabled', () => {
    loadTimeData.overrideValues({
      // Simulate parental controls.
      showParentalControls: true,
    });

    peoplePage = document.createElement('os-settings-people-page');
    document.body.appendChild(peoplePage);
    flush();

    // Setup button is shown and enabled.
    assert(peoplePage.shadowRoot!.querySelector(
        'settings-parental-controls-page'));
  });

  test('Deep link to parental controls page', async () => {
    loadTimeData.overrideValues({
      // Simulate parental controls.
      showParentalControls: true,
    });

    peoplePage = document.createElement('os-settings-people-page');
    document.body.appendChild(peoplePage);
    flush();

    const params = new URLSearchParams();
    params.append(
        'settingId', settingMojom.Setting.kSetUpParentalControls.toString());
    Router.getInstance().navigateTo(routes.OS_PEOPLE, params);

    const element =
        peoplePage.shadowRoot!.querySelector('settings-parental-controls-page');
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
    peoplePage = document.createElement('os-settings-people-page');
    document.body.appendChild(peoplePage);
    flush();

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
      appsRegistered: false,
      appsSynced: false,
      autofillRegistered: false,
      autofillSynced: false,
      bookmarksRegistered: false,
      bookmarksSynced: false,
      encryptAllData: false,
      extensionsRegistered: false,
      extensionsSynced: false,
      passwordsRegistered: false,
      passwordsSynced: false,
      paymentsIntegrationEnabled: false,
      preferencesRegistered: false,
      preferencesSynced: false,
      readingListRegistered: false,
      readingListSynced: false,
      syncAllDataTypes: false,
      tabsRegistered: false,
      tabsSynced: false,
      themesRegistered: false,
      themesSynced: false,
      trustedVaultKeysRequired: false,
      typedUrlsRegistered: false,
      typedUrlsSynced: false,
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
    peoplePage = document.createElement('os-settings-people-page');
    peoplePage.pageAvailability = createPageAvailabilityForTesting();
    document.body.appendChild(peoplePage);

    await accountManagerBrowserProxy.whenCalled('getAccounts');
    await syncBrowserProxy.whenCalled('getSyncStatus');
    flush();

    // Get page elements.
    const profileIconEl =
        peoplePage.shadowRoot!.querySelector<HTMLElement>('#profile-icon');
    assertTrue(!!profileIconEl);
    const profileRowEl = peoplePage.shadowRoot!.querySelector('#profile-row');
    assertTrue(!!profileRowEl);
    const profileNameEl = peoplePage.shadowRoot!.querySelector('#profile-name');
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
    assertEquals('3 Google Accounts', profileLabelEl.textContent!.trim());

    // Profile row items are actionable.
    assertTrue(profileIconEl.hasAttribute('actionable'));
    assertTrue(profileRowEl.hasAttribute('actionable'));

    // Sub-page trigger is shown.
    const subpageTrigger =
        peoplePage.shadowRoot!.querySelector<CrIconButtonElement>(
            '#account-manager-subpage-trigger');
    assertTrue(!!subpageTrigger);
    assertFalse(subpageTrigger.hidden);

    // Sub-page trigger navigates to Google account manager.
    subpageTrigger.click();
    assertEquals(routes.ACCOUNT_MANAGER, Router.getInstance().currentRoute);
  });
});
