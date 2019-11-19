// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list-entry. */

suite('SiteListEntry', function() {
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy;

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
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
    Polymer.dom.flush();
    const prefIndicator = testElement.$$('cr-policy-pref-indicator');
    assertTrue(!!prefIndicator);
    const icon = prefIndicator.$$('cr-tooltip-icon');
    const paperTooltip = icon.$$('paper-tooltip');
    // Never shown since site-list will show a common tooltip.
    assertEquals('none', paperTooltip.computedStyleMap().get('display').value);
    assertFalse(paperTooltip._showing);
    const wait = test_util.eventToPromise('show-tooltip', document);
    icon.$.indicator.dispatchEvent(
        new MouseEvent('mouseenter', {bubbles: true, composed: true}));
    return wait.then(() => {
      assertTrue(paperTooltip._showing);
      assertEquals(
          'none', paperTooltip.computedStyleMap().get('display').value);
    });
  });

  if (cr.isChromeOS) {
    test('shows androidSms note', function() {
      testElement.model = {
        origin: 'http://example.com',
        showAndroidSmsNote: true,
        category: settings.ContentSettingsTypes.NOTIFICATIONS
      };
      Polymer.dom.flush();
      const siteDescription = testElement.$$('#siteDescription');
      assertEquals(
          loadTimeData.getString('androidSmsNote'),
          siteDescription.textContent);
    });
  }

  test('not valid origin does not go to site details page', function() {
    browserProxy.setIsOriginValid(false);
    testElement.model = {
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      origin: 'example.com',
    };
    settings.navigateTo(settings.routes.SITE_SETTINGS);
    return browserProxy.whenCalled('isOriginValid').then((args) => {
      assertEquals('example.com', args);
      Polymer.dom.flush();
      const settingsRow = testElement.root.querySelector('.settings-row');
      assertFalse(settingsRow.hasAttribute('actionable'));
      const subpageArrow = settingsRow.querySelector('.subpage-arrow');
      assertTrue(!subpageArrow);
      const separator = settingsRow.querySelector('.separator');
      assertTrue(!separator);
      settingsRow.click();
      assertEquals(
          settings.routes.SITE_SETTINGS.path, settings.getCurrentRoute().path);
    });
  });

  test('valid origin goes to site details page', function() {
    browserProxy.setIsOriginValid(true);
    testElement.model = {
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      origin: 'http://example.com',
    };
    settings.navigateTo(settings.routes.SITE_SETTINGS);
    return browserProxy.whenCalled('isOriginValid').then((args) => {
      assertEquals('http://example.com', args);
      Polymer.dom.flush();
      settingsRow = testElement.root.querySelector('.settings-row');
      assertTrue(settingsRow.hasAttribute('actionable'));
      const subpageArrow = settingsRow.querySelector('.subpage-arrow');
      assertFalse(!subpageArrow);
      const separator = settingsRow.querySelector('.separator');
      assertFalse(!separator);
      settingsRow.click();
      assertEquals(
          settings.routes.SITE_SETTINGS_SITE_DETAILS.path,
          settings.getCurrentRoute().path);
    });
  });
});
