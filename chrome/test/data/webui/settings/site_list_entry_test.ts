// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list-entry. */

// clang-format off
import 'chrome://webui-test/cr_elements/cr_policy_strings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SiteListEntryElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, CookiesExceptionType, SITE_EXCEPTION_WILDCARD, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {assertTooltipIsHidden} from './test_util.js';

// clang-format on

suite('SiteListEntry', function() {
  let testElement: SiteListEntryElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('site-list-entry');
    document.body.appendChild(testElement);
  });

  test('fires show-tooltip when mouse over policy indicator', function() {
    testElement.model = {
      category: ContentSettingsTypes.NOTIFICATIONS,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      displayName: '',
      embeddingOrigin: '',
      description: '',
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
    const crTooltip = icon.shadowRoot!.querySelector('cr-tooltip')!;
    // Never shown since site-list will show a common tooltip.
    assertTooltipIsHidden(crTooltip);
    const wait = eventToPromise('show-tooltip', document);
    icon.$.indicator.dispatchEvent(
        new MouseEvent('mouseenter', {bubbles: true, composed: true}));
    return wait.then(() => {
      assertTooltipIsHidden(crTooltip);
    });
  });

  test('not valid origin does not go to site details page', async function() {
    browserProxy.setIsOriginValid(false);
    testElement.model = {
      category: ContentSettingsTypes.GEOLOCATION,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      displayName: '',
      embeddingOrigin: '',
      description: '',
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
      description: '',
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

  // Verify that third-party exceptions in a combined list have an additional
  // description.
  test('third-party exception in a combined exceptions list', function() {
    testElement.cookiesExceptionType = CookiesExceptionType.COMBINED;
    testElement.model = {
      category: ContentSettingsTypes.COOKIES,
      controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
      displayName: '',
      embeddingOrigin: 'http://example.com',
      description: '',
      enforcement: null,
      incognito: false,
      isEmbargoed: false,
      origin: SITE_EXCEPTION_WILDCARD,
      setting: ContentSetting.DEFAULT,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription')!;
    assertEquals(
        loadTimeData.getString('siteSettingsCookiesThirdPartyExceptionLabel'),
        siteDescription.textContent);
  });

  // Verify that third-party exceptions in a third-party exceptions list don't
  // have an additional description.
  test('third-party exception in a third-party exceptions list', function() {
    testElement.cookiesExceptionType = CookiesExceptionType.THIRD_PARTY;
    testElement.model = {
      category: ContentSettingsTypes.COOKIES,
      controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
      displayName: '',
      embeddingOrigin: 'http://example.com',
      description: '',
      enforcement: null,
      incognito: false,
      isEmbargoed: false,
      origin: SITE_EXCEPTION_WILDCARD,
      setting: ContentSetting.DEFAULT,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription')!;
    assertEquals('', siteDescription.textContent);
  });

  // Verify that tracking protection exceptions don't have an embedding-origin
  // description.
  test('tracking protection exception', function() {
    testElement.model = {
      category: ContentSettingsTypes.TRACKING_PROTECTION,
      controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
      displayName: '',
      embeddingOrigin: 'http://example.com',
      description: '',
      enforcement: null,
      incognito: false,
      isEmbargoed: false,
      origin: SITE_EXCEPTION_WILDCARD,
      setting: ContentSetting.DEFAULT,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription')!;
    assertEquals('', siteDescription.textContent);
  });

  // Verify that exceptions with both patterns have proper description for both
  // lists.
  test('cookies exception with both patterns set', function() {
    testElement.cookiesExceptionType = CookiesExceptionType.COMBINED;
    testElement.model = {
      category: ContentSettingsTypes.COOKIES,
      controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
      displayName: '',
      embeddingOrigin: 'http://example1.com',
      description: '',
      enforcement: null,
      incognito: false,
      isEmbargoed: false,
      origin: 'http://example2.com',
      setting: ContentSetting.DEFAULT,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription')!;
    assertEquals(
        loadTimeData.getStringF('embeddedOnHost', 'http://example1.com'),
        siteDescription.textContent);

    // `cookiesExceptionType` is static, the element is only observing changes
    // to the model.
    testElement.cookiesExceptionType = CookiesExceptionType.THIRD_PARTY;
    testElement.model = {...testElement.model};
    flush();
    assertEquals(
        loadTimeData.getStringF('embeddedOnHost', 'http://example1.com'),
        siteDescription.textContent);
  });
  test('chooser exception with website origin', function() {
    testElement.model = {
      category: ContentSettingsTypes.USB_DEVICES,
      controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
      displayName: '',
      embeddingOrigin: '',
      description: '',
      enforcement: null,
      incognito: false,
      isEmbargoed: false,
      origin: 'https://example.com',
      setting: ContentSetting.DEFAULT,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription')!;
    assertEquals('', siteDescription.textContent);
  });
  test('extension shows id in description', function() {
    testElement.model = {
      category: ContentSettingsTypes.USB_DEVICES,
      controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
      displayName: '',
      embeddingOrigin: '',
      description: '',
      enforcement: null,
      incognito: false,
      isEmbargoed: false,
      origin: 'chrome-extension://mhabknllooicelmdboebjilbohdbihln',
      setting: ContentSetting.DEFAULT,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription')!;
    assertEquals(
        'ID: mhabknllooicelmdboebjilbohdbihln', siteDescription.textContent);
  });

  test('description field applies and overrides others', function() {
    testElement.model = {
      category: ContentSettingsTypes.GEOLOCATION,  // Usually has description.
      controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
      displayName: '',
      embeddingOrigin: 'http://bar',
      description: 'foo',
      enforcement: null,
      incognito: false,
      isEmbargoed: true,
      origin: 'https://example.com',
      setting: ContentSetting.DEFAULT,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription')!;
    assertEquals('foo', siteDescription.textContent);
  });
});
