// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list-entry. */

import 'chrome://webui-test/cr_elements/cr_policy_strings.js';

import type {SiteListEntryElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, CookiesExceptionType, SITE_EXCEPTION_WILDCARD, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import {assertTooltipIsHidden} from './test_util.js';

suite('SiteListEntry', function() {
  let testElement: SiteListEntryElement;
  let browserProxy: TestSiteSettingsBrowserProxy;

  setup(function() {
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('site-list-entry');
    document.body.appendChild(testElement);
  });

  test('fires show-tooltip when mouse over policy indicator', async function() {
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
    await microtasksFinished();
    const prefIndicator =
        testElement.shadowRoot.querySelector('cr-policy-pref-indicator');
    assertTrue(!!prefIndicator);
    const icon = prefIndicator.shadowRoot.querySelector('cr-tooltip-icon');
    assertTrue(!!icon);
    const crTooltip = icon.shadowRoot.querySelector('cr-tooltip');
    assertTrue(!!crTooltip);
    // Never shown since site-list will show a common tooltip.
    assertTooltipIsHidden(crTooltip);
    const wait = eventToPromise('show-tooltip', document);
    icon.$.indicator.dispatchEvent(
        new MouseEvent('mouseenter', {bubbles: true, composed: true}));
    await wait;
    assertTooltipIsHidden(crTooltip);
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
    await microtasksFinished();
    assertEquals('example.com', args);
    const originArea =
        testElement.shadowRoot.querySelector<HTMLElement>('#originArea');
    assertTrue(!!originArea);
    assertFalse(originArea.hasAttribute('actionable'));
    const subpageArrow = originArea.querySelector('.subpage-arrow');
    assertNull(subpageArrow);
    const separator = originArea.querySelector('.separator');
    assertNull(separator);
    originArea.click();
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
    await microtasksFinished();
    assertEquals('http://example.com', args);
    const originArea =
        testElement.shadowRoot.querySelector<HTMLElement>('#originArea');
    assertTrue(!!originArea);
    assertTrue(originArea.hasAttribute('actionable'));
    assertTrue(isChildVisible(originArea, '.subpage-arrow', true));
    assertTrue(isChildVisible(originArea, '.separator', true));
    originArea.click();
    assertEquals(
        routes.SITE_SETTINGS_SITE_DETAILS.path,
        Router.getInstance().getCurrentRoute().path);
  });

  // Verify that third-party exceptions in a combined list have an additional
  // description.
  test('third-party exception in a combined exceptions list', async function() {
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
    await microtasksFinished();
    const siteDescription = testElement.$.siteDescription;
    assertEquals(
        loadTimeData.getString('siteSettingsCookiesThirdPartyExceptionLabel'),
        siteDescription.textContent);
  });

  // Verify that third-party exceptions in a third-party exceptions list don't
  // have an additional description.
  test(
      'third-party exception in a third-party exceptions list',
      async function() {
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
        await microtasksFinished();
        const siteDescription = testElement.$.siteDescription;
        assertEquals('', siteDescription.textContent);
      });

  // Verify that exceptions with both patterns have proper description for both
  // lists.
  test('cookies exception with both patterns set', async function() {
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
    await microtasksFinished();
    const siteDescription = testElement.$.siteDescription;
    assertEquals(
        loadTimeData.getStringF('embeddedOnHost', 'http://example1.com'),
        siteDescription.textContent);

    testElement.cookiesExceptionType = CookiesExceptionType.THIRD_PARTY;
    await microtasksFinished();
    assertEquals(
        loadTimeData.getStringF('embeddedOnHost', 'http://example1.com'),
        siteDescription.textContent);
  });
  test('chooser exception with website origin', async function() {
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
    await microtasksFinished();
    const siteDescription = testElement.$.siteDescription;
    assertEquals('', siteDescription.textContent);
  });
  test('extension shows id in description', async function() {
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
    await microtasksFinished();
    const siteDescription = testElement.$.siteDescription;
    assertEquals(
        'ID: mhabknllooicelmdboebjilbohdbihln', siteDescription.textContent);
  });

  test('description field applies and overrides others', async function() {
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
    await microtasksFinished();
    const siteDescription = testElement.$.siteDescription;
    assertEquals('foo', siteDescription.textContent);
  });

  test(
      'the reset button aria-label includes the section header',
      async function() {
        testElement.model = {
          category: ContentSettingsTypes.GEOLOCATION,
          controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
          displayName: 'example.com',
          embeddingOrigin: 'http://bar',
          description: 'foo',
          enforcement: null,
          incognito: false,
          isEmbargoed: true,
          origin: 'https://example.com',
          setting: ContentSetting.ALLOW,
        };
        testElement.sectionHeader =
            loadTimeData.getString('siteSettingsLocationAllowedExceptions');
        await microtasksFinished();

        const resetSite = testElement.shadowRoot.querySelector('#resetSite');
        assertTrue(!!resetSite);

        assertEquals(
            loadTimeData.getStringF(
                'siteSettingsActionResetFromListA11y', 'example.com',
                loadTimeData.getString(
                    'siteSettingsLocationAllowedExceptions')),
            resetSite.getAttribute('aria-label'));
      });

  test(
      'the view details button aria-label includes the section header',
      async function() {
        browserProxy.setIsOriginValid(true);
        testElement.model = {
          category: ContentSettingsTypes.GEOLOCATION,
          controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
          displayName: 'example.com',
          embeddingOrigin: 'http://bar',
          description: 'foo',
          enforcement: null,
          incognito: false,
          isEmbargoed: true,
          origin: 'https://example.com',
          setting: ContentSetting.ALLOW,
        };
        testElement.sectionHeader =
            loadTimeData.getString('siteSettingsLocationAllowedExceptions');

        Router.getInstance().navigateTo(routes.SITE_SETTINGS);
        // Wait for the `isOriginValid` call to set `allowNavigateToSiteDetail_`
        // to true, otherwise the subpage-arrow button will not be shown.
        const args = await browserProxy.whenCalled('isOriginValid');
        await microtasksFinished();
        assertEquals('https://example.com', args);

        const subpageArrow =
            testElement.shadowRoot.querySelector('.subpage-arrow');
        assertTrue(!!subpageArrow);

        assertEquals(
            loadTimeData.getStringF(
                'siteSettingsActionViewFromListA11y', 'example.com',
                loadTimeData.getString(
                    'siteSettingsLocationAllowedExceptions')),
            subpageArrow.getAttribute('aria-label'));
      });
});
