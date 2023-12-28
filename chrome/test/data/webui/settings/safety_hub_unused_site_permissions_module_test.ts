// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {ContentSettingsTypes, SettingsSafetyHubUnusedSitePermissionsModuleElement, SafetyHubBrowserProxyImpl, SafetyHubEvent, UnusedSitePermissions} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, Router, routes, SafetyCheckUnusedSitePermissionsModuleInteractions, SettingsPluralStringProxyImpl, SettingsRoutes} from 'chrome://settings/settings.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
// clang-format on

suite('CrSettingsSafetyHubUnusedSitePermissionsTest', function() {
  let browserProxy: TestSafetyHubBrowserProxy;
  let pluralString: TestPluralStringProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  let testElement: SettingsSafetyHubUnusedSitePermissionsModuleElement;
  let testRoutes: SettingsRoutes;

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

  function assertEqualsMockData(siteList: UnusedSitePermissions[]) {
    // |siteList| coming from WebUI may have the additional property |detail|,
    // so assertDeepEquals doesn't work to compare it with |mockData|. We care
    // about origins and associated permissions being equal.
    assertEquals(siteList.length, mockData.length);
    for (const [i, site] of siteList.entries()) {
      assertEquals(site!.origin, mockData[i]!.origin);
      assertDeepEquals(site!.permissions, mockData[i]!.permissions);
    }
  }

  function assertInitialUi() {
    const expectedSiteCount = mockData.length;
    assertEquals(getSiteList().length, expectedSiteCount);
    assertToast(false);
  }

  /** Assert visibility and content of the undo toast. */
  function assertToast(shouldBeOpen: boolean, expectedText?: string) {
    const undoToast = testElement.$.undoToast;
    if (!shouldBeOpen) {
      assertFalse(undoToast.open);
      return;
    }
    assertTrue(undoToast.open);

    if (expectedText !== undefined) {
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
    return testElement.$.module.shadowRoot!.querySelectorAll('.site-entry')!;
  }

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement =
        document.createElement('settings-safety-hub-unused-site-permissions');
    Router.getInstance().navigateTo(testRoutes.SAFETY_HUB);
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
    testRoutes = {
      SAFETY_HUB: routes.SAFETY_HUB,
    } as unknown as SettingsRoutes;
    Router.resetInstanceForTesting(new Router(routes));
    await createPage();
    metricsBrowserProxy.reset();
    assertInitialUi();
  });

  test('Unused Site Permission strings', function() {
    const siteList = getSiteList();
    assertEquals(4, siteList.length);

    // Check that the text describing the permissions is correct.
    assertEquals(
        mockData[0]!.origin,
        siteList[0]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location',
        siteList[0]!.querySelector('.cr-secondary-text')!.textContent!.trim());

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

  test('Record Suggestions Count', async function() {
    await createPage();
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubUnusedSitePermissionsModuleListCountHistogram');
    assertEquals(getSiteList().length, result);
  });

  test('Allow Again Click', async function() {
    const siteList = getSiteList();
    siteList[0]!.querySelector('cr-icon-button')!.click();

    // Ensure the browser proxy call is done.
    const expectedOrigin =
        siteList[0]!.querySelector('.site-representation')!.textContent!.trim();
    const [origin] =
        await browserProxy.whenCalled('allowPermissionsAgainForUnusedSite');
    assertEquals(origin, expectedOrigin);

    // Ensure the metric for 'Allow Again' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckUnusedSitePermissionsModuleInteractions.ALLOW_AGAIN, result);
  });

  test('Undo Allow Again', async function() {
    for (const [i, site] of getSiteList().entries()) {
      browserProxy.resetResolver('undoAllowPermissionsAgainForUnusedSite');
      site!.querySelector('cr-icon-button')!.click();
      const expectedOrigin =
          site!.querySelector('.site-representation')!.textContent!.trim();

      // Ensure the toast behaves correctly.
      const expectedToastText = testElement.i18n(
          'safetyCheckUnusedSitePermissionsToastLabel', expectedOrigin);
      assertToast(true, expectedToastText);

      // Reset the action captured by clicking the allow again button.
      metricsBrowserProxy.reset();

      // Ensure proxy call for undo is sent correctly.
      testElement.$.toastUndoButton.click();
      const [unusedSitePermissions] = await browserProxy.whenCalled(
          'undoAllowPermissionsAgainForUnusedSite');
      assertEquals(unusedSitePermissions.origin, expectedOrigin);
      assertDeepEquals(
          unusedSitePermissions.permissions, mockData[i]!.permissions);
      // UI should be back to its initial state.
      webUIListenerCallback(
          SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
      flush();

      // Ensure the metric for 'Undo Allow Again' action is recorded.
      const result = await metricsBrowserProxy.whenCalled(
          'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram');
      assertEquals(
          SafetyCheckUnusedSitePermissionsModuleInteractions.UNDO_ALLOW_AGAIN,
          result);

      assertInitialUi();
    }
  });

  test('Undo Allow Again via Ctrl+Z', async function() {
    for (const [i, site] of getSiteList().entries()) {
      assertTrue(!!site);
      browserProxy.resetResolver('undoAllowPermissionsAgainForUnusedSite');
      const allowAgainButton = site.querySelector('cr-icon-button');
      assertTrue(!!allowAgainButton);
      allowAgainButton.click();
      const expectedOrigin =
          site!.querySelector('.site-representation')!.textContent!.trim();

      // Reset the action captured by pressing Ctrl+Z.
      metricsBrowserProxy.reset();

      // Ensure the toast behaves correctly.
      const expectedToastText = testElement.i18n(
          'safetyCheckUnusedSitePermissionsToastLabel', expectedOrigin);
      assertToast(true, expectedToastText);
      // Ensure proxy call for undo is sent correctly after pressing Ctrl+Z.
      keyDownOn(document.documentElement, 0, isMac ? 'meta' : 'ctrl', 'z');
      const [unusedSitePermissions] = await browserProxy.whenCalled(
          'undoAllowPermissionsAgainForUnusedSite');
      assertEquals(unusedSitePermissions.origin, expectedOrigin);
      assertDeepEquals(
          unusedSitePermissions.permissions, mockData[i]!.permissions);
      // UI should be back to its initial state.
      webUIListenerCallback(
          SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
      flush();

      // Ensure the metric for 'Undo Allow Again' action is recorded.
      const result = await metricsBrowserProxy.whenCalled(
          'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram');
      assertEquals(
          SafetyCheckUnusedSitePermissionsModuleInteractions.UNDO_ALLOW_AGAIN,
          result);

      assertInitialUi();
    }
  });

  test('Got It Click', async function() {
    testElement.$.gotItButton.click();
    await flushTasks();

    // Ensure the browser proxy call is done.
    await browserProxy.whenCalled(
        'acknowledgeRevokedUnusedSitePermissionsList');

    // Ensure the metric for 'Acknowledge All' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckUnusedSitePermissionsModuleInteractions.ACKNOWLEDGE_ALL,
        result);
  });

  test('Undo Got It', async function() {
    testElement.$.gotItButton.click();
    // Ensure the toast behaves correctly.
    await assertPluralString(
        'safetyCheckUnusedSitePermissionsToastBulkLabel', mockData.length, 2);
    assertToast(true);
    // Ensure proxy call is sent correctly for undo.

    // Reset the action captured by clicking the Got It button.
    metricsBrowserProxy.reset();

    testElement.$.bulkUndoButton.click();
    const [unusedSitePermissionsList] = await browserProxy.whenCalled(
        'undoAcknowledgeRevokedUnusedSitePermissionsList');
    assertEqualsMockData(unusedSitePermissionsList);
    // UI should be back to its initial state.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
    assertInitialUi();
    // Check visibility of buttons
    assertTrue(isVisible(testElement.$.gotItButton));
    assertFalse(isVisible(testElement.$.bulkUndoButton));

    // Ensure the metric for 'Undo Acknowledge All' action is recorded.
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckUnusedSitePermissionsModuleInteractions.UNDO_ACKNOWLEDGE_ALL,
        result);
  });

  test('Got It Toast Strings', async function() {
    // Check plural version of the string.
    testElement.$.gotItButton.click();
    await flushTasks();
    await assertPluralString(
        'safetyCheckUnusedSitePermissionsToastBulkLabel', mockData.length, 2);
    assertToast(true);

    // Check singular version of the string.
    const oneElementMockData = mockData.slice(0, 1);
    browserProxy.setUnusedSitePermissions(oneElementMockData);
    await createPage();
    assertToast(false);
    testElement.$.gotItButton.click();
    await flushTasks();
    await assertPluralString(
        'safetyCheckUnusedSitePermissionsToastBulkLabel', 1, 2);
    assertToast(true);
  });

  test('Header Strings', async function() {
    // Check header string for plural case.
    let entries = getSiteList();
    assertEquals(4, entries.length);
    await assertPluralString('safetyCheckUnusedSitePermissionsPrimaryLabel', 4);

    // Check header string for singular case.
    const oneElementMockData = mockData.slice(0, 1);
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, oneElementMockData);
    await flushTasks();

    entries = getSiteList();
    assertEquals(1, entries.length);
    await assertPluralString('safetyCheckUnusedSitePermissionsPrimaryLabel', 1);

    // Check header string for completion case.
    webUIListenerCallback(SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();

    const expectedCompletionHeader =
        testElement.i18n('safetyCheckUnusedSitePermissionsDoneLabel');
    assertEquals(expectedCompletionHeader, testElement.$.module.header);
    assertEquals('', testElement.$.module.subheader);

    // Check visibility of buttons
    assertFalse(isVisible(testElement.$.gotItButton));
    assertTrue(isVisible(testElement.$.bulkUndoButton));
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
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckUnusedSitePermissionsModuleInteractions.GO_TO_SETTINGS,
        result);
  });
});
