// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {SettingsReviewNotificationPermissionsElement, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrActionMenuElement} from 'chrome://settings/settings.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

suite('CrSettingsReviewNotificationPermissionsTest', function() {
  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  let testElement: SettingsReviewNotificationPermissionsElement;

  const origin_1 = 'www.example1.com';
  const detail_1 = 'About 4 notifications a day';
  const origin_2 = 'www.example2.com';
  const detail_2 = 'About 1 notification a day';

  function assertNotification(
      toastShouldBeOpen: boolean, toastText?: string): void {
    const undoToast =
        testElement.shadowRoot!.getElementById('undoToast') as CrToastElement;
    if (!toastShouldBeOpen) {
      assertFalse(undoToast.open);
      return;
    }
    assertTrue(undoToast.open);
    const notification = testElement.shadowRoot!.getElementById(
                             'undoNotification') as HTMLElement;
    assertEquals(notification.textContent!.trim(), toastText);
  }

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    const mock_data = [
      {
        origin: origin_1,
        notificationInfoString: detail_1,
      },
      {
        origin: origin_2,
        notificationInfoString: detail_2,
      },
    ];
    browserProxy.setNotificationPermissionReview(mock_data);
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    testElement = document.createElement('review-notification-permissions');
    document.body.appendChild(testElement);
    flush();
  });

  teardown(function() {
    testElement.remove();
  });

  /**
   * Opens the action menu for a particular element in the list.
   * @param index The index of the child element (which site) to
   *     open the action menu for.
   */
  function openActionMenu(index: number) {
    const menu_empty = testElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(menu_empty === null);

    const item = testElement.shadowRoot!.querySelectorAll('.cr-row')[index]!;
    (item.querySelector('#actionMenuButton')! as HTMLElement).click();
    flush();

    const menu = testElement.shadowRoot!.querySelector('cr-action-menu')! as
        CrActionMenuElement;
    assertTrue(isVisible(menu.getDialog()));
  }

  test('Notification Permission strings', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    const entries = testElement.shadowRoot!.querySelectorAll('.cr-row');
    assertEquals(2, entries.length);

    // Check that the text describing the changed permissions is correct.
    assertEquals(
        origin_1,
        entries[0]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        detail_1,
        entries[0]!.querySelector('.second-line')!.textContent!.trim());
    assertEquals(
        origin_2,
        entries[1]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        detail_2,
        entries[1]!.querySelector('.second-line')!.textContent!.trim());
  });

  /**
   * Tests whether clicking on the block button results in the appropriate
   * browser proxy call and shows the notification toast element.
   */
  test('Dont Allow Click', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    const entries = testElement.shadowRoot!.querySelectorAll('.cr-row');
    assertEquals(2, entries.length);

    assertNotification(false);

    // User clicks don't allow.
    const element = entries[0]!.querySelector('#block')! as HTMLElement;
    element.click();
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        entries[0]!.querySelector('.site-representation')!.textContent!.trim();
    await browserProxy.whenCalled('blockNotificationPermissionForOrigin')
        .then(function([origin]) {
          assertEquals(origin, expectedOrigin);
        });
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewBlockedToastLabel',
            expectedOrigin));
  });

  /**
   * Tests whether clicking on the ignore action via the action menu results in
   * the appropriate browser proxy call, closes the action menu, and shows the
   * notification toast element.
   */
  test('Ignore Click', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    const entries = testElement.shadowRoot!.querySelectorAll('.cr-row');
    assertEquals(2, entries.length);

    assertNotification(false);

    // User clicks ignore.
    openActionMenu(0);
    const reset =
        testElement.shadowRoot!.querySelector<HTMLElement>('#ignore')!;
    reset.click();
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        entries[0]!.querySelector('.site-representation')!.textContent!.trim();
    await browserProxy.whenCalled('ignoreNotificationPermissionForOrigin')
        .then(function([origin]) {
          assertEquals(origin, expectedOrigin);
        });
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewIgnoredToastLabel',
            expectedOrigin));
    // Ensure the action menu is closed.
    const menu = testElement.shadowRoot!.querySelector('cr-action-menu')! as
        CrActionMenuElement;
    const dialog = menu.getDialog() as HTMLDialogElement;
    assertFalse(isVisible(dialog));
  });

  /**
   * Tests whether clicking on the reset action via the action menu results in
   * the appropriate browser proxy call, closes the action menu, and shows the
   * notification toast element.
   */
  test('Reset Click', async function() {
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();

    const entries = testElement.shadowRoot!.querySelectorAll('.cr-row');
    assertEquals(2, entries.length);

    assertNotification(false);

    // User clicks reset.
    openActionMenu(0);
    const reset = testElement.shadowRoot!.querySelector<HTMLElement>('#reset')!;
    reset.click();
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        entries[0]!.querySelector('.site-representation')!.textContent!.trim();
    await browserProxy.whenCalled('resetNotificationPermissionForOrigin')
        .then(function([origin]) {
          assertEquals(origin, expectedOrigin);
        });
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewResetToastLabel',
            expectedOrigin));
    // Ensure the action menu is closed.
    const menu = testElement.shadowRoot!.querySelector('cr-action-menu')! as
        CrActionMenuElement;
    const dialog = menu.getDialog() as HTMLDialogElement;
    assertFalse(isVisible(dialog));
  });
});
