// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';

import type {SettingsReviewNotificationPermissionsElement} from 'chrome://settings/lazy_load.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
// clang-format on

suite('CrSettingsReviewNotificationPermissionsInteractiveUITest', function() {
  // The mock proxy object to use during test.
  let browserProxy: TestSafetyHubBrowserProxy;

  let testElement: SettingsReviewNotificationPermissionsElement;

  const origin1 = 'https://www.example1.com:443';
  const detail1 = 'About 4 notifications a day';
  const origin2 = 'https://www.example2.com:443';
  const detail2 = 'About 1 notification a day';

  const mockData = [
    {
      origin: origin1,
      notificationInfoString: detail1,
    },
    {
      origin: origin2,
      notificationInfoString: detail2,
    },
  ];

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

  setup(function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    browserProxy.setNotificationPermissionReview(mockData);
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('review-notification-permissions');
    document.body.appendChild(testElement);

    flush();
  });

  // Opens the action menu for a particular element in the list.
  function openActionMenu(index: number) {
    const item = getEntries()[index];
    assert(item);
    const actionMenu = item.querySelector<HTMLElement>('#actionMenuButton');
    assert(actionMenu);
    actionMenu.click();
    flush();
  }

  function getEntries() {
    return testElement.shadowRoot!.querySelectorAll('.site-list .site-entry');
  }

  /**
   * Tests whether the focus returns to the expand button after clicking undo
   * following a click on the block button.
   */
  test('Undo Block Click Refocus', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    // User blocks the site.
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '.site-entry #block')!.click();

    const focusPromise = waitForFocusEventOnExpandButton();
    testElement.$.undoToast.querySelector('cr-button')!.click();
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);
    await focusPromise;
    assertExpandButtonFocus();
  });

  /**
   * Tests whether the focus returns to the expand button after clicking undo
   * following a click on the ignore button.
   */
  test('Undo Ignore Click Refocus', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    openActionMenu(0);
    // User ignores notifications for the site.
    testElement.shadowRoot!.querySelector<HTMLElement>('#ignore')!.click();

    const focusPromise = waitForFocusEventOnExpandButton();
    testElement.$.undoToast.querySelector('cr-button')!.click();
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);
    await focusPromise;
    assertExpandButtonFocus();
  });

  /**
   * Tests whether the focus returns to the expand button after clicking undo
   * following a click on the reset button.
   */
  test('Undo Reset Click Refocus', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    openActionMenu(0);
    // User resets permissions for the site.
    testElement.shadowRoot!.querySelector<HTMLElement>('#reset')!.click();

    const focusPromise = waitForFocusEventOnExpandButton();
    testElement.$.undoToast.querySelector('cr-button')!.click();
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);
    await focusPromise;
    assertExpandButtonFocus();
  });

  /**
   * Tests whether the focus returns to the expand button after clicking undo
   * following a click on the block-all button.
   */
  test('Block All Click Refocus', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#blockAllButton')!.click();
    // Click undo button.
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#undoToast cr-button')!.click();

    const focusPromise = waitForFocusEventOnExpandButton();
    testElement.$.undoToast.querySelector('cr-button')!.click();
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);
    await focusPromise;
    assertExpandButtonFocus();
  });
});
