// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list-entry. */

// clang-format off
import 'chrome://test/cr_elements/cr_policy_strings.js';

import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSettingsTypes,SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

suite('SiteListEntry', function() {
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy;

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    PolymerTest.clearBody();
    testElement = document.createElement('site-list-entry');
    document.body.appendChild(testElement);
  });

  test('fires show-tooltip when mouse over policy indicator', function() {
    testElement.model = {
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      origin: 'http://example.com',
    };
    flush();
    const prefIndicator = testElement.$$('cr-policy-pref-indicator');
    assertTrue(!!prefIndicator);
    const icon = prefIndicator.$$('cr-tooltip-icon');
    const paperTooltip = icon.$$('paper-tooltip');
    // Never shown since site-list will show a common tooltip.
    assertEquals('none', paperTooltip.computedStyleMap().get('display').value);
    assertFalse(paperTooltip._showing);
    const wait = eventToPromise('show-tooltip', document);
    icon.$.indicator.dispatchEvent(
        new MouseEvent('mouseenter', {bubbles: true, composed: true}));
    return wait.then(() => {
      assertTrue(paperTooltip._showing);
      assertEquals(
          'none', paperTooltip.computedStyleMap().get('display').value);
    });
  });

  if (isChromeOS) {
    test('shows androidSms note', function() {
      testElement.model = {
        origin: 'http://example.com',
        showAndroidSmsNote: true,
        category: ContentSettingsTypes.NOTIFICATIONS
      };
      flush();
      const siteDescription = testElement.$$('#siteDescription');
      assertEquals(
          loadTimeData.getString('androidSmsNote'),
          siteDescription.textContent);
    });
  }

  test('shows settingDetail', function() {
    // Verify that `settingDetail` is respected.
    testElement.model = {
      origin: 'http://example.com',
      settingDetail: '.txt',
      category: ContentSettingsTypes.FILE_HANDLING,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription');
    assertEquals('.txt', siteDescription.textContent);

    // Verify that with no settingDetail, a computed label is used.
    testElement.model = {
      origin: 'http://example.com',
      category: ContentSettingsTypes.GEOLOCATION,
    };
    flush();
    assertEquals(
        loadTimeData.getString('embeddedOnAnyHost'),
        siteDescription.textContent);

    // Verify that settingDetail overrides other (computed) labels.
    testElement.model = {
      origin: 'http://example.com',
      category: ContentSettingsTypes.GEOLOCATION,
      settingDetail: '.txt',
    };
    flush();
    assertEquals('.txt', siteDescription.textContent);
  });

  // Verify that with GEOLOCATION, the "embedded on any host" text is shown.
  // Regression test for crbug.com/1205103
  test('location embedded on any host', function() {
    testElement.model = {
      origin: 'http://example.com',
      category: ContentSettingsTypes.GEOLOCATION,
    };
    flush();
    const siteDescription = testElement.$$('#siteDescription');
    assertEquals(
        loadTimeData.getString('embeddedOnAnyHost'),
        siteDescription.textContent);
  });

  test('not valid origin does not go to site details page', function() {
    browserProxy.setIsOriginValid(false);
    testElement.model = {
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      origin: 'example.com',
    };
    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
    return browserProxy.whenCalled('isOriginValid').then((args) => {
      assertEquals('example.com', args);
      flush();
      const settingsRow = testElement.root.querySelector('.settings-row');
      assertFalse(settingsRow.hasAttribute('actionable'));
      const subpageArrow = settingsRow.querySelector('.subpage-arrow');
      assertTrue(!subpageArrow);
      const separator = settingsRow.querySelector('.separator');
      assertTrue(!separator);
      settingsRow.click();
      assertEquals(
          routes.SITE_SETTINGS.path,
          Router.getInstance().getCurrentRoute().path);
    });
  });

  test('valid origin goes to site details page', function() {
    browserProxy.setIsOriginValid(true);
    testElement.model = {
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      origin: 'http://example.com',
    };
    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
    return browserProxy.whenCalled('isOriginValid').then((args) => {
      assertEquals('http://example.com', args);
      flush();
      const settingsRow = testElement.root.querySelector('.settings-row');
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
});
