// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSmartCardReadersPageElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, routes, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on
suite('FileSystemSettings_EnablePersistentPermissions', function() {
  let testElement: SettingsSmartCardReadersPageElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      enableSmartCardReadersContentSetting: true,
    });
    resetRouterForTesting();
  });

  // Initialize the settings-smart-card-readers-page element.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    testElement = document.createElement('settings-smart-card-readers-page');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);
  });

  test('SmartCardReadersPage_IsPopulated', async function() {
    browserProxy.setSmartCardReaderGrants([
      {
        readerName: 'reader',
        origins: [
          'foo.com',
        ],
      },
      {
        readerName: 'reader',
        origins: [
          'bar.com',
        ],
      },
      {
        readerName: 'reader 2',
        origins: [
          'bar.com',
        ],
      },
    ]);

    testElement.currentRouteChanged(routes.SITE_SETTINGS_SMART_CARD_READERS);
    await browserProxy.whenCalled('getSmartCardReaderGrants');
    flush();
    const originEntries = testElement.shadowRoot!.querySelectorAll(
        'smart-card-reader-origin-entry');
    assertEquals(originEntries.length, 3);
    assertEquals(browserProxy.getCallCount('getSmartCardReaderGrants'), 1);

    const emptyHeaders =
        testElement.shadowRoot!.querySelectorAll('#readersNotFound');
    assertEquals(emptyHeaders.length, 1);
    assertFalse(emptyHeaders[0]!.checkVisibility());

    const resetButtons =
        testElement.shadowRoot!.querySelectorAll('#resetButton');
    assertEquals(resetButtons.length, 1);
    assertTrue(resetButtons[0]!.checkVisibility());
  });

  test('SmartCardReadersPage_IsPopulatedEmpty', async function() {
    browserProxy.setSmartCardReaderGrants([]);

    testElement.currentRouteChanged(routes.SITE_SETTINGS_SMART_CARD_READERS);
    await browserProxy.whenCalled('getSmartCardReaderGrants');
    flush();
    const originEntries = testElement.shadowRoot!.querySelectorAll(
        'smart-card-reader-origin-entry');
    assertEquals(originEntries.length, 0);
    assertEquals(browserProxy.getCallCount('getSmartCardReaderGrants'), 1);

    const emptyHeaders =
        testElement.shadowRoot!.querySelectorAll('#readersNotFound');
    assertEquals(emptyHeaders.length, 1);
    assertTrue(emptyHeaders[0]!.checkVisibility());

    const resetButtons =
        testElement.shadowRoot!.querySelectorAll('#resetButton');
    assertEquals(resetButtons.length, 1);
    assertFalse(resetButtons[0]!.checkVisibility());
  });
});
