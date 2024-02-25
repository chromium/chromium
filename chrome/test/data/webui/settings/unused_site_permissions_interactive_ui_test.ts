// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {SettingsUnusedSitePermissionsElement} from 'chrome://settings/lazy_load.js';
import {ContentSettingsTypes, SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';

import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';

import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
// clang-format on

suite('CrSettingsUnusedSitePermissionsInteractiveUITest', function() {
  // The mock proxy object to use during test.
  let browserProxy: TestSafetyHubBrowserProxy;

  let testElement: SettingsUnusedSitePermissionsElement;

  const permissions = [
    ContentSettingsTypes.GEOLOCATION,
    ContentSettingsTypes.MIC,
    ContentSettingsTypes.CAMERA,
    ContentSettingsTypes.MIDI_DEVICES,
  ];

  const mockData = [1, 2, 3, 4].map(
      i => ({
        origin: `https://www.example${i}.com:443`,
        permissions: permissions.slice(0, i),
        expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
      }));

  function assertExpandButtonFocus() {
    const expandButton =
        testElement.shadowRoot!.querySelector('cr-expand-button');
    assert(expandButton);
    assertTrue(expandButton.matches(':focus-within'));
  }

  function waitForFocusEventOnExpandButton(): Promise<void> {
    return new Promise((resolve) => {
      const expandButton =
          testElement.shadowRoot!.querySelector('cr-expand-button');
      assert(expandButton);
      const callback = () => {
        expandButton.removeEventListener('focus', callback);
        resolve();
      };
      expandButton.addEventListener('focus', callback);
    });
  }

  setup(async function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    browserProxy.setUnusedSitePermissions(mockData);
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-unused-site-permissions');
    testElement.setModelUpdateDelayMsForTesting(0);
    document.body.appendChild(testElement);
    await browserProxy.whenCalled('getRevokedUnusedSitePermissionsList');
    flush();
  });

  /**
   * Tests whether the focus returns to the expand button after clicking undo
   * following a click on the "Allow again" button.
   */
  test('Undo Allow Again Click Refocus', async function() {
    // Click "Allow Again" button.
    const siteList =
        testElement.shadowRoot!.querySelectorAll('.site-list .site-entry');
    siteList[0]!.querySelector('cr-icon-button')!.click();

    const focusPromise = waitForFocusEventOnExpandButton();
    // Click on "Undo" button.
    testElement.$.undoToast.querySelector('cr-button')!.click();
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
    await focusPromise;
    assertExpandButtonFocus();
  });

  /**
   * Tests whether the focus returns to the expand button after clicking undo
   * following a click on the "Got it" button.
   */
  test('Undo Got It Click Refocus', async function() {
    // Click "Got it" button.
    const button = testElement.shadowRoot!.querySelector<HTMLElement>(
        '.bulk-action-button');
    assertTrue(!!button);
    button.click();

    const focusPromise = waitForFocusEventOnExpandButton();
    // Click on "Undo" button.
    testElement.$.undoToast.querySelector('cr-button')!.click();
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
    await focusPromise;
    assertExpandButtonFocus();
  });
});
