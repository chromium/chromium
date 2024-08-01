// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {SettingsReviewNotificationPermissionsElement} from 'chrome://settings/lazy_load.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, resetRouterForTesting, Router, routes, SafetyCheckNotificationsModuleInteractions} from 'chrome://settings/settings.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';

// clang-format on

suite('CrSettingsReviewNotificationPermissionsTest', function() {
  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSafetyHubBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

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

  function assertNotification(
      toastShouldBeOpen: boolean, toastText?: string): void {
    const undoToast = testElement.$.undoToast;
    if (!toastShouldBeOpen) {
      assertFalse(undoToast.open);
      return;
    }
    assertTrue(undoToast.open);
    assertEquals(testElement.$.undoNotification.textContent!.trim(), toastText);
  }

  /**
   * Clicks the Undo button and verifies that the correct origins are given to
   * the browser proxy call.
   */
  async function assertUndo(expectedProxyCall: string, index: number) {
    const entries = getEntries();
    const expectedOrigin =
        entries[index]!.querySelector(
                           '.site-representation')!.textContent!.trim();
    browserProxy.resetResolver(expectedProxyCall);
    testElement.$.undoToast.querySelector('cr-button')!.click();
    const origins = await browserProxy.whenCalled(expectedProxyCall);
    assertEquals(origins[0], expectedOrigin);
    assertNotification(false);
  }

  /* Asserts for each row whether or not it is animating. */
  function assertAnimation(expectedAnimation: boolean[]) {
    const rows = getEntries();

    assertEquals(
        rows.length, expectedAnimation.length,
        'Provided ' + expectedAnimation.length +
            ' expectations but there are ' + rows.length + ' rows');
    for (let i = 0; i < rows.length; ++i) {
      assertEquals(
          expectedAnimation[i]!, rows[i]!.classList.contains('removed'),
          'Expectation not met for row #' + i);
    }
  }

  async function assertMetricsInteraction(
      interaction: SafetyCheckNotificationsModuleInteractions) {
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckNotificationsModuleInteractionsHistogram');
    assertEquals(interaction, result);
    metricsBrowserProxy.resetResolver(
        'recordSafetyCheckNotificationsModuleInteractionsHistogram');
  }

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('review-notification-permissions');
    testElement.setModelUpdateDelayMsForTesting(0);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_NOTIFICATIONS);
    document.body.appendChild(testElement);
    // Wait until the element has asked for the list of revoked permissions
    // that will be shown for review.
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();
  }

  setup(async function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    browserProxy.setNotificationPermissionReview(mockData);
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    resetRouterForTesting();
    await createPage();
    // Clear the metrics that were recorded as part of the initial creation of
    // the page.
    metricsBrowserProxy.reset();
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
    const menu1 = testElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!menu1);
    assertFalse(isVisible(menu1.getDialog()));

    const item = getEntries()[index]!;
    item.querySelector<HTMLElement>('#actionMenuButton')!.click();
    flush();

    const menu2 = testElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!menu2);
    assertTrue(isVisible(menu2.getDialog()));
  }

  function getEntries() {
    return testElement.shadowRoot!.querySelectorAll('.site-list .site-entry');
  }

  test('Capture metrics on visit', async function() {
    await createPage();
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckNotificationsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckNotificationsModuleInteractions.OPEN_REVIEW_UI, result);
  });

  test('Notification Permission strings', async function() {
    const entries = getEntries();
    assertEquals(2, entries.length);

    // Check that the text describing the changed permissions is correct.
    assertEquals(
        origin1,
        entries[0]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        detail1,
        entries[0]!.querySelector('.second-line')!.textContent!.trim());
    assertEquals(
        origin2,
        entries[1]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        detail2,
        entries[1]!.querySelector('.second-line')!.textContent!.trim());
  });

  /**
   * Tests whether clicking on the block button results in the appropriate
   * browser proxy call and shows the notification toast element.
   */
  test('Dont Allow Click', async function() {
    const entries = getEntries();
    assertEquals(2, entries.length);

    assertNotification(false);
    assertAnimation([false, false]);

    // User clicks don't allow.
    const element = entries[0]!.querySelector<HTMLElement>('#block');
    assertTrue(!!element);
    element.click();
    assertAnimation([true, false]);
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        entries[0]!.querySelector('.site-representation')!.textContent!.trim();
    const origins =
        await browserProxy.whenCalled('blockNotificationPermissionForOrigins');
    assertEquals(origins[0], expectedOrigin);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewBlockedToastLabel',
            expectedOrigin));
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.BLOCK);
  });

  /**
   * Tests whether clicking on the ignore action via the action menu results in
   * the appropriate browser proxy call, closes the action menu, and shows the
   * notification toast element.
   */
  test('Ignore Click', async function() {
    const entries = getEntries();
    assertEquals(2, entries.length);

    assertNotification(false);
    assertAnimation([false, false]);

    // User clicks ignore.
    openActionMenu(0);
    const reset =
        testElement.shadowRoot!.querySelector<HTMLElement>('#ignore')!;
    reset.click();
    assertAnimation([true, false]);
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        entries[0]!.querySelector('.site-representation')!.textContent!.trim();
    const origins =
        await browserProxy.whenCalled('ignoreNotificationPermissionForOrigins');
    assertEquals(origins[0], expectedOrigin);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewIgnoredToastLabel',
            expectedOrigin));
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.IGNORE);
    // Ensure the action menu is closed.
    const menu = testElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!menu);
    assertFalse(isVisible(menu.getDialog()));
  });

  /**
   * Tests whether clicking on the reset action via the action menu results in
   * the appropriate browser proxy call, closes the action menu, and shows the
   * notification toast element.
   */
  test('Reset Click', async function() {
    const entries = getEntries();
    assertEquals(2, entries.length);

    assertNotification(false);
    assertAnimation([false, false]);

    // User clicks reset.
    openActionMenu(0);
    const reset = testElement.shadowRoot!.querySelector<HTMLElement>('#reset')!;
    reset.click();
    assertAnimation([true, false]);
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        entries[0]!.querySelector('.site-representation')!.textContent!.trim();
    const origins =
        await browserProxy.whenCalled('resetNotificationPermissionForOrigins');
    assertEquals(origins[0], expectedOrigin);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewResetToastLabel',
            expectedOrigin));
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.RESET);
    // Ensure the action menu is closed.
    const menu = testElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!menu);
    assertFalse(isVisible(menu.getDialog()));
  });

  /**
   * Tests whether clicking the Undo button after blocking a site correctly
   * resets the site to allow notifications and makes the toast element
   * disappear.
   */
  test('Undo Block Click', async function() {
    // User blocks the site.
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '.site-entry #block')!.click();
    assertAnimation([true, false]);
    metricsBrowserProxy.resetResolver(
        'recordSafetyCheckNotificationsModuleInteractionsHistogram');

    await assertUndo('allowNotificationPermissionForOrigins', 0);
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);
    assertAnimation([false, false]);
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.UNDO_BLOCK);
  });

  /**
   * Tests whether clicking the Undo button after ignoring notification a site
   * for permission review correctly removes the site from the blocklist
   * and makes the toast element disappear.
   */
  test('Undo Ignore Click', async function() {
    openActionMenu(0);
    // User ignores notifications for the site.
    testElement.shadowRoot!.querySelector<HTMLElement>('#ignore')!.click();
    assertAnimation([true, false]);
    metricsBrowserProxy.resetResolver(
        'recordSafetyCheckNotificationsModuleInteractionsHistogram');

    await assertUndo('undoIgnoreNotificationPermissionForOrigins', 0);
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);
    assertAnimation([false, false]);
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.UNDO_IGNORE);
  });

  /**
   * Tests whether clicking the Undo button after resetting notification
   * permissions for a site correctly resets the site to allow notifications
   * and makes the toast element disappear.
   */
  test('Undo Reset Click', async function() {
    openActionMenu(0);
    // User resets permissions for the site.
    testElement.shadowRoot!.querySelector<HTMLElement>('#reset')!.click();
    assertAnimation([true, false]);
    metricsBrowserProxy.resetResolver(
        'recordSafetyCheckNotificationsModuleInteractionsHistogram');

    await assertUndo('allowNotificationPermissionForOrigins', 0);
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);
    assertAnimation([false, false]);
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.UNDO_RESET);
  });

  /**
   * Tests whether clicking the Block All button will block notifications for
   * all entries in the list, and whether clicking the Undo button afterwards
   * will allow the notifications for that same list.
   */
  test('Block All Click', async function() {
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#blockAllButton')!.click();
    const origins1 =
        await browserProxy.whenCalled('blockNotificationPermissionForOrigins');
    assertEquals(2, origins1.length);
    assertEquals(
        JSON.stringify(origins1.sort()), JSON.stringify([origin1, origin2]));
    const notificationText =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewBlockAllToastLabel', 2);
    assertNotification(true, notificationText);
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.BLOCK_ALL);

    // Click undo button.
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#undoToast cr-button')!.click();
    const origins2 =
        await browserProxy.whenCalled('allowNotificationPermissionForOrigins');
    assertEquals(2, origins2.length);
    assertEquals(
        JSON.stringify(origins2.sort()), JSON.stringify([origin1, origin2]));
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.UNDO_BLOCK);
  });

  /**
   * Tests whether pressing the ctrl+z key combination correctly undoes the last
   * user action.
   */
  test('Undo Block via Ctrl+Z', async function() {
    // User blocks the site.
    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '.site-entry #block')!.click();
    assertAnimation([true, false]);
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.BLOCK);

    const entries = getEntries();
    const expectedOrigin =
        entries[0]!.querySelector('.site-representation')!.textContent!.trim();
    browserProxy.resetResolver('allowNotificationPermissionForOrigins');
    const notificationText = testElement.i18n(
        'safetyCheckNotificationPermissionReviewBlockedToastLabel',
        expectedOrigin);
    assertNotification(true, notificationText);

    keyDownOn(document.documentElement, 0, isMac ? 'meta' : 'ctrl', 'z');

    const origins =
        await browserProxy.whenCalled('allowNotificationPermissionForOrigins');
    assertEquals(origins[0], expectedOrigin);
    assertNotification(false);
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.UNDO_BLOCK);
  });

  test('Block All Click single entry', async function() {
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, [{
          origin: origin1,
          notificationInfoString: detail1,
        }]);
    await flushTasks();

    const entries = getEntries();
    assertEquals(1, entries.length);

    testElement.shadowRoot!.querySelector<HTMLElement>(
                               '#blockAllButton')!.click();

    const blockedOrigins =
        await browserProxy.whenCalled('blockNotificationPermissionForOrigins');
    assertEquals(blockedOrigins[0], origin1);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewBlockedToastLabel',
            origin1));
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.BLOCK_ALL);
  });

  test('Completion State', async function() {
    // Before review, header and list of permissions are visible.
    assertTrue(isChildVisible(testElement, '#review-header'));
    assertTrue(isChildVisible(testElement, '.site-list'));
    assertFalse(isChildVisible(testElement, '#done-header'));

    // Through reviewing permissions the permission list is empty and only the
    // completion info is visible.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();
    assertFalse(isChildVisible(testElement, '#review-header'));
    assertFalse(isChildVisible(testElement, '.site-list'));
    assertTrue(isChildVisible(testElement, '#done-header'));

    // The element returns to showing the list of permissions when new items are
    // added while the completion state is visible.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);
    await flushTasks();
    assertTrue(isChildVisible(testElement, '#review-header'));
    assertTrue(isChildVisible(testElement, '.site-list'));
    assertFalse(isChildVisible(testElement, '#done-header'));
  });

  test('Collapsible List', async function() {
    const expandButton =
        testElement.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);

    const notificationPermissionList =
        testElement.shadowRoot!.querySelector('cr-collapse');
    assertTrue(!!notificationPermissionList);

    // Button and list start out expanded.
    assertTrue(expandButton.expanded);
    assertTrue(notificationPermissionList.opened);

    // User collapses the list.
    expandButton.click();
    await expandButton.updateComplete;
    await assertMetricsInteraction(
        SafetyCheckNotificationsModuleInteractions.MINIMIZE);

    // Button and list are collapsed.
    assertFalse(expandButton.expanded);
    assertFalse(notificationPermissionList.opened);

    // User expands the list.
    expandButton.click();
    await expandButton.updateComplete;

    // Button and list are expanded.
    assertTrue(expandButton.expanded);
    assertTrue(notificationPermissionList.opened);
  });

  /**
   * Tests whether header string updated based on the notification permission
   * list size for plural and singular case.
   */
  test('Header String', async function() {
    // Check header string for plural case.
    let entries = getEntries();
    assertEquals(2, entries.length);

    const headerElement =
        testElement.shadowRoot!.querySelector('#review-header h2');
    assertTrue(headerElement !== null);

    const headerStringTwo =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewPrimaryLabel', 2);
    assertEquals(headerStringTwo, headerElement.textContent!.trim());

    // Check header string for singular case.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, [{
          origin: origin1,
          notificationInfoString: detail1,
        }]);
    await flushTasks();

    entries = getEntries();
    assertEquals(1, entries.length);

    const headerStringOne =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckNotificationPermissionReviewPrimaryLabel', 1);
    assertEquals(headerStringOne, headerElement.textContent!.trim());
  });

  test('Review list size record metrics', async function() {
    browserProxy.setNotificationPermissionReview(mockData);
    await createPage();
    const resultNumSites = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckNotificationsListCountHistogram');
    assertEquals(mockData.length, resultNumSites);

    metricsBrowserProxy.resetResolver(
        'recordSafetyCheckNotificationsListCountHistogram');

    browserProxy.setNotificationPermissionReview([]);
    await createPage();
    const resultEmpty = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckNotificationsListCountHistogram');
    assertEquals(0, resultEmpty);
  });
});
