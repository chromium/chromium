// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent, SettingsSafetyHubNotificationPermissionsModuleElement} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, Router, routes, SafetyCheckNotificationsModuleInteractions, SettingsPluralStringProxyImpl, SettingsRoutes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
// clang-format on

suite('CrSettingsSafetyHubNotificationPermissionsTest', function() {
  let browserProxy: TestSafetyHubBrowserProxy;
  let pluralStringProxy: TestPluralStringProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  let testElement: SettingsSafetyHubNotificationPermissionsModuleElement;
  let testRoutes: SettingsRoutes;

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

  function getEntries(): NodeListOf<HTMLElement> {
    return testElement.$.module.shadowRoot!.querySelectorAll<HTMLElement>(
        '.site-entry');
  }

  function assertNotification(
      toastShouldBeOpen: boolean, toastText?: string): void {
    const undoToast = testElement.$.undoToast;
    if (!toastShouldBeOpen) {
      assertFalse(undoToast.open);
      return;
    }
    assertTrue(undoToast.open);
    if (toastText) {
      assertEquals(
          testElement.$.undoNotification.textContent!.trim(), toastText);
    }
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
    metricsBrowserProxy.reset();
    testElement.$.toastUndoButton.click();
    const origins = await browserProxy.whenCalled(expectedProxyCall);
    assertEquals(origins[0], expectedOrigin);
    assertNotification(false);
  }

  /**
   * Assert expected plural string is populated. Whenever getPluralString is
   * called, TestPluralStringProxy stacks them in args. If getPluralString is
   * called multiple times, passing 'index' will make the corresponding callback
   * checked.
   */
  async function assertPluralString(
      messageName: string, itemCount: number, index: number = 0) {
    await pluralStringProxy.whenCalled('getPluralString');
    const params = pluralStringProxy.getArgs('getPluralString')[index];
    await flushTasks();
    assertEquals(messageName, params.messageName);
    assertEquals(itemCount, params.itemCount);
    pluralStringProxy.resetResolver('getPluralString');
  }

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement(
        'settings-safety-hub-notification-permissions-module');
    Router.getInstance().navigateTo(testRoutes.SAFETY_HUB);
    document.body.appendChild(testElement);
    // Wait until the element has asked for the list of revoked permissions
    // that will be shown for review.
    await browserProxy.whenCalled('getNotificationPermissionReview');
    flush();
  }

  /**
   * Clicks the button to the HTMLElement passed as a parameter.
   * @param button The HTMLElement that will be clicked.
   */
  function clickButton(button: HTMLElement|null) {
    assertTrue(!!button);
    button.click();
    flush();
  }

  /**
   * Opens the action menu for a particular element in the list.
   * @param index The index of the child element (which site) to
   *     open the action menu for.
   */
  function openActionMenu(index: number) {
    assertFalse(isVisible(testElement.$.actionMenu.getDialog()));

    clickButton(getEntries()[index]!.querySelector('#moreActionButton'));

    assertTrue(isVisible(testElement.$.actionMenu.getDialog()));
  }

  setup(async function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    browserProxy.setNotificationPermissionReview(mockData);
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    pluralStringProxy = new TestPluralStringProxy();
    SettingsPluralStringProxyImpl.setInstance(pluralStringProxy);
    testRoutes = {
      SAFETY_HUB: routes.SAFETY_HUB,
    } as unknown as SettingsRoutes;
    Router.resetInstanceForTesting(new Router(routes));
    await createPage();
    assertEquals(2, getEntries().length);
    metricsBrowserProxy.reset();
  });

  teardown(function() {
    testElement.remove();
  });

  test('Notification Permission strings', async function() {
    const entries = getEntries();

    // Check that the text describing the changed permissions is correct.
    for (let i = 0; i < entries.length; i++) {
      assertEquals(
          mockData[i]!.origin,
          entries[i]!.querySelector(
                         '.site-representation')!.textContent!.trim());

      assertEquals(
          mockData[i]!.notificationInfoString,
          entries[i]!.querySelector('.cr-secondary-text')!.textContent!.trim());
    }
  });

  test('Record Suggestions Count', async function() {
    await createPage();
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleListCountHistogram');
    assertEquals(mockData.length, result);
  });

  /**
   * Tests whether clicking on the block button results in the appropriate
   * browser proxy call and shows the notification toast element.
   */
  test('Dont Allow Click', async function() {
    assertNotification(false);

    // User clicks don't allow.
    const entry = getEntries()[0]!;
    clickButton(entry.querySelector('#mainButton'));
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        entry.querySelector('.site-representation')!.textContent!.trim();
    const origins =
        await browserProxy.whenCalled('blockNotificationPermissionForOrigins');
    assertEquals(origins[0], expectedOrigin);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewBlockedToastLabel',
            expectedOrigin));

    // Ensure the metric for 'Block' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(SafetyCheckNotificationsModuleInteractions.BLOCK, result);
  });

  /**
   * Tests whether clicking on the ignore action via the action menu results in
   * the appropriate browser proxy call, closes the action menu, and shows the
   * notification toast element.
   */
  test('Ignore Click', async function() {
    assertNotification(false);

    // User clicks ignore.
    openActionMenu(0);
    clickButton(testElement.shadowRoot!.querySelector('#ignore'));

    // Ensure the browser proxy call is done.
    const expectedOrigin =
        getEntries()[0]!.querySelector(
                            '.site-representation')!.textContent!.trim();
    const origins =
        await browserProxy.whenCalled('ignoreNotificationPermissionForOrigins');
    assertEquals(origins[0], expectedOrigin);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewIgnoredToastLabel',
            expectedOrigin));
    // Ensure the action menu is closed.
    assertFalse(isVisible(testElement.$.actionMenu.getDialog()));

    // Ensure the metric for 'Ignore' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(SafetyCheckNotificationsModuleInteractions.IGNORE, result);
  });

  /**
   * Tests whether clicking on the reset action via the action menu results in
   * the appropriate browser proxy call, closes the action menu, and shows the
   * notification toast element.
   */
  test('Reset Click', async function() {
    assertNotification(false);

    // User clicks reset.
    openActionMenu(0);
    testElement.$.reset.click();
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        getEntries()[0]!.querySelector(
                            '.site-representation')!.textContent!.trim();
    const origins =
        await browserProxy.whenCalled('resetNotificationPermissionForOrigins');
    assertEquals(origins[0], expectedOrigin);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewResetToastLabel',
            expectedOrigin));
    // Ensure the action menu is closed.
    assertFalse(isVisible(testElement.$.actionMenu.getDialog()));

    // Ensure the metric for 'Reset' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(SafetyCheckNotificationsModuleInteractions.RESET, result);
  });

  /**
   * Tests whether clicking the Undo button after blocking a site correctly
   * resets the site to allow notifications and makes the toast element
   * disappear.
   */
  test('Undo Block Click', async function() {
    // User blocks the site.
    clickButton(getEntries()[0]!.querySelector('#mainButton'));

    await assertUndo('allowNotificationPermissionForOrigins', 0);
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);

    // Ensure the metric for 'Undo Block' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(SafetyCheckNotificationsModuleInteractions.UNDO_BLOCK, result);
  });

  /**
   * Tests whether clicking the Undo button after ignoring notification a site
   * for permission review correctly removes the site from the blocklist
   * and makes the toast element disappear.
   */
  test('Undo Ignore Click', async function() {
    openActionMenu(0);
    // User ignores notifications for the site.
    testElement.$.ignore.click();

    await assertUndo('undoIgnoreNotificationPermissionForOrigins', 0);
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);

    // Ensure the metric for 'Undo Ignore' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckNotificationsModuleInteractions.UNDO_IGNORE, result);
  });

  /**
   * Tests whether clicking the Undo button after resetting notification
   * permissions for a site correctly resets the site to allow notifications
   * and makes the toast element disappear.
   */
  test('Undo Reset Click', async function() {
    openActionMenu(0);
    // User resets permissions for the site.
    testElement.$.reset.click();

    await assertUndo('allowNotificationPermissionForOrigins', 0);
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);

    // Ensure the metric for 'Undo Reset' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(SafetyCheckNotificationsModuleInteractions.UNDO_RESET, result);
  });

  /**
   * Tests whether clicking the Block All button will block notifications for
   * all entries in the list, and whether clicking the Undo button afterwards
   * will allow the notifications for that same list.
   */
  test('Block All Click', async function() {
    testElement.$.blockAllButton.click();
    const origins1 =
        await browserProxy.whenCalled('blockNotificationPermissionForOrigins');
    assertEquals(2, origins1.length);
    assertEquals(
        JSON.stringify(origins1.sort()), JSON.stringify([origin1, origin2]));
    assertNotification(true);
    await assertPluralString(
        'safetyCheckNotificationPermissionReviewBlockAllToastLabel', 2, 2);

    // Ensure the metric for 'Block All' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(SafetyCheckNotificationsModuleInteractions.BLOCK_ALL, result);
    metricsBrowserProxy.reset();

    // Click undo button.
    testElement.$.toastUndoButton.click();
    const origins2 =
        await browserProxy.whenCalled('allowNotificationPermissionForOrigins');
    assertEquals(2, origins2.length);
    assertEquals(
        JSON.stringify(origins2.sort()), JSON.stringify([origin1, origin2]));

    // Ensure the metric for 'Undo Block All' action is recorded.
    const result_undo = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckNotificationsModuleInteractions.UNDO_BLOCK_ALL, result_undo);
  });

  /**
   * Tests whether pressing the ctrl+z key combination correctly undoes the last
   * user action.
   */
  test('Undo Block via Ctrl+Z', async function() {
    assertNotification(false);

    // User clicks don't allow.
    const entry = getEntries()[0]!;
    clickButton(entry.querySelector('#mainButton'));
    // Reset the action captured by clicking the Block button.
    metricsBrowserProxy.reset();

    const expectedOrigin =
        entry.querySelector('.site-representation')!.textContent!.trim();
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

    // Ensure the metric for 'Undo Block' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(SafetyCheckNotificationsModuleInteractions.UNDO_BLOCK, result);
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

    testElement.$.blockAllButton.click();

    const blockedOrigins =
        await browserProxy.whenCalled('blockNotificationPermissionForOrigins');
    assertEquals(blockedOrigins[0], origin1);
    assertNotification(
        true,
        testElement.i18n(
            'safetyCheckNotificationPermissionReviewBlockedToastLabel',
            origin1));
  });

  /**
   * Tests whether header string updated based on the notification permission
   * list size for plural and singular case.
   */
  test('Header String', async function() {
    // Check header string for plural case.
    let entries = getEntries();
    assertEquals(2, entries.length);
    await assertPluralString('safetyHubNotificationPermissionsPrimaryLabel', 2);

    // Check header string for singular case.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, [{
          origin: origin1,
          notificationInfoString: detail1,
        }]);
    await flushTasks();

    entries = getEntries();
    assertEquals(1, entries.length);
    await assertPluralString('safetyHubNotificationPermissionsPrimaryLabel', 1);

    // Check visibility of buttons
    assertTrue(isVisible(testElement.$.blockAllButton));
    assertFalse(isVisible(testElement.$.bulkUndoButton));

    // Check header string for completion case.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();

    const expectedCompletionHeader =
        testElement.i18n('safetyCheckNotificationPermissionReviewDoneLabel');
    assertEquals(expectedCompletionHeader, testElement.$.module.header);
    assertEquals('', testElement.$.module.subheader);

    // Check visibility of buttons
    assertFalse(isVisible(testElement.$.blockAllButton));
    assertTrue(isVisible(testElement.$.bulkUndoButton));
  });

  /**
   * Tests whether pressing the more action button correctly shows the menu and
   * if the navigation happens correctly.
   */
  test('More Actions Button in Header', async function() {
    assertFalse(isVisible(testElement.$.headerActionMenu.getDialog()));

    // The action menu should be visible after clicking the button.
    clickButton(testElement.shadowRoot!.querySelector('#moreActionButton'));
    assertTrue(isVisible(testElement.$.headerActionMenu.getDialog()));

    clickButton(testElement.shadowRoot!.querySelector('#goToSettings'));
    // The action menu should be gone after clicking the button.
    assertFalse(isVisible(testElement.$.headerActionMenu.getDialog()));
    // Ensure the site settings page is shown.
    assertEquals(
        routes.SITE_SETTINGS_NOTIFICATIONS,
        Router.getInstance().getCurrentRoute());
    // Ensure the metric for 'Go To Settings' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckNotificationsModuleInteractions.GO_TO_SETTINGS, result);
  });
});
