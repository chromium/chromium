// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl, PrefsBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {createBlockedSiteEntry, createPasswordEntry, makePasswordManagerPrefs} from './test_util.js';

// Disable clang format to keep OS-specific includes.
// clang-format off
// <if expr="is_win or is_macosx">
import {PrefToggleButtonElement} from 'chrome://password-manager/password_manager.js';
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
  let prefsProxy: TestPrefsBrowserProxy;
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    prefsProxy = new TestPrefsBrowserProxy();
    PrefsBrowserProxyImpl.setInstance(prefsProxy);
    prefsProxy.prefs = makePasswordManagerPrefs();
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
  });

  test('pref value displayed in the UI', async function() {
    await prefsProxy.setPref('credentials_enable_service', false);

    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await prefsProxy.whenCalled('getPref');

    assertFalse(settings.$.passwordToggle.checked);
    assertTrue(settings.$.autosigninToggle.checked);
  });

  test('clicking the toggle updates corresponding', async function() {
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await prefsProxy.whenCalled('getPref');

    assertTrue(settings.$.passwordToggle.checked);
    settings.$.passwordToggle.click();
    assertFalse(settings.$.passwordToggle.checked);

    const {key, value} = await prefsProxy.whenCalled('setPref');
    assertEquals('credentials_enable_service', key);
    assertFalse(value);
  });

  test('pref updated externally', async function() {
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await prefsProxy.whenCalled('getPref');

    assertTrue(settings.$.autosigninToggle.checked);
    prefsProxy.prefs = makePasswordManagerPrefs();
    await prefsProxy.setPref('credentials_enable_autosignin', false);

    assertFalse(settings.$.autosigninToggle.checked);
  });

  // <if expr="is_win or is_macosx">
  // Tests that biometric auth pref is visible, and clicking on it triggers
  // biometric auth validation instead of directly updating the pref value.
  test('biometric auth prefs when feature is available', async function() {
    await prefsProxy.setPref(
        'password_manager.biometric_authentication_filling', false);
    loadTimeData.overrideValues(
        {biometricAuthenticationForFillingToggleVisible: true});

    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await prefsProxy.whenCalled('getPref');
    await flushTasks();

    const biometricAuthenticationToggle =
        settings.shadowRoot!.querySelector<PrefToggleButtonElement>(
            '#biometricAuthenticationToggle') as PrefToggleButtonElement;
    assertTrue(!!biometricAuthenticationToggle);
    assertFalse(biometricAuthenticationToggle.checked);
    biometricAuthenticationToggle.click();

    // Pref settings should not change until authentication succeeds.
    await passwordManager.whenCalled('switchBiometricAuthBeforeFillingState');
    assertFalse(biometricAuthenticationToggle.checked);

    // Imitate prefs changing after successful identification.
    prefsProxy.prefs = makePasswordManagerPrefs();
    await prefsProxy.setPref(
        'password_manager.biometric_authentication_filling', true);
    assertTrue(biometricAuthenticationToggle.checked);
  });

  // Tests that biometric auth pref is not shown, if biometric auth is
  // unavailable.
  test('biometric auth prefs when feature is unavailable', async function() {
    loadTimeData.overrideValues(
        {biometricAuthenticationForFillingToggleVisible: false});
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await prefsProxy.whenCalled('getPref');
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

  // Add Shortcut banner is shown and clickable if the shortcut is not yet
  // installed.
  test('showAddShortcutBanner', async function() {
    loadTimeData.overrideValues({isPasswordManagerShortcutInstalled: false});
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();

    const addShortcutBanner =
        settings.shadowRoot!.querySelector<HTMLElement>('#addShortcutBanner');
    assertTrue(!!addShortcutBanner);

    addShortcutBanner.click();
    await passwordManager.whenCalled('showAddShortcutDialog');
  });

  // Add Shortcut banner is not shown if the shortcut is already installed.
  test('hideAddShortcutBanner', async function() {
    loadTimeData.overrideValues({isPasswordManagerShortcutInstalled: true});
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();

    assertFalse(!!settings.shadowRoot!.querySelector('#addShortcutBanner'));
  });

  test('Password exporter element', async function() {
    // Exporter should not be present if there are no saved passwords.
    passwordManager.data.passwords = [];
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await passwordManager.whenCalled('getSavedPasswordList');
    assertFalse(!!settings!.shadowRoot!.querySelector('passwords-exporter'));

    // Exporter should appear when saved passwords are observed.
    passwordManager.data.passwords.push(
        createPasswordEntry({username: 'user1', id: 1}));
    passwordManager.listeners.savedPasswordListChangedListener!
        (passwordManager.data.passwords);
    flush();
    assertTrue(!!settings.shadowRoot!.querySelector('passwords-exporter'));
  });
});
