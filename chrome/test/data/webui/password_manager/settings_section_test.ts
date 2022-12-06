// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl, PrefsBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {createBlockedSiteEntry, makePasswordManagerPrefs} from './test_util.js';

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

  // Add Shortcut banner is shown if the shortcut is not yet installed.
  test('showAddShortcutBanner', async function() {
    loadTimeData.overrideValues({isPasswordManagerShortcutInstalled: false});
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();

    assertTrue(!!settings!.shadowRoot!.querySelector('#addShortcutBanner'));
  });

  // Add Shortcut banner is not shown if the shortcut is already installed.
  test('hideAddShortcutBanner', async function() {
    loadTimeData.overrideValues({isPasswordManagerShortcutInstalled: true});
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();

    assertFalse(!!settings!.shadowRoot!.querySelector('#addShortcutBanner'));
  });

  test('Export dialog appears after clicking on banner', async function() {
    const settings = document.createElement('settings-section');
    document.body.appendChild(settings);
    await flushTasks();

    settings.$.exportPasswordsBanner.click();
    await flushTasks();
    const exportPasswordsDialog =
        settings!.shadowRoot!.querySelector('passwords-export-dialog');
    assertTrue(!!exportPasswordsDialog);
  });
});
