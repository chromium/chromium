// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSafetyHubNotificationPermissionsModuleElement} from 'chrome://settings/lazy_load.js';
import {SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, resetRouterForTesting, Router, routes, SafetyCheckNotificationsModuleInteractions as Interactions, SettingsPluralStringProxyImpl} from 'chrome://settings/settings.js';
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

  /**
   * Asserts the Undo toast is shown with a correct origin-containing string.
   * @param stringId The id to retrieve the correct toast string. Provided only
   *     if toastShouldBeOpen is true.
   * @param index The index of the element whose origin is in the toast string.
   *     Provided only if toastShouldBeOpen is true. The default value is 0.
   */
  function assertUndoToast(
      toastShouldBeOpen: boolean, stringId?: string, index?: number): void {
    const undoToast = testElement.$.undoToast;
    if (!toastShouldBeOpen) {
      assertFalse(undoToast.open);
      return;
    }
    assertTrue(undoToast.open);
    if (stringId) {
      if (!index) {
        index = 0;
      }
      const expectedOrigin = mockData[index]!.origin;
      const toastText = testElement.i18n(stringId, expectedOrigin);
      assertEquals(
          toastText, testElement.$.undoNotification.textContent!.trim());
    }
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
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
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
   * @param index The index of the child element (which site) to open the action
   *     menu for. The default value is 0.
   */
  function openActionMenu(index?: number) {
    if (!index) {
      index = 0;
    }
    assertFalse(isVisible(testElement.$.actionMenu.getDialog()));

    clickButton(getEntries()[index]!.querySelector('#moreActionButton'));

    assertTrue(isVisible(testElement.$.actionMenu.getDialog()));
  }

  /**
   * Sets up the notification permissions review list with a single entry.
   * @param index The index of the child element to include in the list. The
   *     default value is 0.
   */
  async function setupSingleEntry(index?: number) {
    if (!index) {
      index = 0;
    }
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, [{
          origin: mockData[index]!.origin,
          notificationInfoString: mockData[index]!.notificationInfoString,
        }]);
    await flushTasks();

    assertEquals(1, getEntries().length);
  }

  /**
   * Asserts a browser proxy call with a message is done for the origin.
   * @param index The index of the child element used for the browser call. The
   *     default value is 0.
   */
  async function assertBrowserCall(message: string, index?: number) {
    if (!index) {
      index = 0;
    }
    const [result] = await browserProxy.whenCalled(message);
    assertEquals(mockData[index]!.origin, result);
  }

  /**
   * Asserts a browser proxy call with a message is done for multiple origins.
   * @param maxIndex The maximum index of the child elements used for the
   *     browser call. The default value is 1.
   */
  async function assertBrowserCallPlural(message: string, maxIndex?: number) {
    if (!maxIndex) {
      maxIndex = 1;
    }

    const origins = mockData.map(data => data.origin);
    const result = await browserProxy.whenCalled(message);
    assertEquals(maxIndex + 1, result.length);
    assertEquals(
        JSON.stringify(origins.sort()),
        JSON.stringify(origins.slice(0, maxIndex + 1)));
  }

  /**
   * Asserts the header string equals to a correct origin-containing string.
   * @param index The index of the element whose origin is in the
   * header. The default value is 0.
   */
  async function assertCompletionHeaderString(strId: string, index?: number) {
    if (!index) {
      index = 0;
    }
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();

    const expectedString = testElement.i18n(strId, mockData[index]!.origin);
    const headerString = testElement.$.module.header;
    assertEquals(expectedString, headerString);
  }

  /**
   * Asserts a correct action was recorded into
   * recordSafetyHubNotificationPermissionsModuleInteractionsHistogram
   * histogram.
   */
  async function assertInteractionMetricRecorded(expectedAction: Interactions) {
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubNotificationPermissionsModuleInteractionsHistogram');
    assertEquals(expectedAction, result);
    metricsBrowserProxy.reset();
  }

  setup(async function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    browserProxy.setNotificationPermissionReview(mockData);
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    pluralStringProxy = new TestPluralStringProxy();
    SettingsPluralStringProxyImpl.setInstance(pluralStringProxy);
    resetRouterForTesting();
    await createPage();
    assertEquals(2, getEntries().length);
    metricsBrowserProxy.reset();
    assertUndoToast(false);
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
  test('Block Click', async function() {
    // User clicks don't allow.
    const entry = getEntries()[0]!;
    clickButton(entry.querySelector('#mainButton'));

    // Ensure the correctness of the browser proxy call and the undo toast.
    await assertBrowserCall('blockNotificationPermissionForOrigins');
    assertUndoToast(
        true, 'safetyCheckNotificationPermissionReviewBlockedToastLabel');

    await browserProxy.whenCalled('recordSafetyHubInteraction');

    // Ensure the metric for 'Block' action is recorded.
    await assertInteractionMetricRecorded(Interactions.BLOCK);
  });

  /**
   * Tests whether clicking on the ignore action via the action menu results in
   * the appropriate browser proxy call, closes the action menu, and shows the
   * notification toast element.
   */
  test('Ignore Click', async function() {
    // User clicks ignore.
    openActionMenu();
    clickButton(testElement.shadowRoot!.querySelector('#ignore'));

    // Ensure the browser proxy call is done, undo toast with a correct text is
    // shown and action menu is closed.
    await assertBrowserCall('ignoreNotificationPermissionForOrigins');
    assertUndoToast(
        true, 'safetyCheckNotificationPermissionReviewIgnoredToastLabel');
    assertFalse(isVisible(testElement.$.actionMenu.getDialog()));

    await browserProxy.whenCalled('recordSafetyHubInteraction');

    // Ensure the metric for 'Ignore' action is recorded.
    await assertInteractionMetricRecorded(Interactions.IGNORE);
  });

  /**
   * Tests whether clicking on the reset action via the action menu results in
   * the appropriate browser proxy call, closes the action menu, and shows the
   * notification toast element.
   */
  test('Reset Click', async function() {
    // User clicks reset.
    openActionMenu();
    testElement.$.reset.click();

    // Ensure the browser proxy call is done, undo toast with a correct text is
    // shown and action menu is closed.
    await assertBrowserCall('resetNotificationPermissionForOrigins');
    assertUndoToast(
        true, 'safetyCheckNotificationPermissionReviewResetToastLabel');
    assertFalse(isVisible(testElement.$.actionMenu.getDialog()));

    await browserProxy.whenCalled('recordSafetyHubInteraction');

    // Ensure the metric for 'Reset' action is recorded.
    await assertInteractionMetricRecorded(Interactions.RESET);
  });

  /**
   * Tests whether clicking the Undo button after blocking a site correctly
   * resets the site to allow notifications and makes the toast element
   * disappear.
   */
  test('Undo Block Click', async function() {
    // User blocks the site and then clicks on undo toast.
    clickButton(getEntries()[0]!.querySelector('#mainButton'));
    metricsBrowserProxy.reset();
    testElement.$.toastUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('allowNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Undo Block' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_BLOCK);
  });

  /**
   * Tests whether clicking the Undo button after ignoring notification a site
   * for permission review correctly removes the site from the blocklist
   * and makes the toast element disappear.
   */
  test('Undo Ignore Click', async function() {
    // User ignores notifications for the site and then clicks on undo toast.
    openActionMenu();
    testElement.$.ignore.click();
    metricsBrowserProxy.reset();
    testElement.$.toastUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('undoIgnoreNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Undo Ignore' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_IGNORE);
  });

  /**
   * Tests whether clicking the Undo button after resetting notification
   * permissions for a site correctly resets the site to allow notifications
   * and makes the toast element disappear.
   */
  test('Undo Reset Click', async function() {
    // User resets permissions for the site and then clicks on undo toast.
    openActionMenu();
    testElement.$.reset.click();
    metricsBrowserProxy.reset();
    testElement.$.toastUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('allowNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Undo Reset' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_RESET);
  });

  /**
   * Tests whether
   * - clicking the Block All button will block notifications for all entries in
   * the list without showing an undo toast;
   * - clicking Bulk Undo afterwards will allow the same list of notifications
   * without showing an undo toast.
   */
  test('Block All Click and Bulk Undo', async function() {
    // User clicks Block All.
    testElement.$.blockAllButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCallPlural('blockNotificationPermissionForOrigins');
    assertUndoToast(false);

    await browserProxy.whenCalled('recordSafetyHubInteraction');

    // UI should be in a completion state.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();

    // Check visibility of buttons
    assertFalse(isVisible(testElement.$.blockAllButton));
    assertTrue(isVisible(testElement.$.bulkUndoButton));

    // Ensure the metric for 'Block All' action is recorded.
    await assertInteractionMetricRecorded(Interactions.BLOCK_ALL);

    // User clicks Bulk Undo.
    metricsBrowserProxy.reset();
    testElement.$.bulkUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCallPlural('allowNotificationPermissionForOrigins');
    assertUndoToast(false);

    // UI should be back to its initial state.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);
    await flushTasks();

    // Check visibility of buttons
    assertTrue(isVisible(testElement.$.blockAllButton));
    assertFalse(isVisible(testElement.$.bulkUndoButton));

    // Ensure the metric for 'Undo Block All' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_BLOCK_ALL);
  });

  /**
   * Tests whether pressing the ctrl+z key combination correctly undoes the last
   * user action.
   */
  test('Undo Block via Ctrl+Z', async function() {
    // User clicks don't allow.
    const entry = getEntries()[0]!;
    clickButton(entry.querySelector('#mainButton'));
    metricsBrowserProxy.reset();

    // User presses Ctrl+Z to undo.
    keyDownOn(document.documentElement, 0, isMac ? 'meta' : 'ctrl', 'z');

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('allowNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Undo Block' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_BLOCK);
  });

  /**
   * Tests whether:
   * - clicking on the Block button of the single site in review results in the
   * appropriate browser proxy call without showing an undo toast;
   * - clicking Undo afterwards resets the site to allow notifications without
   * showing an undo toast.
   */
  test('Block Click and Undo - single entry', async function() {
    await setupSingleEntry();

    // User clicks Block.
    const entry = getEntries()[0]!;
    clickButton(entry.querySelector('#mainButton'));

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('blockNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Block' action is recorded.
    await assertInteractionMetricRecorded(Interactions.BLOCK);

    // User clicks Undo.
    metricsBrowserProxy.reset();
    testElement.$.bulkUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('allowNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Undo Block' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_BLOCK);
  });

  /**
   * Tests whether:
   * - clicking on the Ignore action of the single site in review results in the
   * appropriate browser proxy call without showing an undo toast;
   * - clicking Undo afterwards correctly removes the site from the blocklist.
   */
  test('Ignore Click and Undo - single entry', async function() {
    await setupSingleEntry();

    // User clicks ignore.
    openActionMenu();
    clickButton(testElement.shadowRoot!.querySelector('#ignore'));

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('ignoreNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Ignore' action is recorded.
    await assertInteractionMetricRecorded(Interactions.IGNORE);

    // User clicks Undo.
    metricsBrowserProxy.reset();
    testElement.$.bulkUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('undoIgnoreNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Undo Ignore' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_IGNORE);
  });

  /**
   * Tests whether:
   * - clicking on the Reset action of the single site in review results in the
   * appropriate browser proxy call without showing an undo toast;
   * - clicking Undo afterwards correctly resets the site to allow
   * notifications.
   */
  test('Reset Click and Undo - single entry', async function() {
    await setupSingleEntry();

    // User clicks reset.
    openActionMenu();
    testElement.$.reset.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('resetNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Reset' action is recorded.
    await assertInteractionMetricRecorded(Interactions.RESET);

    // User clicks Undo.
    metricsBrowserProxy.reset();
    testElement.$.bulkUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('allowNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Undo Reset' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_RESET);
  });

  /**
   * Tests whether:
   * - clicking on the Block All button while having a single site in review
   * results in the appropriate browser proxy call without showing an undo
   * toast.
   * - clicking Bulk Undo afterwards correctly resets the site to allow
   * notifications.
   */
  test('Block All Click and Bulk Undo - single entry', async function() {
    await setupSingleEntry();

    // Click 'Block all' button.
    testElement.$.blockAllButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('blockNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Block All' action is recorded.
    await assertInteractionMetricRecorded(Interactions.BLOCK_ALL);

    // User clicks Bulk Undo.
    metricsBrowserProxy.reset();
    testElement.$.bulkUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertBrowserCall('allowNotificationPermissionForOrigins');
    assertUndoToast(false);

    // Ensure the metric for 'Undo Block All' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_BLOCK_ALL);
  });

  /**
   * Tests whether header string is updated based on the notification permission
   * list size for plural, singular and completion cases.
   */
  test('Header String', async function() {
    // Check header string for plural case.
    let entries = getEntries();
    assertEquals(2, entries.length);
    await assertPluralString('safetyHubNotificationPermissionsPrimaryLabel', 2);

    // Check header string for singular case.
    await setupSingleEntry();
    entries = getEntries();
    assertEquals(1, entries.length);
    await assertPluralString('safetyHubNotificationPermissionsPrimaryLabel', 1);

    // Check the header string for a completion case after Block All action
    // (multiple entries in review).
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, mockData);
    await flushTasks();
    testElement.$.blockAllButton.click();
    await assertPluralString(
        'safetyCheckNotificationPermissionReviewBlockAllToastLabel', 2, 2);

    // Check the header string for a completion case after Block All action
    // (single entry in review).
    await setupSingleEntry();
    testElement.$.blockAllButton.click();
    await assertPluralString(
        'safetyCheckNotificationPermissionReviewBlockAllToastLabel', 1, 2);

    // Check the header string for a completion case after Block action.
    await setupSingleEntry();
    clickButton(getEntries()[0]!.querySelector('#mainButton'));
    await assertCompletionHeaderString(
        'safetyCheckNotificationPermissionReviewBlockedToastLabel');
    testElement.$.bulkUndoButton.click();

    // Check the header string for a completion case after Ignore action.
    openActionMenu();
    clickButton(testElement.shadowRoot!.querySelector('#ignore'));
    await assertCompletionHeaderString(
        'safetyCheckNotificationPermissionReviewIgnoredToastLabel');
    testElement.$.bulkUndoButton.click();

    // Check the header string for a completion case after Reset action.
    openActionMenu();
    clickButton(testElement.shadowRoot!.querySelector('#reset'));
    await assertCompletionHeaderString(
        'safetyCheckNotificationPermissionReviewResetToastLabel');
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
    await assertInteractionMetricRecorded(Interactions.GO_TO_SETTINGS);
  });

  /**
   * Tests that previously shown undo tast does not affect the next action's
   * undo toast.
   */
  test('Undo Toast Behavior', function() {
    mockData.push({
      origin: 'https://www.example3.com:443',
      notificationInfoString: 'About 3 notifications a day',
    });
    assertEquals(3, mockData.length);

    // Click Always Allow for the first item in review. This triggers an undo
    // toast to appear.
    openActionMenu();
    clickButton(testElement.shadowRoot!.querySelector('#ignore'));
    assertUndoToast(
        true, 'safetyCheckNotificationPermissionReviewIgnoredToastLabel');

    // Click Don't Allow for the second item. This hides the existing undo toast
    // and shows a new one.
    clickButton(getEntries()[1]!.querySelector('#mainButton'));
    assertUndoToast(
        true, 'safetyCheckNotificationPermissionReviewBlockedToastLabel', 1);

    // Click BlockAll that hides the existing toast and doesn't show a new one.
    testElement.$.blockAllButton.click();
    assertUndoToast(false);
  });
});
