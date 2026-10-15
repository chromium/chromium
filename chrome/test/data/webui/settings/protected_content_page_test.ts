// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsCollapseRadioButtonElement, ProtectedContentPageElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';
import {isMac, isWindows} from 'chrome://resources/js/platform.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
// clang-format on

const ENCRYPTED_MEDIA_PREF = 'webkit.webprefs.encrypted_media_enabled';

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: ENCRYPTED_MEDIA_PREF,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
  ];
}

suite('ProtectedContentPage', function() {
  let page: ProtectedContentPageElement;
  let prefService: PrefService;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
  });

  function createPage() {
    const page = document.createElement('settings-protected-content-page');
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

  test('EncryptedMediaRadioGroup', async function() {
    page = createPage();
    await flushTasks();

    const allowedButton =
        page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
            '#allowedButton');
    assertTrue(!!allowedButton);
    const blockedButton =
        page.shadowRoot!.querySelector<SettingsCollapseRadioButtonElement>(
            '#blockedButton');
    assertTrue(!!blockedButton);

    assertTrue(prefService.getPref<boolean>(ENCRYPTED_MEDIA_PREF).value);

    blockedButton.click();
    await flushTasks();
    assertFalse(prefService.getPref<boolean>(ENCRYPTED_MEDIA_PREF).value);

    allowedButton.click();
    await flushTasks();
    assertTrue(prefService.getPref<boolean>(ENCRYPTED_MEDIA_PREF).value);
  });
});
