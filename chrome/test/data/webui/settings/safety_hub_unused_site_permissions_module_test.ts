// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {SettingsSafetyHubUnusedSitePermissionsModuleElement, UnusedSitePermissions} from 'chrome://settings/lazy_load.js';
import {ContentSettingsTypes, SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, resetRouterForTesting, Router, routes, SafetyCheckUnusedSitePermissionsModuleInteractions as Interactions, SettingsPluralStringProxyImpl} from 'chrome://settings/settings.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
// clang-format on

suite('CrSettingsSafetyHubUnusedSitePermissionsTest', function() {
  let browserProxy: TestSafetyHubBrowserProxy;
  let pluralString: TestPluralStringProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  let testElement: SettingsSafetyHubUnusedSitePermissionsModuleElement;

  const permissions = [
    ContentSettingsTypes.GEOLOCATION,
    ContentSettingsTypes.MIC,
    ContentSettingsTypes.CAMERA,
    ContentSettingsTypes.COOKIES,
    ContentSettingsTypes.NOTIFICATIONS,
  ];

  const mockData = [1, 2, 3, 4, 5].map(
      i => ({
        origin: `https://www.example${i}.com:443`,
        permissions: permissions.slice(0, i),
        expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
      }));

  function assertEqualsMockData(
      siteList: UnusedSitePermissions[], mockDataLength: number) {
    // |siteList| coming from WebUI may have the additional property |detail|,
    // so assertDeepEquals doesn't work to compare it with |mockData|. We care
    // about origins and associated permissions being equal.
    assertEquals(mockDataLength, siteList.length);
    for (const [i, site] of siteList.entries()) {
      assertEquals(mockData[i]!.origin, site!.origin);
      assertDeepEquals(site!.permissions, mockData[i]!.permissions);
    }
  }

  function assertInitialUi() {
    const expectedSiteCount = mockData.length;
    assertEquals(getSiteList().length, expectedSiteCount);
    assertUndoToast(false);
  }

  /**
   * Asserts the Undo toast is shown with a correct origin-containing string.
   * @param stringId The id to retrieve the correct toast string. Provided only
   *     if shouldBeOpen is true.
   * @param index The index of the element whose origin is in the toast string.
   *     Provided only if shouldBeOpen is true. The default value is 0.
   */
  function assertUndoToast(
      shouldBeOpen: boolean, stringId?: string, index?: number) {
    const undoToast = testElement.$.undoToast;
    if (!shouldBeOpen) {
      assertFalse(undoToast.open);
      return;
    }
    assertTrue(undoToast.open);

    if (stringId) {
      if (!index) {
        index = 0;
      }
      const expectedText = testElement.i18n(stringId, mockData[index]!.origin);
      const actualText = undoToast.querySelector('div')!.textContent!.trim();
      assertEquals(expectedText, actualText);
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
    await pluralString.whenCalled('getPluralString');
    const params = pluralString.getArgs('getPluralString')[index];
    await flushTasks();
    assertEquals(messageName, params.messageName);
    assertEquals(itemCount, params.itemCount);
    pluralString.resetResolver('getPluralString');
  }

  function getSiteList(): NodeListOf<HTMLElement> {
    return testElement.$.module.shadowRoot!.querySelectorAll('.site-entry');
  }

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement =
        document.createElement('settings-safety-hub-unused-site-permissions');
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
    document.body.appendChild(testElement);
    // Wait until the element has asked for the list of revoked permissions
    // that will be shown for review.
    await browserProxy.whenCalled('getRevokedUnusedSitePermissionsList');
    await flushTasks();
  }

  /**
   * Sets up the unused site permissions list with a single entry.
   * @param index The index of the child element to include in the list. The
   *     default value is 0.
   */
  async function setupSingleEntry(index?: number) {
    if (!index) {
      index = 0;
    }
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        mockData.slice(index, 1));
    await flushTasks();
  }

  /**
   * Asserts a correct browser call is done for the given origin.
   * @param index The index of the child element used for the browser call. The
   *     default value is 0.
   */
  async function assertAllowAgain(index?: number) {
    if (!index) {
      index = 0;
    }
    const [origin] =
        await browserProxy.whenCalled('allowPermissionsAgainForUnusedSite');
    assertEquals(mockData[index]!.origin, origin);
  }

  /**
   * Asserts a correct browser call is done for the given origin.
   * @param index The index of the element for whose origin the call is done.
   *     The default value is 0.
   */
  async function assertUndoAllowAgain(index?: number) {
    if (!index) {
      index = 0;
    }
    const [unusedSitePermissions] =
        await browserProxy.whenCalled('undoAllowPermissionsAgainForUnusedSite');
    assertEquals(mockData[index]!.origin, unusedSitePermissions.origin);
    assertDeepEquals(
        unusedSitePermissions.permissions, mockData[index]!.permissions);
    browserProxy.resetResolver('undoAllowPermissionsAgainForUnusedSite');
  }

  /**
   * Asserts a correct action was recorded into
   * recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram histogram.
   */
  async function assertInteractionMetricRecorded(
      action: Interactions, isAbusiveNotification?: boolean) {
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(action, result);

    if (!isAbusiveNotification) {
      isAbusiveNotification = false;
    }
    if (isAbusiveNotification) {
      const resultAbusiveNotification = await metricsBrowserProxy.whenCalled(
          'recordSafetyHubAbusiveNotificationPermissionRevocationInteractionsHistogram');
      assertEquals(action, resultAbusiveNotification);
    }
    metricsBrowserProxy.reset();
  }

  setup(async function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    browserProxy.setUnusedSitePermissions(mockData);
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    pluralString = new TestPluralStringProxy();
    SettingsPluralStringProxyImpl.setInstance(pluralString);
    resetRouterForTesting();
    await createPage();
    metricsBrowserProxy.reset();
    assertInitialUi();
  });

  test('Abusive and Unused Site Permission strings', function() {
    const siteList = getSiteList();
    assertEquals(5, siteList.length);

    // Check that the text describing the permissions is correct.
    assertEquals(
        mockData[0]!.origin,
        getSiteList()[0]!.querySelector(
                             '.site-representation')!.textContent!.trim());
    assertTrue(!!getSiteList()[0]!.querySelector(
                                      '.cr-secondary-text')!.textContent!.trim()
                     .match(
                         'You haven\'t visited recently. ' +
                         'Chrome|Chromium removed location'));

    assertEquals(
        mockData[1]!.origin,
        siteList[1]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertTrue(
        !!siteList[1]!.querySelector('.cr-secondary-text')!.textContent!.trim()
              .match(
                  'You haven\'t visited recently. ' +
                  'Chrome|Chromium removed location, microphone'));

    assertEquals(
        mockData[2]!.origin,
        siteList[2]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertTrue(
        !!siteList[2]!.querySelector('.cr-secondary-text')!.textContent!.trim()
              .match(
                  'You haven\'t visited recently. ' +
                  'Chrome|Chromium removed location, microphone, camera'));

    assertEquals(
        mockData[3]!.origin,
        siteList[3]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertTrue(
        !!siteList[3]!.querySelector('.cr-secondary-text')!.textContent!.trim()
              .match(
                  'You haven\'t visited recently. ' +
                  'Chrome|Chromium removed location, microphone, and 2 more'));

    assertEquals(
        mockData[4]!.origin,
        siteList[4]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertTrue(
        !!siteList[4]!.querySelector('.cr-secondary-text')!.textContent!.trim()
              .match('Dangerous site. Chrome|Chromium removed notifications.'));
  });

  test('Record Suggestions Count', async function() {
    await createPage();
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubUnusedSitePermissionsModuleListCountHistogram');
    assertEquals(getSiteList().length, result);
  });

  test('Allow Again Click', async function() {
    // User clicks Allow Again.
    getSiteList()[0]!.querySelector('cr-icon-button')!.click();

    // Ensure the correctness of the browser proxy call and the undo toast.
    await assertAllowAgain();
    assertUndoToast(true, 'safetyCheckUnusedSitePermissionsToastLabel');

    await browserProxy.whenCalled('recordSafetyHubInteraction');

    // Ensure the metric for 'Allow Again' action is recorded.
    await assertInteractionMetricRecorded(Interactions.ALLOW_AGAIN);
  });

  test('Allow Again Click Abusive Notification Site', async function() {
    // User clicks Allow Again.
    getSiteList()[4]!.querySelector('cr-icon-button')!.click();

    // Ensure the correctness of the browser proxy call and the undo toast.
    await assertAllowAgain(4);
    assertUndoToast(true, 'safetyCheckUnusedSitePermissionsToastLabel', 4);

    await browserProxy.whenCalled('recordSafetyHubInteraction');

    // Ensure the metric for 'Allow Again' action is recorded.
    await assertInteractionMetricRecorded(Interactions.ALLOW_AGAIN, true);
  });

  test('Undo Allow Again', async function() {
    for (const [i, site] of getSiteList().entries()) {
      // User clicks Allow Again and then Undo.
      site!.querySelector('cr-icon-button')!.click();
      metricsBrowserProxy.reset();
      testElement.$.toastUndoButton.click();

      // Ensure the browser proxy call is done and no undo toast is shown.
      await assertUndoAllowAgain(i);
      assertUndoToast(false);

      // UI should be back to its initial state.
      webUIListenerCallback(
          SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
      flush();

      // Ensure the metric for 'Undo Allow Again' action is recorded. The
      // last site at index 4 includes revoked notifications, so the abusive
      // notification histogram should also be recorded.
      if (i === 4) {
        await assertInteractionMetricRecorded(
            Interactions.UNDO_ALLOW_AGAIN, true);
      } else {
        await assertInteractionMetricRecorded(Interactions.UNDO_ALLOW_AGAIN);
      }

      assertInitialUi();
    }
  });

  test('Undo Allow Again via Ctrl+Z', async function() {
    for (const [i, site] of getSiteList().entries()) {
      // User clicks Allow Again and then Ctrl+Z.
      assertTrue(!!site);
      const allowAgainButton = site.querySelector('cr-icon-button');
      assertTrue(!!allowAgainButton);
      allowAgainButton.click();
      metricsBrowserProxy.reset();
      keyDownOn(document.documentElement, 0, isMac ? 'meta' : 'ctrl', 'z');

      // Ensure the browser proxy call is done and no undo toast is shown.
      await assertUndoAllowAgain(i);

      // UI should be back to its initial state.
      webUIListenerCallback(
          SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
      flush();

      // Ensure the metric for 'Undo Allow Again' action is recorded. The
      // last site at index 4 includes revoked notifications, so the abusive
      // notification histogram should also be recorded.
      if (i === 4) {
        await assertInteractionMetricRecorded(
            Interactions.UNDO_ALLOW_AGAIN, true);
      } else {
        await assertInteractionMetricRecorded(Interactions.UNDO_ALLOW_AGAIN);
      }

      assertInitialUi();
    }
  });

  test('Got It Click', async function() {
    // User clicks Got It.
    testElement.$.gotItButton.click();
    await flushTasks();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await browserProxy.whenCalled(
        'acknowledgeRevokedUnusedSitePermissionsList');
    assertUndoToast(false);
    await browserProxy.whenCalled('recordSafetyHubInteraction');

    // UI should be in a completion state.
    webUIListenerCallback(SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();

    // Check visibility of buttons
    assertFalse(isVisible(testElement.$.gotItButton));
    assertTrue(isVisible(testElement.$.bulkUndoButton));

    // Ensure the metric for 'Acknowledge All' action is recorded.
    await assertInteractionMetricRecorded(Interactions.ACKNOWLEDGE_ALL, true);
  });

  test('Undo Got It', async function() {
    // User clicks Got It and then Bulk Undo.
    testElement.$.gotItButton.click();
    metricsBrowserProxy.reset();
    testElement.$.bulkUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    const [unusedSitePermissionsList] = await browserProxy.whenCalled(
        'undoAcknowledgeRevokedUnusedSitePermissionsList');
    assertEqualsMockData(unusedSitePermissionsList, mockData.length);
    assertUndoToast(false);

    // UI should be back to its initial state.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
    assertInitialUi();

    // Check visibility of buttons
    assertTrue(isVisible(testElement.$.gotItButton));
    assertFalse(isVisible(testElement.$.bulkUndoButton));

    // Ensure the metric for 'Undo Acknowledge All' action is recorded.
    await assertInteractionMetricRecorded(
        Interactions.UNDO_ACKNOWLEDGE_ALL, true);
  });

  test('Allow Again Click and Undo - single entry', async function() {
    await setupSingleEntry();

    // User clicks Allow Again.
    getSiteList()[0]!.querySelector('cr-icon-button')!.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertAllowAgain();
    assertUndoToast(false);

    // Ensure the metric for 'Allow Again' action is recorded.
    await assertInteractionMetricRecorded(Interactions.ALLOW_AGAIN);

    // User clicks Undo.
    metricsBrowserProxy.reset();
    testElement.$.toastUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await assertUndoAllowAgain();
    assertUndoToast(false);

    // Ensure the metric for 'Undo Allow Again' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_ALLOW_AGAIN);
  });

  test('Got It Click and Undo - single entry', async function() {
    await setupSingleEntry();

    // User clicks Got It.
    testElement.$.gotItButton.click();
    await flushTasks();

    // Ensure the browser proxy call is done and no undo toast is shown.
    await browserProxy.whenCalled(
        'acknowledgeRevokedUnusedSitePermissionsList');
    assertUndoToast(false);

    // Ensure the metric for 'Got It' action is recorded.
    await assertInteractionMetricRecorded(Interactions.ACKNOWLEDGE_ALL);

    // User clicks Bulk Undo.
    metricsBrowserProxy.reset();
    testElement.$.bulkUndoButton.click();

    // Ensure the browser proxy call is done and no undo toast is shown.
    const [unusedSitePermissionsList] = await browserProxy.whenCalled(
        'undoAcknowledgeRevokedUnusedSitePermissionsList');
    assertEqualsMockData(unusedSitePermissionsList, 1);
    assertUndoToast(false);

    // Ensure the metric for 'Undo Acknowledge All' action is recorded.
    await assertInteractionMetricRecorded(Interactions.UNDO_ACKNOWLEDGE_ALL);
  });

  test('Header Strings', async function() {
    // Check header string for plural case.
    let entries = getSiteList();
    assertEquals(5, entries.length);
    await assertPluralString('safetyCheckUnusedSitePermissionsPrimaryLabel', 5);

    // Check header string for singular case.
    const oneElementMockData = mockData.slice(0, 1);
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, oneElementMockData);
    await flushTasks();

    entries = getSiteList();
    assertEquals(1, entries.length);
    await assertPluralString('safetyCheckUnusedSitePermissionsPrimaryLabel', 1);

    // Check the header string for a completion case after Got It action
    // (single entry in review).
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData.slice(0, 1));
    await flushTasks();
    testElement.$.gotItButton.click();
    await assertPluralString(
        'safetyCheckUnusedSitePermissionsToastBulkLabel', 1, 2);

    // Check the header string for a completion case after Got It action
    // (multiple entries in review).
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
    await flushTasks();
    testElement.$.gotItButton.click();
    await assertPluralString(
        'safetyCheckUnusedSitePermissionsToastBulkLabel', 5, 2);

    // Check the header string for a completion case after Allow Again action.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData.slice(0, 1));
    await flushTasks();
    getSiteList()[0]!.querySelector('cr-icon-button')!.click();
    webUIListenerCallback(SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();
    const expectedHeaderString = testElement.i18n(
        'safetyCheckUnusedSitePermissionsToastLabel', mockData[0]!.origin);
    assertEquals(expectedHeaderString, testElement.$.module.header);
  });

  test('Subheader Strings', async function() {
    // Check header string for plural case.
    let entries = getSiteList();
    assertEquals(5, entries.length);
    await assertPluralString('safetyHubRevokedPermissionsSecondaryLabel', 5, 1);

    // Check header string for singular case.
    const oneElementMockData = mockData.slice(0, 1);
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, oneElementMockData);
    await flushTasks();

    entries = getSiteList();
    assertEquals(1, entries.length);
    await assertPluralString('safetyHubRevokedPermissionsSecondaryLabel', 1, 1);
  });

  test('More Actions Button in Header', async function() {
    assertFalse(isVisible(testElement.$.headerActionMenu.getDialog()));

    // The action menu should be visible after clicking the button.
    testElement.$.moreActionButton.click();
    assertTrue(isVisible(testElement.$.headerActionMenu.getDialog()));

    testElement.$.goToSettings.click();
    // The action menu should be gone after clicking the button.
    assertFalse(isVisible(testElement.$.headerActionMenu.getDialog()));
    // Ensure the site settings page is shown.
    assertEquals(routes.SITE_SETTINGS, Router.getInstance().getCurrentRoute());

    // Ensure the metric for 'Go To Settings' action is recorded.
    await assertInteractionMetricRecorded(Interactions.GO_TO_SETTINGS, true);
  });

  /**
   * Tests that previously shown undo tast does not affect the next action's
   * undo toast.
   */
  test('Undo toast behaviour', async function() {
    // Click Allow Again for the first item in review to trigger an undo toast
    // to appear.
    getSiteList()[0]!.querySelector('cr-icon-button')!.click();
    assertUndoToast(true, 'safetyCheckUnusedSitePermissionsToastLabel', 0);

    // Click Allow Again for the second item. This hides the existing toast and
    // shows a new one.
    getSiteList()[1]!.querySelector('cr-icon-button')!.click();
    assertUndoToast(true, 'safetyCheckUnusedSitePermissionsToastLabel', 1);

    // Click Got It which hides the existing toast and does not show a new one.
    testElement.$.gotItButton.click();
    await flushTasks();
    assertUndoToast(false);
  });
});

// TODO(crbug.com/328773301): Remove after
// SafetyHubAbusiveNotificationRevocationDisabled is launched.
suite('SafetyHubAbusiveNotificationRevocationDisabled', function() {
  let browserProxy: TestSafetyHubBrowserProxy;
  let pluralString: TestPluralStringProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  let testElement: SettingsSafetyHubUnusedSitePermissionsModuleElement;

  const permissions = [
    ContentSettingsTypes.GEOLOCATION,
    ContentSettingsTypes.MIC,
    ContentSettingsTypes.CAMERA,
    ContentSettingsTypes.NOTIFICATIONS,
  ];

  const mockData = [1, 2, 3, 4].map(
      i => ({
        origin: `https://www.example${i}.com:443`,
        permissions: permissions.slice(0, i),
        expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
      }));

  function assertInitialUi() {
    const expectedSiteCount = mockData.length;
    assertEquals(getSiteList().length, expectedSiteCount);
    assertUndoToast(false);
  }

  /**
   * Asserts the Undo toast is shown with a correct origin-containing string.
   * @param stringId The id to retrieve the correct toast string. Provided only
   *     if shouldBeOpen is true.
   * @param index The index of the element whose origin is in the toast string.
   *     Provided only if shouldBeOpen is true. The default value is 0.
   */
  function assertUndoToast(
      shouldBeOpen: boolean, stringId?: string, index?: number) {
    const undoToast = testElement.$.undoToast;
    if (!shouldBeOpen) {
      assertFalse(undoToast.open);
      return;
    }
    assertTrue(undoToast.open);

    if (stringId) {
      if (!index) {
        index = 0;
      }
      const expectedText = testElement.i18n(stringId, mockData[index]!.origin);
      const actualText = undoToast.querySelector('div')!.textContent!.trim();
      assertEquals(expectedText, actualText);
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
    await pluralString.whenCalled('getPluralString');
    const params = pluralString.getArgs('getPluralString')[index];
    await flushTasks();
    assertEquals(messageName, params.messageName);
    assertEquals(itemCount, params.itemCount);
    pluralString.resetResolver('getPluralString');
  }

  function getSiteList(): NodeListOf<HTMLElement> {
    return testElement.$.module.shadowRoot!.querySelectorAll('.site-entry');
  }

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement =
        document.createElement('settings-safety-hub-unused-site-permissions');
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
    document.body.appendChild(testElement);
    // Wait until the element has asked for the list of revoked permissions
    // that will be shown for review.
    await browserProxy.whenCalled('getRevokedUnusedSitePermissionsList');
    await flushTasks();
  }

  setup(async function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    browserProxy.setUnusedSitePermissions(mockData);
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    pluralString = new TestPluralStringProxy();
    SettingsPluralStringProxyImpl.setInstance(pluralString);
    loadTimeData.overrideValues({
      safetyHubAbusiveNotificationRevocationEnabled: false,
    });
    resetRouterForTesting();
    await createPage();
    metricsBrowserProxy.reset();
    assertInitialUi();
  });

  test('Subheader Strings', async function() {
    // Check header string for plural case.
    let entries = getSiteList();
    assertEquals(4, entries.length);
    await assertPluralString(
        'safetyCheckUnusedSitePermissionsSecondaryLabel', 4, 1);

    // Check header string for singular case.
    const oneElementMockData = mockData.slice(0, 1);
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, oneElementMockData);
    await flushTasks();

    entries = getSiteList();
    assertEquals(1, entries.length);
    await assertPluralString(
        'safetyCheckUnusedSitePermissionsSecondaryLabel', 1, 1);
  });

  test('Unused Site Permission strings', function() {
    const siteList = getSiteList();
    assertEquals(4, siteList.length);

    // Check that the text describing the permissions is correct.
    assertEquals(
        mockData[0]!.origin,
        getSiteList()[0]!.querySelector(
                             '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location',
        getSiteList()[0]!.querySelector(
                             '.cr-secondary-text')!.textContent!.trim());

    assertEquals(
        mockData[1]!.origin,
        siteList[1]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone',
        siteList[1]!.querySelector('.cr-secondary-text')!.textContent!.trim());

    assertEquals(
        mockData[2]!.origin,
        siteList[2]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone, camera',
        siteList[2]!.querySelector('.cr-secondary-text')!.textContent!.trim());

    assertEquals(
        mockData[3]!.origin,
        siteList[3]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone, and 2 more',
        siteList[3]!.querySelector('.cr-secondary-text')!.textContent!.trim());
  });
});
