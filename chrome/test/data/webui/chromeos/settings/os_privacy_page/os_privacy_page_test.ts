// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrivacyHubBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrDialogElement, createRouterForTesting, CrRadioGroupElement, GeolocationAccessLevel, OsSettingsPrivacyPageElement, PageStatus, PeripheralDataAccessBrowserProxyImpl, Router, routes, SecureDnsMode, settingMojom, SettingsToggleButtonElement, SyncBrowserProxy, SyncBrowserProxyImpl} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';
import {FakeQuickUnlockPrivate} from '../fake_quick_unlock_private.js';
import {TestSyncBrowserProxy} from '../test_os_sync_browser_proxy.js';

import {TestPeripheralDataAccessBrowserProxy} from './test_peripheral_data_access_browser_proxy.js';
import {TestPrivacyHubBrowserProxy} from './test_privacy_hub_browser_proxy.js';

const CROS_SETTING_PREF_NAME = 'cros.device.peripheral_data_access_enabled';
const LOCAL_STATE_PREF_NAME =
    'settings.local_state_device_pci_data_access_enabled';

suite('with isRevampWayfindingEnabled set to true', () => {
  let privacyPage: OsSettingsPrivacyPageElement;
  let browserProxy: TestPeripheralDataAccessBrowserProxy;
  let syncBrowserProxy: SyncBrowserProxy&TestSyncBrowserProxy;

  async function createPage(): Promise<void> {
    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);
    flush();

    await browserProxy.whenCalled('isThunderboltSupported');
  }

  setup(() => {
    loadTimeData.overrideValues({
      isRevampWayfindingEnabled: true,
    });
    const testRouter = createRouterForTesting();
    Router.resetInstanceForTesting(testRouter);

    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    PeripheralDataAccessBrowserProxyImpl.setInstanceForTesting(browserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);

    Router.getInstance().navigateTo(routes.OS_PRIVACY);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    privacyPage.remove();
  });

  test('Deep link to encryption options on old sync page', async () => {
    createPage();
    await waitAfterNextRender(privacyPage);

    // Load the sync page.
    Router.getInstance().navigateTo(routes.SYNC);
    await flushTasks();

    // Make the sync page configurable.
    const syncPage =
        privacyPage.shadowRoot!.querySelector('os-settings-sync-subpage');
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
        `#${PageStatus.CONFIGURE}`);
    assertTrue(!!configureElement);
    assertFalse(configureElement.hidden);
    const spinnerElement = syncPage.shadowRoot!.querySelector<HTMLElement>(
        `#${PageStatus.SPINNER}`);
    assertTrue(!!spinnerElement);
    assertTrue(spinnerElement.hidden);

    // Try the deep link.
    const params = new URLSearchParams();
    const syncEncryptionOptionsSettingId =
        settingMojom.Setting.kNonSplitSyncEncryptionOptions.toString();
    params.append('settingId', syncEncryptionOptionsSettingId);
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
        `Encryption option should be focused for settingId=${
            syncEncryptionOptionsSettingId}.`);
  });
});

