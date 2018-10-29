// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list-entry. */

suite('SiteListEntry', function() {
  let testElement;
  setup(function() {
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
    const tooltip = paperTooltip.$.tooltip;
    // Never shown since site-list will show a common tooltip.
    assertEquals('none', tooltip.computedStyleMap().get('display').value);
    assertFalse(paperTooltip._showing);
    const wait = test_util.eventToPromise('show-tooltip', document);
    icon.$.indicator.dispatchEvent(
        new MouseEvent('mouseenter', {bubbles: true, composed: true}));
    return wait.then(() => {
      assertTrue(paperTooltip._showing);
      assertEquals('none', tooltip.computedStyleMap().get('display').value);
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
});
