// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://webui-test/cr_elements/cr_policy_strings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SiteDetailsPermissionDeviceEntryElement} from 'chrome://settings/lazy_load.js';
import {ChooserType, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createChooserException, createSiteException} from './test_util.js';
// clang-format on

/** @fileoverview Suite of tests for site-details-permission-device-entry. */
suite('SiteDetailsPermissionDeviceEntry', function() {
  /**
   * A site details permission device entry element created before each test.
   */
  let testElement: SiteDetailsPermissionDeviceEntryElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  // Initialize a site-details-permission-device-entry before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement =
        document.createElement('site-details-permission-device-entry');
    document.body.appendChild(testElement);
  });

  async function assertUserGranted(
      testElement: SiteDetailsPermissionDeviceEntryElement, origin: string,
      deviceName: string) {
    // The device display name is correct.
    const deviceDisplayName =
        testElement.shadowRoot!.querySelector('.url-directionality');
    assertTrue(!!deviceDisplayName);
    assertEquals(deviceDisplayName.textContent!.trim(), deviceName);

    // The reset button is not hidden.
    const resetButton = testElement.$.resetSite;
    assertFalse(resetButton.hidden);
    // Click the reset button trigger resetChooserExceptionForSite.
    resetButton.click();
    const [chooserType, actualOrigin, exception] =
        await browserProxy.whenCalled('resetChooserExceptionForSite');
    assertEquals(ChooserType.HID_DEVICES, chooserType);
    assertEquals(origin, actualOrigin);
    assertDeepEquals({name: deviceName}, exception);

    // The policy enforced indicator is hidden.
    const policyIndicator =
        testElement.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertFalse(!!policyIndicator);
  }

  function assertPolicyGranted(
      testElement: SiteDetailsPermissionDeviceEntryElement,
      deviceName: string) {
    // The device display name is correct.
    const deviceDisplayName =
        testElement.shadowRoot!.querySelector('.url-directionality');
    assertTrue(!!deviceDisplayName);
    assertEquals(deviceDisplayName.textContent!.trim(), deviceName);

    // The reset button is hidden.
    assertTrue(testElement.$.resetSite.hidden);

    // The policy enforced indicator is not hidden.
    const policyIndicator =
        testElement.shadowRoot!.querySelector('cr-policy-pref-indicator');
    assertTrue(!!policyIndicator);
  }

  test('User granted chooser exception', async function() {
    testElement.exception = createChooserException(
        ChooserType.HID_DEVICES,
        [
          createSiteException('https://foo.com'),
        ],
        {displayName: 'Gadget', object: {name: 'Gadget'}});

    // Flush the container to ensure that the container is populated.
    flush();

    await assertUserGranted(testElement, 'https://foo.com', 'Gadget');
  });

  test('Policy granted chooser exception', function() {
    testElement.exception = createChooserException(
        ChooserType.HID_DEVICES, [createSiteException('https://foo.com', {
          enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
        })],
        {displayName: 'Gadget', object: {name: 'Gadget'}});

    // Flush the container to ensure that the container is populated.
    flush();

    assertPolicyGranted(testElement, 'Gadget');
  });

  test(
      'Chooser exception with both policy and user granted should show as ' +
          'if policy granted',
      function() {
        testElement.exception = createChooserException(
            ChooserType.HID_DEVICES,
            [
              createSiteException('https://foo.com'),
              createSiteException('https://foo.com', {
                enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
                controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
              }),
            ],
            {displayName: 'Gadget', object: {name: 'Gadget'}});

        // Flush the container to ensure that the container is populated.
        flush();

        assertPolicyGranted(testElement, 'Gadget');
      });
});
