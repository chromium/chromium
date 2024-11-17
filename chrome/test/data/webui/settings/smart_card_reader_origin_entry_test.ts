// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SmartCardReaderOriginEntryElement} from 'chrome://settings/lazy_load.js';
import {SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {CrIconButtonElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertStringContains, assertDeepEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
    testElement.origin = {origin: 'foo.com', displayName: 'Foo.com'};
    document.body.appendChild(testElement);
  });

  test('HasAllElements', function() {
    const clickableDivs =
        testElement.shadowRoot!.querySelectorAll('.settings-row');
    assertEquals(1, clickableDivs.length);
    const clickableDiv = clickableDivs[0]!;
    const icons = clickableDiv.querySelectorAll('site-favicon');
    assertEquals(1, icons.length);
    assertEquals('foo.com', icons[0]!.url);

    const origins = clickableDiv.querySelectorAll('.origin');
    assertEquals(1, origins.length);
    assertTrue(!!origins[0]!.textContent);
    assertStringContains('Foo.com', origins[0]!.textContent);

    const arrows = clickableDiv.querySelectorAll('#fileSystemSiteDetails');
    assertEquals(1, arrows.length);
    assertTrue(!!arrows[0]!.ariaLabel);
    assertEquals('Foo.com', arrows[0]!.ariaLabel);

    const removeButtons =
        testElement.shadowRoot!.querySelectorAll<CrIconButtonElement>(
            '#removeOrigin');
    assertEquals(1, removeButtons.length);
    assertTrue(!!removeButtons[0]!.ariaLabel);
    assertEquals('foo.com', removeButtons[0]!.ariaLabel);
  });

  test('RevokeButtonWorks', async function() {
    const buttons =
        testElement.shadowRoot!.querySelectorAll<CrIconButtonElement>(
            '#removeOrigin');
    assertEquals(1, buttons.length);
    buttons[0]!.click();

    await browserProxy.whenCalled('revokeSmartCardReaderGrant');
    flush();

    assertEquals(1, browserProxy.getCallCount('revokeSmartCardReaderGrant'));
    assertDeepEquals(
        ['reader', 'foo.com'],
        browserProxy.getArgs('revokeSmartCardReaderGrant')[0]);
  });

  test('NavigationToSiteDetails', function() {
    const clickableRows =
        testElement.shadowRoot!.querySelectorAll<HTMLElement>('.settings-row');
    assertEquals(1, clickableRows.length);
    assertTrue(clickableRows[0]!.hasAttribute('actionable'));
    clickableRows[0]!.click();
    assertDeepEquals(
        routes.SITE_SETTINGS_SITE_DETAILS,
        Router.getInstance().getCurrentRoute());
    assertEquals('foo.com', new URL(document.URL).searchParams.get('site'));
  });
});
