// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl, PrefsBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
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
});
