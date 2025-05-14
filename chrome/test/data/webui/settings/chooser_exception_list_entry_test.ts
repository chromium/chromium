// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://webui-test/cr_elements/cr_policy_strings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ChooserExceptionListEntryElement} from 'chrome://settings/lazy_load.js';
import {ChooserType, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {assertTooltipIsHidden, createChooserException, createSiteException} from './test_util.js';
// clang-format on

/**
 * @fileoverview Suite of tests for chooser-exception-list-entry.
 */

suite('ChooserExceptionListEntry', function() {
  /**
   * A chooser exception list entry element created before each test.
   */
  let testElement: ChooserExceptionListEntryElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  // Initialize a chooser-exception-list-entry before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('chooser-exception-list-entry');
    document.body.appendChild(testElement);
  });

  test(
      'User granted chooser exceptions should show the reset button',
      function() {
        testElement.exception =
            createChooserException(ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com'),
            ]);

        // Flush the container to ensure that the container is populated.
        flush();

        const siteListEntry =
            testElement.shadowRoot!.querySelector('site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu button is not visible.
        assertFalse(isChildVisible(siteListEntry, '#actionMenuButton'));
        // Ensure that the reset button is visible.
        assertTrue(isChildVisible(siteListEntry, '#resetSite'));
        // Ensure that the policy enforced indicator is not visible.
        assertFalse(isChildVisible(siteListEntry, 'cr-policy-pref-indicator'));
      });

  test(
      'Policy granted chooser exceptions should show the policy indicator ' +
          'icon',
      function() {
        testElement.exception =
            createChooserException(ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com', {
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
              }),
            ]);

        // Flush the container to ensure that the container is populated.
        flush();

        const siteListEntry =
            testElement.shadowRoot!.querySelector('site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu button is not visible.
        assertFalse(isChildVisible(siteListEntry, '#actionMenuButton'));
        // Ensure that the reset button is not visible.
        assertFalse(isChildVisible(siteListEntry, '#resetSite'));
        // Ensure that the policy enforced indicator is visible.
        assertTrue(isChildVisible(siteListEntry, 'cr-policy-pref-indicator'));
      });

  test(
      'Site exceptions from mixed sources should display properly', function() {
        // The SiteExceptions returned by the getChooserExceptionList will be
        // ordered by provider source, then alphabetically by requesting origin
        // and embedding origin.
        testElement.exception =
            createChooserException(ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com', {
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
              }),
              createSiteException('https://bar.com'),
            ]);

        // Flush the container to ensure that the container is populated.
        flush();

        const siteListEntries =
            testElement.$.listContainer.querySelectorAll('site-list-entry');
        assertTrue(!!siteListEntries);
        assertEquals(siteListEntries.length, 2);

        // The first entry should be policy enforced.
        assertFalse(isChildVisible(siteListEntries[0]!, '#actionMenuButton'));
        assertFalse(isChildVisible(siteListEntries[0]!, '#resetSite'));
        assertTrue(
            isChildVisible(siteListEntries[0]!, 'cr-policy-pref-indicator'));

        // The second entry should be user granted.
        assertFalse(isChildVisible(siteListEntries[1]!, '#actionMenuButton'));
        assertTrue(isChildVisible(siteListEntries[1]!, '#resetSite'));
        assertFalse(
            isChildVisible(siteListEntries[1]!, 'cr-policy-pref-indicator'));
      });

  test(
      'The show-tooltip event is fired when mouse hovers over policy indicator',
      function() {
        testElement.exception =
            createChooserException(ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com', {
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
              }),
            ]);

        // Flush the container to ensure that the container is populated.
        flush();

        const siteListEntry =
            testElement.shadowRoot!.querySelector('site-list-entry');
        assertTrue(!!siteListEntry);

        const policyIndicator =
            siteListEntry.shadowRoot!.querySelector('cr-policy-pref-indicator');
        assertTrue(!!policyIndicator);

        const icon =
            policyIndicator.shadowRoot!.querySelector('cr-tooltip-icon');
        assertTrue(!!icon);

        const crTooltip = icon.shadowRoot.querySelector('cr-tooltip');
        assertTrue(!!crTooltip);

        // This tooltip is never shown since a common tooltip will be used.
        assertTrue(!!crTooltip);
        assertTooltipIsHidden(crTooltip);

        const wait = eventToPromise('show-tooltip', document);
        icon.$.indicator.dispatchEvent(
            new MouseEvent('mouseenter', {bubbles: true, composed: true}));
        return wait.then(() => {
          assertTooltipIsHidden(crTooltip);
        });
      });

  test(
      'The reset button calls the resetChooserExceptionForSite method',
      async function() {
        testElement.exception =
            createChooserException(ChooserType.USB_DEVICES, [
              createSiteException('https://foo.com'),
            ]);

        // Flush the container to ensure that the container is populated.
        flush();

        const siteListEntry =
            testElement.shadowRoot!.querySelector('site-list-entry');
        assertTrue(!!siteListEntry);

        // Ensure that the action menu button is not visible.
        assertFalse(isChildVisible(siteListEntry, '#actionMenuButton'));

        const resetButton =
            siteListEntry.shadowRoot!.querySelector<HTMLElement>('#resetSite');
        assertTrue(!!resetButton);
        assertTrue(isVisible(resetButton));

        resetButton.click();
        const args =
            await browserProxy.whenCalled('resetChooserExceptionForSite');

        // The args should be the chooserType, origin, and object.
        assertEquals(ChooserType.USB_DEVICES, args[0]);
        assertEquals('https://foo.com', args[1]);
        assertEquals('object', typeof args[2]);
      });
});
