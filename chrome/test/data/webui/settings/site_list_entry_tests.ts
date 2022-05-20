// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list-entry. */

// clang-format off
import 'chrome://webui-test/cr_elements/cr_policy_strings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, ContentSettingsTypes, SiteListEntryElement, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

suite('SiteListEntry', function() {
  let testElement: SiteListEntryElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = '';
    testElement = document.createElement('site-list-entry');
    document.body.appendChild(testElement);
  });

  test('fires show-tooltip when mouse over policy indicator', function() {
    testElement.model = {
      category: ContentSettingsTypes.NOTIFICATIONS,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      displayName: '',
      embeddingOrigin: '',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      incognito: false,
      isEmbargoed: false,
      origin: 'http://example.com',
      setting: ContentSetting.DEFAULT,
    };
    flush();
    const prefIndicator = testElement.$$('cr-policy-pref-indicator');
    assertTrue(!!prefIndicator);
    const icon = prefIndicator!.shadowRoot!.querySelector('cr-tooltip-icon')!;
    const paperTooltip = icon.shadowRoot!.querySelector('paper-tooltip')!;
    // Never shown since site-list will show a common tooltip.
    assertEquals('none', (paperTooltip.computedStyleMap().get('display') as {
                           value: number,
                         }).value);
    assertFalse(paperTooltip._showing);
    const wait = eventToPromise('show-tooltip', document);
    icon.$.indicator.dispatchEvent(
        new MouseEvent('mouseenter', {bubbles: true, composed: true}));
    return wait.then(() => {
      assertTrue(paperTooltip._showing);
      assertEquals('none', (paperTooltip.computedStyleMap().get('display') as {
                             value: number,
                           }).value);
    });
  });

  // <if expr="chromeos_ash">
  test('shows androidSms note', function() {
    testElement.model = {
      category: ContentSettingsTypes.NOTIFICATIONS,
      controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
      displayName: '',
      embeddingOrigin: '',
      enforcement: null,
      incognito: false,
      isEmbargoed: false,
      origin: 'http://example.com',
      setting: ContentSetting.DEFAULT,
      showAndroidSmsNote: true,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription')!;
    assertEquals(
        loadTimeData.getString('androidSmsNote'), siteDescription.textContent);
  });
  // </if>

  // Verify that with GEOLOCATION, the "embedded on any host" text is shown.
  // Regression test for crbug.com/1205103
  test('location embedded on any host', function() {
    testElement.model = {
      category: ContentSettingsTypes.GEOLOCATION,
      controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
      displayName: '',
      embeddingOrigin: '',
      enforcement: null,
      incognito: false,
      isEmbargoed: false,
      origin: 'http://example.com',
      setting: ContentSetting.DEFAULT,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription')!;
    assertEquals(
        loadTimeData.getString('embeddedOnAnyHost'),
        siteDescription.textContent);
  });

  test('not valid origin does not go to site details page', async function() {
    browserProxy.setIsOriginValid(false);
    testElement.model = {
      category: ContentSettingsTypes.GEOLOCATION,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      displayName: '',
      embeddingOrigin: '',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      incognito: false,
      isEmbargoed: false,
      origin: 'example.com',
      setting: ContentSetting.DEFAULT,
    };
    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
    const args = await browserProxy.whenCalled('isOriginValid');
    assertEquals('example.com', args);
    flush();
    const settingsRow =
        testElement.shadowRoot!.querySelector<HTMLElement>('.settings-row')!;
    assertFalse(settingsRow.hasAttribute('actionable'));
    const subpageArrow = settingsRow.querySelector('.subpage-arrow');
    assertTrue(!subpageArrow);
    const separator = settingsRow.querySelector('.separator');
    assertTrue(!separator);
    settingsRow!.click();
    assertEquals(
        routes.SITE_SETTINGS.path, Router.getInstance().getCurrentRoute().path);
  });

  test('valid origin goes to site details page', async function() {
    browserProxy.setIsOriginValid(true);
    testElement.model = {
      category: ContentSettingsTypes.GEOLOCATION,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      displayName: '',
      embeddingOrigin: '',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      incognito: false,
      isEmbargoed: false,
      origin: 'http://example.com',
      setting: ContentSetting.DEFAULT,
    };
    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
    const args = await browserProxy.whenCalled('isOriginValid');
    assertEquals('http://example.com', args);
    flush();
    const settingsRow =
        testElement.shadowRoot!.querySelector<HTMLElement>('.settings-row')!;
    assertTrue(settingsRow.hasAttribute('actionable'));
    const subpageArrow = settingsRow.querySelector('.subpage-arrow');
    assertFalse(!subpageArrow);
    const separator = settingsRow.querySelector('.separator');
    assertFalse(!separator);
    settingsRow.click();
    assertEquals(
        routes.SITE_SETTINGS_SITE_DETAILS.path,
        Router.getInstance().getCurrentRoute().path);
  });
});
