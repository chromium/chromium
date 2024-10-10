// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SmartCardReaderOriginEntryElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {CrIconButtonElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import type {SiteFaviconElement} from 'chrome://settings/settings.js';
import {assertEquals, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on
suite('SmartCardReadersPageSettings_OriginEntry', function() {
  let testElement: SmartCardReaderOriginEntryElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    testElement = document.createElement('smart-card-reader-origin-entry');
    assertTrue(!!testElement);
    testElement.smartCardReaderName = 'reader';
    testElement.origin = 'foo.com';
    document.body.appendChild(testElement);
  });

  test('HasAllElements', async function() {
    const icons = testElement.shadowRoot!.querySelectorAll<SiteFaviconElement>(
        'site-favicon');
    assertEquals(1, icons.length);
    assertTrue(!!icons[0]);
    assertEquals('foo.com', icons[0].url);

    const origins = testElement.shadowRoot!.querySelectorAll('.origin');
    assertEquals(1, origins.length);
    assertTrue(!!origins[0] && !!origins[0].textContent);
    assertStringContains('foo.com', origins[0].textContent);

    const removeButtons =
        testElement.shadowRoot!.querySelectorAll<CrIconButtonElement>(
            '#removeOrigin');
    assertEquals(1, removeButtons.length);
    assertTrue(!!removeButtons[0]!.ariaLabel);
    assertStringContains('foo.com', removeButtons[0]!.ariaLabel);
  });

  test('RevokeButtonWorks', async function() {
    assertTrue(!!testElement.shadowRoot);
    const buttons =
        testElement.shadowRoot!.querySelectorAll<CrIconButtonElement>(
            '#removeOrigin');
    assertEquals(1, buttons.length);
    assertTrue(!!buttons[0]);
    buttons[0].click();

    await browserProxy.whenCalled('revokeSmartCardReaderGrant');
    flush();

    assertEquals(1, browserProxy.getCallCount('revokeSmartCardReaderGrant'));
    const lastArgs = browserProxy.getArgs('revokeSmartCardReaderGrant')[0];
    assertEquals('reader', lastArgs[0]);
    assertEquals('foo.com', lastArgs[1]);
  });
});