suite('<os-settings-privacy-page>', () => {
  let privacyPage: OsSettingsPrivacyPageElement;
  let browserProxy: TestPeripheralDataAccessBrowserProxy;

  const PRIVACY_PAGE_PREFS = {
    'ash': {
      'user': {
        'camera_allowed': {
          value: true,
        },
        'microphone_allowed': {
          value: true,
        },
        'geolocation_access_level': {
          key: 'ash.user.geolocation_access_level',
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: GeolocationAccessLevel.ALLOWED,
        },
      },
    },
    'settings': {
      'suggested_content_enabled': {
        value: false,
      },
    },
    'cros': {
      'device': {
        'peripheral_data_access_enabled': {
          value: true,
        },
      },
    },
    'dns_over_https': {
      'mode': {
        value: SecureDnsMode.AUTOMATIC,
      },
      'templates': {
        value: '',
      },
    },
  };

  setup(async () => {
    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    PeripheralDataAccessBrowserProxyImpl.setInstanceForTesting(browserProxy);
    privacyPage = document.createElement('os-settings-privacy-page');
    privacyPage.prefs = Object.assign({}, PRIVACY_PAGE_PREFS);
    document.body.appendChild(privacyPage);
    flush();

    await browserProxy.whenCalled('isThunderboltSupported');
  });

  teardown(() => {
    privacyPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * Returns true if the element exists and has not been 'removed' by the
   * Polymer template system.
   * @param selector The ID of the element about which to query.
   * @return Whether or not the element has been masked by the
   *                   template system.
   */
  function elementExists(selector: string): boolean {
    const el = privacyPage.shadowRoot!.querySelector<HTMLElement>(selector);
    return (el !== null) && (el.style.display !== 'none');
  }

  test(
      'Suggested content, hidden when privacy hub feature flag is enabled',
      () => {
        privacyPage = document.createElement('os-settings-privacy-page');
        document.body.appendChild(privacyPage);
        flush();

        assertFalse(elementExists('#contentRecommendationsToggle'));
      });

  test('Deep link to verified access', async () => {
    const params = new URLSearchParams();
    params.append('settingId', settingMojom.Setting.kVerifiedAccess.toString());
    Router.getInstance().navigateTo(routes.OS_PRIVACY, params);

    flush();

    const toggle =
        privacyPage.shadowRoot!.querySelector('#verifiedAccessToggle');
    assertTrue(!!toggle);
    const deepLinkElement = toggle.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Verified access toggle should be focused for settingId=1101.');
  });

  test('Deep link to guest browsing on users page', async () => {
    const params = new URLSearchParams();
    params.append(
        'settingId', settingMojom.Setting.kGuestBrowsingV2.toString());
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    flush();

    const element =
        privacyPage.shadowRoot!.querySelector('settings-manage-users-subpage');
    assertTrue(!!element);
    const toggle = element.shadowRoot!.querySelector('#allowGuestBrowsing');
    assertTrue(!!toggle);
    const deepLinkElement = toggle.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Allow guest browsing should be focused for settingId=1104.');
  });

  test('Deep link to show usernames on sign in on users page', async () => {
    const params = new URLSearchParams();
    params.append(
        'settingId',
        settingMojom.Setting.kShowUsernamesAndPhotosAtSignInV2.toString());
    Router.getInstance().navigateTo(routes.ACCOUNTS, params);

    flush();

    const element =
        privacyPage.shadowRoot!.querySelector('settings-manage-users-subpage');
    assertTrue(!!element);
    const toggle = element.shadowRoot!.querySelector('#showUserNamesOnSignIn');
    assertTrue(!!toggle);
    const deepLinkElement = toggle.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Allow guest browsing should be focused for settingId=1105.');
  });

  test('Deep link to snooping protection on smart privacy page', async () => {
    loadTimeData.overrideValues({
      isSnoopingProtectionEnabled: true,
    });

    const params = new URLSearchParams();
    params.append(
        'settingId', settingMojom.Setting.kSnoopingProtection.toString());
    Router.getInstance().navigateTo(routes.SMART_PRIVACY, params);

    flush();

    const element =
        privacyPage.shadowRoot!.querySelector('settings-smart-privacy-subpage');
    assertTrue(!!element);
    const toggle =
        element.shadowRoot!.querySelector('#snoopingProtectionToggle');
    assertTrue(!!toggle);
    const deepLinkElement = toggle.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Snooping protection should be focused for settingId=1114.');
  });

  test('Deep link to quick dim on smart privacy page', async () => {
    loadTimeData.overrideValues({
      isQuickDimEnabled: true,
    });

    const params = new URLSearchParams();
    params.append('settingId', settingMojom.Setting.kQuickDim.toString());
    Router.getInstance().navigateTo(routes.SMART_PRIVACY, params);

    flush();

    const element =
        privacyPage.shadowRoot!.querySelector('settings-smart-privacy-subpage');
    assertTrue(!!element);
    const quickDimToggle = element.shadowRoot!.querySelector('#quickDimToggle');
    assertTrue(!!quickDimToggle);
    const deepLinkElement =
        quickDimToggle.shadowRoot!.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Quick dim should be focused for settingId=1115.');
  });

  test(
      'Manage accounts row is focused when returning from subpage',
      async () => {
        Router.getInstance().navigateTo(routes.OS_PRIVACY);

        const triggerSelector = '#manageOtherPeopleRow';
        const subpageTrigger =
            privacyPage.shadowRoot!.querySelector<HTMLElement>(triggerSelector);
        assertTrue(!!subpageTrigger);

        // Sub-page trigger navigates to subpage for route
        subpageTrigger.click();
        assertEquals(routes.ACCOUNTS, Router.getInstance().currentRoute);

        // Navigate back
        const popStateEventPromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await popStateEventPromise;
        await waitAfterNextRender(privacyPage);

        assertEquals(
            subpageTrigger, privacyPage.shadowRoot!.activeElement,
            `${triggerSelector} should be focused.`);
      });

  test('Lock screen row is focused when returning from subpage', async () => {
    Router.getInstance().navigateTo(routes.OS_PRIVACY);

    const tokenLabel = loadTimeData.getBoolean('isAuthPanelEnabled') ?
        'authTokenReply_' :
        'authTokenInfo_';

    const quickUnlockPrivateApi = new FakeQuickUnlockPrivate();
    privacyPage.set(tokenLabel, quickUnlockPrivateApi.getFakeToken());

    const triggerSelector = '#lockScreenRow';
    const subpageTrigger =
        privacyPage.shadowRoot!.querySelector<HTMLElement>(triggerSelector);
    assertTrue(!!subpageTrigger);

    // Sub-page trigger navigates to subpage for route
    subpageTrigger.click();
    assertEquals(routes.LOCK_SCREEN, Router.getInstance().currentRoute);

    // Navigate back
    const popStateEventPromise = eventToPromise('popstate', window);
    Router.getInstance().navigateToPreviousRoute();
    await popStateEventPromise;
    await waitAfterNextRender(privacyPage);

    assertEquals(
        subpageTrigger, privacyPage.shadowRoot!.activeElement,
        `${triggerSelector} should be focused.`);
  });

  test('Fingerprint dialog closes when token expires', async () => {
    loadTimeData.overrideValues({
      fingerprintUnlockEnabled: true,
    });

    const tokenLabel = loadTimeData.getBoolean('isAuthPanelEnabled') ?
        'authTokenReply_' :
        'authTokenInfo_';

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    const quickUnlockPrivateApi = new FakeQuickUnlockPrivate();
    privacyPage[tokenLabel] = quickUnlockPrivateApi.getFakeToken();

    Router.getInstance().navigateTo(routes.LOCK_SCREEN);
    flush();

    const subpageTrigger =
        privacyPage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#lockScreenRow');
    assertTrue(!!subpageTrigger);
    // Sub-page trigger navigates to the lock screen page.
    subpageTrigger.click();
    flush();

    assertEquals(routes.LOCK_SCREEN, Router.getInstance().currentRoute);
    const lockScreenPage = privacyPage.shadowRoot!.querySelector('#lockScreen');
    assertTrue(!!lockScreenPage);

    // Password dialog should not open because the authTokenInfo_ is set.
    assertFalse(privacyPage.get('showPasswordPromptDialog_'));

    const editFingerprintsTrigger =
        lockScreenPage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#editFingerprints');
    assertTrue(!!editFingerprintsTrigger);
    editFingerprintsTrigger.click();
    flush();

    assertEquals(routes.FINGERPRINT, Router.getInstance().currentRoute);
    assertFalse(privacyPage.get('showPasswordPromptDialog_'));

    const element = privacyPage.shadowRoot!.querySelector('#fingerprint-list');
    assertTrue(!!element);
    const fingerprintTrigger =
        element.shadowRoot!.querySelector<HTMLButtonElement>('#addFingerprint');
    assertTrue(!!fingerprintTrigger);
    fingerprintTrigger.click();

    // Invalidate the auth token by firing an event.
    assertNotEquals(undefined, privacyPage.get(tokenLabel));
    const event = new CustomEvent('invalidate-auth-token-requested');
    lockScreenPage.dispatchEvent(event);
    assertEquals(undefined, privacyPage.get(tokenLabel));

    assertEquals(routes.FINGERPRINT, Router.getInstance().currentRoute);

    if (!loadTimeData.getBoolean('isAuthPanelEnabled')) {
      assertTrue(privacyPage.get('showPasswordPromptDialog_'));
    }
  });

  test('Smart privacy hidden when both features disabled', async () => {
    loadTimeData.overrideValues({
      isSnoopingProtectionEnabled: false,
      isQuickDimEnabled: false,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    assertFalse(elementExists('#smartPrivacySubpageTrigger'));
  });

  test('Smart privacy shown when only quick dim enabled', async () => {
    loadTimeData.overrideValues({
      isSnoopingProtectionEnabled: false,
      isQuickDimEnabled: true,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    assertTrue(elementExists('#smartPrivacySubpageTrigger'));
  });

  test('Smart privacy shown if only snooping protection enabled', async () => {
    loadTimeData.overrideValues({
      isSnoopingProtectionEnabled: true,
      isQuickDimEnabled: false,
    });

    privacyPage = document.createElement('os-settings-privacy-page');
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    assertTrue(elementExists('#smartPrivacySubpageTrigger'));
  });

  test('Open PrivacyHub', async () => {
    const privacyHubBrowserProxy = new TestPrivacyHubBrowserProxy();
    PrivacyHubBrowserProxyImpl.setInstanceForTesting(privacyHubBrowserProxy);

    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;
    privacyPage = document.createElement('os-settings-privacy-page');
    privacyPage.prefs = Object.assign({}, PRIVACY_PAGE_PREFS);
    document.body.appendChild(privacyPage);

    await waitAfterNextRender(privacyPage);

    const privacyHubPageRow =
        privacyPage.shadowRoot!.querySelector<HTMLButtonElement>(
            '#privacyHubSubpageTrigger');
    assertTrue(!!privacyHubPageRow);

    assertEquals(
        0,
        fakeMetricsPrivate.countMetricValue('ChromeOS.PrivacyHub.Opened', 0));

    privacyHubPageRow.click();
    flush();
    await waitAfterNextRender(privacyPage);

    assertEquals(
        1,
        fakeMetricsPrivate.countMetricValue('ChromeOS.PrivacyHub.Opened', 0));
  });

  // TODO(crbug.com/40202733): add a test for deep linking to snopping setting
  //                          once it has been added.
});

suite('PeripheralDataAccessTest', () => {
  let privacyPage: OsSettingsPrivacyPageElement;
  let browserProxy: TestPeripheralDataAccessBrowserProxy;

  const prefs_ = {
    'cros': {
      'device': {
        'peripheral_data_access_enabled': {
          value: false,
        },
      },
    },
    'settings': {'local_state_device_pci_data_access_enabled': {value: false}},
    'dns_over_https':
        {'mode': {value: SecureDnsMode.AUTOMATIC}, 'templates': {value: ''}},
  };

  setup(() => {
    browserProxy = new TestPeripheralDataAccessBrowserProxy();
    PeripheralDataAccessBrowserProxyImpl.setInstanceForTesting(browserProxy);
  });

  teardown(() => {
    privacyPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  async function setUpPage(prefName: string, isUserConfigurable: boolean) {
    browserProxy.setPolicyState(prefName, isUserConfigurable);
    privacyPage = document.createElement('os-settings-privacy-page');
    privacyPage.prefs = Object.assign({}, prefs_);
    document.body.appendChild(privacyPage);
    flush();

    await browserProxy.whenCalled('getPolicyState');
    await waitAfterNextRender(privacyPage);
    flush();
  }

  test('DialogOpensOnToggle', async () => {
    await setUpPage(CROS_SETTING_PREF_NAME, /**isUserConfigurable=*/ true);
    // The default state is checked.
    const toggle =
        privacyPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#crosSettingDataAccessToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    flush();

    await waitAfterNextRender(privacyPage);

    const protectionDialog =
        privacyPage.shadowRoot!.querySelector('#protectionDialog');
    assertTrue(!!protectionDialog);
    const dialog = protectionDialog.shadowRoot!.querySelector<CrDialogElement>(
        '#warningDialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    // Ensure that the toggle is still checked.
    assertTrue(toggle.checked);

    // Click on the dialog's cancel button and expect the toggle to switch back
    // to enabled.
    const cancelButton =
        dialog.querySelector<HTMLButtonElement>('#cancelButton');
    assertTrue(!!cancelButton);
    cancelButton.click();
    flush();
    assertFalse(dialog.open);

    // The toggle should not have changed position.
    assertTrue(toggle.checked);
  });

  test('DisableClicked', async () => {
    await setUpPage(CROS_SETTING_PREF_NAME, /**isUserConfigurable=*/ true);
    // The default state is checked.
    const toggle =
        privacyPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#crosSettingDataAccessToggle');
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    flush();

    await waitAfterNextRender(privacyPage);

    const protectionDialog =
        privacyPage.shadowRoot!.querySelector('#protectionDialog');
    assertTrue(!!protectionDialog);
    const dialog = protectionDialog.shadowRoot!.querySelector<CrDialogElement>(
        '#warningDialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    // Advance the dialog and move onto the next dialog.
    const disableButton =
        dialog.querySelector<HTMLButtonElement>('#disableConfirmation');
    assertTrue(!!disableButton);
    disableButton.click();
    flush();

    // The toggle should now be flipped to unset.
    assertFalse(toggle.checked);
  });

  test('managedAndConfigurablePrefIsToggleable', async () => {
    await setUpPage(LOCAL_STATE_PREF_NAME, /**isUserConfigurable=*/ true);
    flush();

    // Ensure only the local state toggle appears.
    const crosSettingDataAccessToggle =
        privacyPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#crosSettingDataAccessToggle');
    assertTrue(!!crosSettingDataAccessToggle);
    assertTrue(crosSettingDataAccessToggle.hidden);

    // The default state is checked.
    const toggle =
        privacyPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#localStateDataAccessToggle');

    // The default state is checked.
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    flush();

    await waitAfterNextRender(privacyPage);

    const protectionDialog =
        privacyPage.shadowRoot!.querySelector('#protectionDialog');
    assertTrue(!!protectionDialog);
    const dialog = protectionDialog.shadowRoot!.querySelector<CrDialogElement>(
        '#warningDialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    // Ensure that the toggle is still checked.
    assertTrue(toggle.checked);

    // Click on the dialog's cancel button and expect the toggle to switch back
    // to enabled.
    const cancelButton =
        dialog.querySelector<HTMLButtonElement>('#cancelButton');
    assertTrue(!!cancelButton);
    cancelButton.click();
    flush();
    assertFalse(dialog.open);

    // The toggle should not have changed position.
    assertTrue(toggle.checked);
  });

  test('managedAndNonConfigurablePrefIsNotToggleable', async () => {
    await setUpPage(LOCAL_STATE_PREF_NAME, /**isUserConfigurable=*/ false);
    flush();

    // Ensure only the local state toggle appears.
    const crosSettingDataAccessToggle =
        privacyPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#crosSettingDataAccessToggle');
    assertTrue(!!crosSettingDataAccessToggle);
    assertTrue(crosSettingDataAccessToggle.hidden);

    // The default state is checked.
    const toggle =
        privacyPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#localStateDataAccessToggle');

    // The default state is checked.
    assertTrue(!!toggle);
    assertTrue(toggle.checked);

    // Attempting to switch the toggle off will result in the warning dialog
    // appearing.
    toggle.click();
    flush();

    await waitAfterNextRender(privacyPage);

    // Dialog should not appear since the toggle is disabled.
    const dialog = privacyPage.shadowRoot!.querySelector('#protectionDialog');
    assertNull(dialog);
  });
});
