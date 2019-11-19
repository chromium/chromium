// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for chooser-exception-list-entry.
 */

suite('ChooserExceptionListEntry', function() {
  /**
   * A chooser exception list entry element created before each test.
   * @type {ChooserExceptionListEntry}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  // Initialize a chooser-exception-list-entry before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('chooser-exception-list-entry');
    document.body.appendChild(testElement);
  });

  function createSiteException(origin, override) {
    return Object.assign(
        {
          embeddingOrigin: origin,
          incognito: false,
          origin: origin,
          displayName: origin,
          setting: settings.ContentSetting.DEFAULT,
          enforcement: null,
          controlledBy: chrome.settingsPrivate.ControlledBy.PRIMARY_USER,
        },
        override || {});
  }

  function createChooserException(chooserType, sites, override) {
    return Object.assign(
        {
          chooserType: chooserType,
          displayName: '',
          object: {},
          sites: sites,
        },
        override || {});
  }

  test(
      'User granted chooser exceptions should show the reset button',
      function() {
        testElement.exception =
            createChooserException(settings.ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com'),
            ]);

        // Flush the container to ensure that the container is populated.
        Polymer.dom.flush();

        const siteListEntry = testElement.$$('site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu button container is hidden.
        const dotsMenu = siteListEntry.$$('#actionMenuButton');
        assertTrue(!!dotsMenu);
        assertTrue(dotsMenu.hidden);

        // Ensure that the reset button is not hidden.
        const resetButton = siteListEntry.$$('#resetSite');
        assertTrue(!!resetButton);
        assertFalse(resetButton.hidden);

        // Ensure that the policy enforced indicator is hidden.
        const policyIndicator = siteListEntry.$$('cr-policy-pref-indicator');
        assertFalse(!!policyIndicator);
      });

  test(
      'Policy granted chooser exceptions should show the policy indicator ' +
          'icon',
      function() {
        testElement.exception =
            createChooserException(settings.ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com', {
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
              }),
            ]);

        // Flush the container to ensure that the container is populated.
        Polymer.dom.flush();

        const siteListEntry = testElement.$$('site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu button container is hidden.
        const dotsMenu = siteListEntry.$$('#actionMenuButton');
        assertTrue(!!dotsMenu);
        assertTrue(dotsMenu.hidden);

        // Ensure that the reset button is hidden.
        const resetButton = siteListEntry.$$('#resetSite');
        assertTrue(!!resetButton);
        assertTrue(resetButton.hidden);

        // Ensure that the policy enforced indicator is not hidden.
        const policyIndicator = siteListEntry.$$('cr-policy-pref-indicator');
        assertTrue(!!policyIndicator);
      });

  test(
      'Site exceptions from mixed sources should display properly', function() {
        // The SiteExceptions returned by the getChooserExceptionList will be
        // ordered by provider source, then alphabetically by requesting origin
        // and embedding origin.
        testElement.exception =
            createChooserException(settings.ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com', {
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
              }),
              createSiteException('https://bar.com'),
            ]);

        // Flush the container to ensure that the container is populated.
        Polymer.dom.flush();

        const siteListEntries =
            testElement.$.listContainer.querySelectorAll('site-list-entry');
        assertTrue(!!siteListEntries);
        assertEquals(siteListEntries.length, 2);

        // The first entry should be policy enforced.
        const firstDotsMenu = siteListEntries[0].$$('#actionMenuButton');
        assertTrue(!!firstDotsMenu);
        assertTrue(firstDotsMenu.hidden);

        const firstResetButton = siteListEntries[0].$$('#resetSite');
        assertTrue(!!firstResetButton);
        assertTrue(firstResetButton.hidden);

        const firstPolicyIndicator =
            siteListEntries[0].$$('cr-policy-pref-indicator');
        assertTrue(!!firstPolicyIndicator);

        // The second entry should be user granted.
        const secondDotsMenu = siteListEntries[1].$$('#actionMenuButton');
        assertTrue(!!secondDotsMenu);
        assertTrue(secondDotsMenu.hidden);

        const secondResetButton = siteListEntries[1].$$('#resetSite');
        assertTrue(!!secondResetButton);
        assertFalse(secondResetButton.hidden);

        const secondPolicyIndicator =
            siteListEntries[1].$$('cr-policy-pref-indicator');
        assertFalse(!!secondPolicyIndicator);
      });

  test(
      'The show-tooltip event is fired when mouse hovers over policy indicator',
      function() {
        testElement.exception =
            createChooserException(settings.ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com', {
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
              }),
            ]);

        // Flush the container to ensure that the container is populated.
        Polymer.dom.flush();

        const siteListEntry = testElement.$$('site-list-entry');
        assertTrue(!!siteListEntry);

        const policyIndicator = siteListEntry.$$('cr-policy-pref-indicator');
        assertTrue(!!policyIndicator);

        const icon = policyIndicator.$$('cr-tooltip-icon');
        assertTrue(!!icon);

        const paperTooltip = icon.$$('paper-tooltip');
        assertTrue(!!paperTooltip);

        // This tooltip is never shown since a common tooltip will be used.
        assertTrue(!!paperTooltip);
        assertEquals(
            'none', paperTooltip.computedStyleMap().get('display').value);
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

  test(
      'The reset button calls the resetChooserExceptionForSite method',
      function() {
        testElement.exception =
            createChooserException(settings.ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com'),
            ]);

        // Flush the container to ensure that the container is populated.
        Polymer.dom.flush();

        const siteListEntry = testElement.$$('site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu button is hidden.
        const dotsMenu = siteListEntry.$$('#actionMenuButton');
        assertTrue(!!dotsMenu);
        assertTrue(dotsMenu.hidden);

        // Ensure that the reset button is not hidden.
        const resetButton = siteListEntry.$$('#resetSite');
        assertTrue(!!resetButton);
        assertFalse(resetButton.hidden);

        resetButton.click();
        return browserProxy.whenCalled('resetChooserExceptionForSite')
            .then(function(args) {
              // The args should be the chooserType, origin, embeddingOrigin,
              // and object.
              assertEquals(settings.ChooserType.USB_DEVICES, args[0]);
              assertEquals('https://foo.com', args[1]);
              assertEquals('https://foo.com', args[2]);
              assertEquals('object', typeof args[3]);
            });
      });
});
