// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {OpenWindowProxyImpl, PasswordManagerImpl, PrefToggleButtonElement, SyncBrowserProxyImpl, TrustedVaultBannerState} from 'chrome://password-manager/password_manager.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {createBlockedSiteEntry, createPasswordEntry, makePasswordManagerPrefs} from './test_util.js';

// clang-format off
// <if expr="is_win or is_macosx">
import {PasskeysBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';

import {TestPasskeysBrowserProxy} from './test_passkeys_browser_proxy.js';
// </if>
// clang-format on

/**
 * Helper method that validates a that elements in the exception list match
 * the expected data.
 * @param nodes The nodes that will be checked.
 * @param blockedSiteList The expected data.
 */
function assertBlockedSiteList(
    nodes: NodeListOf<HTMLElement>,
    blockedSiteList: chrome.passwordsPrivate.ExceptionEntry[]) {
  assertEquals(blockedSiteList.length, nodes.length);
  for (let index = 0; index < blockedSiteList.length; ++index) {
    const node = nodes[index]!;
    const blockedSite = blockedSiteList[index]!;
    assertEquals(blockedSite.urls.shown, node.textContent!.trim());
  }
}

suite('SettingsSectionTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let syncProxy: TestSyncBrowserProxy;
  // <if expr="is_win or is_macosx">
  let passkeysProxy: TestPasskeysBrowserProxy;
  // </if>

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);
    // <if expr="is_win or is_macosx">
    passkeysProxy = new TestPasskeysBrowserProxy();
    PasskeysBrowserProxyImpl.setInstance(passkeysProxy);
    // </if>
  });

  test('pref value displayed in the UI', async function() {
    const settings = document.createElement('settings-section');
    settings.prefs = makePasswordManagerPrefs();
    settings.prefs.credentials_enable_service.value = false;
    document.body.appendChild(settings);
    await flushTasks();

    assertFalse(settings.$.passwordToggle.checked);
    assertTrue(settings.$.autosigninToggle.checked);
  });

  test('clicking the toggle updates corresponding pref', async function() {
    const settings = document.createElement('settings-section');
    settings.prefs = makePasswordManagerPrefs();
    document.body.appendChild(settings);
    await flushTasks();

    assertTrue(settings.getPref('credentials_enable_service').value);
    assertTrue(settings.$.passwordToggle.checked);

    settings.$.passwordToggle.click();
    assertFalse(settings.getPref('credentials_enable_service').value);
    assertFalse(settings.$.passwordToggle.checked);
  });

  test('enforcement disables toggle', async function() {
    const settings = document.createElement('settings-section');
    settings.prefs = makePasswordManagerPrefs();
    settings.prefs.credentials_enable_service.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    document.body.appendChild(settings);
    await flushTasks();

    assertTrue(settings.getPref('credentials_enable_service').value);
    assertTrue(settings.$.passwordToggle.checked);
    settings.$.passwordToggle.click();
    assertTrue(settings.getPref('credentials_enable_service').value);
  });

  test('extension control includes icon', async function() {
    const settings = document.createElement('settings-section');
    settings.prefs = makePasswordManagerPrefs();
    settings.prefs.credentials_enable_service.extensionId = 'test';
    settings.prefs.credentials_enable_service.controlledByName =
        'test extension';
    document.body.appendChild(settings);
    await flushTasks();

    assertTrue(settings.$.passwordToggle.checked);
    assertTrue(
        !!settings.shadowRoot!.querySelector('extension-controlled-indicator'));
  });

  test('no extension control icon by default', async function() {
    const settings = document.createElement('settings-section');
    settings.prefs = makePasswordManagerPrefs();
    document.body.appendChild(settings);
    await flushTasks();

    assertTrue(settings.$.passwordToggle.checked);
    settings.$.passwordToggle.click();
    assertFalse(!!settings.$.passwordToggle.shadowRoot!.querySelector(
        'extension-controlled-indicator'));
  });

  test('pref updated externally', async function() {
    const settings = document.createElement('settings-section');
    settings.prefs = makePasswordManagerPrefs();
    document.body.appendChild(settings);
    await flushTasks();

    assertTrue(settings.$.autosigninToggle.checked);
    settings.set('prefs.credentials_enable_autosignin.value', false);

    assertFalse(settings.$.autosigninToggle.checked);
  });

  // <if expr="is_win or is_macosx">
  // Tests that biometric auth pref is visible, and clicking on it triggers
  // biometric auth validation instead of directly updating the pref value.
  test('biometric auth prefs when feature is available', async function() {
    loadTimeData.overrideValues(
        {biometricAuthenticationForFillingToggleVisible: true});

    const settings = document.createElement('settings-section');
    settings.prefs = makePasswordManagerPrefs();
    settings.prefs.password_manager.biometric_authentication_filling.value =
        false;
    document.body.appendChild(settings);
    await flushTasks();

    const biometricAuthenticationToggle =
        settings.shadowRoot!.querySelector<PrefToggleButtonElement>(
            '#biometricAuthenticationToggle') as PrefToggleButtonElement;
    assertTrue(!!biometricAuthenticationToggle);
    assertFalse(biometricAuthenticationToggle.checked);
    assertFalse(
        settings.getPref('password_manager.biometric_authentication_filling')
            .value);

    biometricAuthenticationToggle.click();

    // Pref settings should not change until authentication succeeds.
    await passwordManager.whenCalled('switchBiometricAuthBeforeFillingState');
    assertFalse(biometricAuthenticationToggle.checked);
    assertFalse(
        settings.getPref('password_manager.biometric_authentication_filling')
            .value);
  });

  // Tests that biometric auth pref is not shown, if biometric auth is
  // unavailable.
  test('biometric auth prefs when feature is unavailable', async function() {
    loadTimeData.overrideValues(
        {biometricAuthenticationForFillingToggleVisible: false});
    const settings = document.createElement('settings-section');
    settings.prefs = makePasswordManagerPrefs();
    document.body.appendChild(settings);
    await flushTasks();
    assertFalse(!!settings.shadowRoot!.querySelector<PrefToggleButtonElement>(
        '#biometricAuthenticationToggle'));
  });
  // </if>

  test('settings section shows blockedSites', async function() {
    passwordManager.data.blockedSites = [
      createBlockedSiteEntry('test.com', 0),
      createBlockedSiteEntry('test2.com', 1),
    ];
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();
    await passwordManager.whenCalled('getBlockedSitesList');

    assertTrue(isVisible(settings.$.blockedSitesList));
    assertBlockedSiteList(
        settings.$.blockedSitesList.querySelectorAll<HTMLElement>(
            '.blocked-site-content'),
        passwordManager.data.blockedSites);
  });

  test('blockedSites can be deleted', async function() {
    const blockedId = 1;
    passwordManager.data.blockedSites =
        [createBlockedSiteEntry('test.com', blockedId)];
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();
    await passwordManager.whenCalled('getBlockedSitesList');
    assertTrue(isVisible(settings.$.blockedSitesList));

    settings.$.blockedSitesList
        .querySelector<HTMLElement>('#removeBlockedValueButton')!.click();
    const removedId = await passwordManager.whenCalled('removeBlockedSite');
    assertEquals(blockedId, removedId);
  });

  test('blockedSites listener updates the list', async function() {
    passwordManager.data.blockedSites = [createBlockedSiteEntry('test.com', 1)];
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();
    await passwordManager.whenCalled('getBlockedSitesList');
    // Check that only one entry is shown.
    assertTrue(isVisible(settings.$.blockedSitesList));
    assertEquals(
        settings.$.blockedSitesList
            .querySelectorAll<HTMLElement>('.blocked-site-content')
            .length,
        1);

    passwordManager.data.blockedSites.push(
        createBlockedSiteEntry('test2.com', 2));
    passwordManager.listeners.blockedSitesListChangedListener!
        (passwordManager.data.blockedSites);
    await flushTasks();
    // Check that two entries are shown.
    assertTrue(isVisible(settings.$.blockedSitesList));
    assertEquals(
        settings.$.blockedSitesList
            .querySelectorAll<HTMLElement>('.blocked-site-content')
            .length,
        2);
  });

  // Add Shortcut banner is shown and clickable.
  test('showAddShortcutBanner', async function() {
    loadTimeData.overrideValues({canAddShortcut: true});

    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();

    const addShortcutBanner =
        settings.shadowRoot!.querySelector<HTMLElement>('#addShortcutBanner');
    assertTrue(!!addShortcutBanner);
    assertTrue(isVisible(addShortcutBanner));
    addShortcutBanner.click();
    await passwordManager.whenCalled('showAddShortcutDialog');
  });

  // Add Shortcut banner is shown and clickable.
  test('addShortcutBanner hidden', async function() {
    loadTimeData.overrideValues({canAddShortcut: false});

    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();

    assertFalse(!!settings.shadowRoot!.querySelector('#addShortcutBanner'));
  });

  test('import hidden when policy disabled', async function() {
    const settings = document.createElement('settings-section');
    settings.prefs = makePasswordManagerPrefs();
    settings.prefs.credentials_enable_service.value = false;
    settings.prefs.credentials_enable_service.enforcement =
        chrome.settingsPrivate.Enforcement.ENFORCED;
    document.body.appendChild(settings);
    await flushTasks();

    assertFalse(!!settings.shadowRoot!.querySelector('passwords-importer'));
  });

  test('Password exporter element', async function() {
    // Exporter should not be present if there are no saved passwords.
    passwordManager.data.passwords = [];
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await passwordManager.whenCalled('getSavedPasswordList');
    assertFalse(!!settings.shadowRoot!.querySelector('passwords-exporter'));

    // Exporter should appear when saved passwords are observed.
    passwordManager.data.passwords.push(
        createPasswordEntry({username: 'user1', id: 1}));
    passwordManager.listeners.savedPasswordListChangedListener!
        (passwordManager.data.passwords);
    flush();
    assertTrue(!!settings.shadowRoot!.querySelector('passwords-exporter'));
  });

  test('trustedVaultBannerVisibilityChangesWithState', async function() {
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    webUIListenerCallback(
        'trusted-vault-banner-state-changed',
        TrustedVaultBannerState.NOT_SHOWN);
    flush();
    assertTrue(settings.$.trustedVaultBanner.hidden);

    webUIListenerCallback(
        'trusted-vault-banner-state-changed',
        TrustedVaultBannerState.OFFER_OPT_IN);
    flush();
    assertFalse(settings.$.trustedVaultBanner.hidden);
    assertEquals(
        settings.i18n('trustedVaultBannerSubLabelOfferOptIn'),
        settings.$.trustedVaultBanner.subLabel);

    webUIListenerCallback(
        'trusted-vault-banner-state-changed', TrustedVaultBannerState.OPTED_IN);
    flush();
    assertFalse(settings.$.trustedVaultBanner.hidden);
    assertEquals(
        settings.i18n('trustedVaultBannerSubLabelOptedIn'),
        settings.$.trustedVaultBanner.subLabel);
  });

  test('trustedVaultBannerOpensOptInPage', async function() {
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    webUIListenerCallback(
        'trusted-vault-banner-state-changed',
        TrustedVaultBannerState.OFFER_OPT_IN);
    flush();
    assertFalse(settings.$.trustedVaultBanner.hidden);

    settings.$.trustedVaultBanner.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('trustedVaultOptInUrl'));
  });

  test('trustedVaultBannerOpensLearnMorePage', async function() {
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    webUIListenerCallback(
        'trusted-vault-banner-state-changed', TrustedVaultBannerState.OPTED_IN);
    flush();
    assertFalse(settings.$.trustedVaultBanner.hidden);

    settings.$.trustedVaultBanner.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('trustedVaultLearnMoreUrl'));
  });

  test('account storage toggle when feature is available', async function() {
    passwordManager.data.isOptedInAccountStorage = false;
    syncProxy.accountInfo = {
      email: 'testemail@gmail.com',
    };
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await syncProxy.whenCalled('getSyncInfo');
    await syncProxy.whenCalled('getAccountInfo');
    await flushTasks();
    await flushTasks();

    const accountStorageToggle =
        settings.shadowRoot!.querySelector<PrefToggleButtonElement>(
            '#accountStorageToggle');
    assertTrue(!!accountStorageToggle);
    assertFalse(accountStorageToggle.hasAttribute('checked'));
    accountStorageToggle.click();

    // Toggle should not change until authentication succeeds.
    await passwordManager.whenCalled('optInForAccountStorage');
    assertFalse(accountStorageToggle.hasAttribute('checked'));

    // Assert that password section subscribed as a listener to opt in state and
    // opt out from account storage.
    assertTrue(!!passwordManager.listeners.accountStorageOptInStateListener);
    passwordManager.data.isOptedInAccountStorage = true;
    // Imitate listener notification after successful identification.
    passwordManager.listeners.accountStorageOptInStateListener(true);
    await flushTasks();

    assertTrue(accountStorageToggle.checked);
  });

  // Tests that account storage toggle is not shown, if it should not be shown.
  test(
      'account storage pref toggle when feature is unavailable',
      async function() {
        syncProxy.syncInfo = {
          isEligibleForAccountStorage: false,
          isSyncingPasswords: false,
        };
        const settings = document.createElement('settings-section');
        document.body.appendChild(settings);
        await syncProxy.whenCalled('getSyncInfo');
        await flushTasks();

        assertFalse(
            !!settings.shadowRoot!.querySelector<PrefToggleButtonElement>(
                '#accountStorageToggle'));
      });

  // <if expr="is_win or is_macosx">
  test('managePasskeysNotShownWithoutPasskeys', async function() {
    passkeysProxy.passkeysPresent = false;
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await passkeysProxy.whenCalled('passkeysHasPasskeys');
    flush();
    assertFalse(!!settings.shadowRoot!.querySelector('#managePasskeysRow'));
  });

  test('managePasskeysShownWhenNeeded', async function() {
    passkeysProxy.passkeysPresent = true;
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await passkeysProxy.whenCalled('passkeysHasPasskeys');
    flush();
    const managePasskeysRow = settings.shadowRoot!.querySelector<HTMLElement>(
                                  '#managePasskeysRow') as HTMLElement;
    assertTrue(!!managePasskeysRow);

    managePasskeysRow.click();
    await passkeysProxy.whenCalled('passkeysManagePasskeys');
  });
  // </if>

  test('iCloudKeychainToggleNotShown', async function() {
    // The control for iCloud Keychain should appear only on macOS.
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    flush();
    const element = settings.shadowRoot!.querySelector<HTMLElement>(
                        '#createPasskeysInICloudKeychainRow') as HTMLElement;

    // <if expr="not is_macosx">
    assertFalse(!!element);
    // </if>

    // <if expr="is_macosx">
    assertTrue(!!element);
    // </if>
  });

  test('blockedSites section hidden when no blocked sites', async function() {
    passwordManager.data.blockedSites = [];
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();
    await passwordManager.whenCalled('getBlockedSitesList');

    assertFalse(isVisible(settings.$.blockedSitesList));
  });
});
