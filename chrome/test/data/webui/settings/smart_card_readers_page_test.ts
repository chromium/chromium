// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSmartCardReadersPageElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, routes, resetRouterForTesting} from 'chrome://settings/settings.js';
import {assertEquals, assertDeepEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on
suite('SmartCardReadersPageSettings', function() {
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

  test('IsPopulated', async function() {
    browserProxy.setSmartCardReaderGrants([
      {
        readerName: 'reader',
        origins: [
          {
            origin: 'foo.com',
            displayName: 'Foo.com',
          },
          {
            origin: 'bar.com',
            displayName: 'Bar.com',
          },
        ],
      },
      {
        readerName: 'reader 2',
        origins: [
          {
            origin: 'bar.com',
            displayName: 'Bar.com',
          },
        ],
      },
    ]);

    testElement.currentRouteChanged(routes.SITE_SETTINGS_SMART_CARD_READERS);
    await browserProxy.whenCalled('getSmartCardReaderGrants');
    flush();

    assertDeepEquals(
        [
          [
            'reader',
            {
              origin: 'foo.com',
              displayName: 'Foo.com',
            },
          ],
          [
            'reader',
            {
              origin: 'bar.com',
              displayName: 'Bar.com',
            },
          ],
          [
            'reader 2',
            {
              origin: 'bar.com',
              displayName: 'Bar.com',
            },
          ],
        ],
        Array.from(
            testElement.shadowRoot!.querySelectorAll(
                'smart-card-reader-origin-entry'),
            x => [x.smartCardReaderName, x.origin]));

    const emptyHeaders =
        testElement.shadowRoot!.querySelectorAll('#readersNotFound');
    assertEquals(1, emptyHeaders.length);
    assertFalse(emptyHeaders[0]!.checkVisibility());

    const resetButtons =
        testElement.shadowRoot!.querySelectorAll('#resetButton');
    assertEquals(1, resetButtons.length);
    assertTrue(resetButtons[0]!.checkVisibility());

    const readerEntries = testElement.shadowRoot!.querySelectorAll(
        '.smart-card-reader-entry:not(#readersNotFound)');
    assertEquals(2, readerEntries.length);
  });

  test('IsPopulatedEmpty', async function() {
    browserProxy.setSmartCardReaderGrants([]);

    testElement.currentRouteChanged(routes.SITE_SETTINGS_SMART_CARD_READERS);
    await browserProxy.whenCalled('getSmartCardReaderGrants');
    flush();
    const originEntries = testElement.shadowRoot!.querySelectorAll(
        'smart-card-reader-origin-entry');
    assertEquals(0, originEntries.length);
    assertEquals(1, browserProxy.getCallCount('getSmartCardReaderGrants'));

    const emptyHeaders =
        testElement.shadowRoot!.querySelectorAll('#readersNotFound');
    assertEquals(1, emptyHeaders.length);
    assertTrue(emptyHeaders[0]!.checkVisibility());

    const resetButtons =
        testElement.shadowRoot!.querySelectorAll('#resetButton');
    assertEquals(1, resetButtons.length);
    assertFalse(resetButtons[0]!.checkVisibility());

    const readerEntries = testElement.shadowRoot!.querySelectorAll(
        '.smart-card-reader-entry:not(#readersNotFound)');
    assertEquals(0, readerEntries.length);
  });
});
