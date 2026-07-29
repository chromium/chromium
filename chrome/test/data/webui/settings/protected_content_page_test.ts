// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsCollapseRadioButtonElement, ProtectedContentPageElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import {isMac, isWindows} from 'chrome://resources/js/platform.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
// clang-format on

suite('ProtectedContentPage', function() {
  let page: ProtectedContentPageElement;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
  });

  function createPage() {
    const page = document.createElement('settings-protected-content-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return page;
  }

  test('ProtectedContentStrings', async function() {
    page = createPage();
    await flushTasks();

    const isWinOrMac = isWindows || isMac;

    const description = page.shadowRoot!.querySelector('#description');
    assertTrue(!!description);
    assertEquals(
        loadTimeData.getString('siteSettingsProtectedContentDescription'),
        description.textContent.trim());

    const allowedButton =
        page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
            '#allowedButton');
    assertTrue(!!allowedButton);

    const blockedButton =
        page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
            '#blockedButton');
    assertTrue(!!blockedButton);

    assertEquals(
        loadTimeData.getString('siteSettingsProtectedContentAllowed'),
        allowedButton.label);

    assertEquals(
        loadTimeData.getString('siteSettingsProtectedContentBlocked'),
        blockedButton.label);

    assertEquals(
        loadTimeData.getString('siteSettingsProtectedContentBlockedSubLabel'),
        blockedButton.subLabel);

    if (isWinOrMac) {
      assertEquals(
          loadTimeData.getString('siteSettingsProtectedContentAllowedSubLabel'),
          allowedButton.subLabel);
    } else {
      assertFalse(!!allowedButton.subLabel);
    }
  });
});
