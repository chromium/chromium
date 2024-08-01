// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {SettingsUnusedSitePermissionsElement, UnusedSitePermissions} from 'chrome://settings/lazy_load.js';
import {ContentSettingsTypes, SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import {MetricsBrowserProxyImpl, resetRouterForTesting, Router, routes, SafetyCheckUnusedSitePermissionsModuleInteractions} from 'chrome://settings/settings.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';

// clang-format on

suite('CrSettingsUnusedSitePermissionsTest', function() {
  let browserProxy: TestSafetyHubBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  let testElement: SettingsUnusedSitePermissionsElement;

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

  /* Asserts for each row whether or not it is animating. */
  function assertAnimation(expectedAnimation: boolean[]) {
    const rows = getSiteList();

    assertEquals(
        rows.length, expectedAnimation.length,
        'Provided ' + expectedAnimation.length +
            ' expectations but there are ' + rows.length + ' rows');
    for (const [i, row] of rows.entries()) {
      assertEquals(
          expectedAnimation[i]!, row!.classList.contains('removed'),
          'Expectation not met for row #' + i);
    }
  }

  function assertEqualsMockData(siteList: UnusedSitePermissions[]) {
    // |siteList| coming from WebUI may have the additional property |visible|,
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
    assertAnimation(new Array(expectedSiteCount).fill(false));
    assertToast(false);
  }

  /** Assert visibility and content of the undo toast. */
  function assertToast(shouldBeOpen: boolean, expectedText?: string) {
    const undoToast = testElement.shadowRoot!.querySelector('cr-toast')!;
    if (!shouldBeOpen) {
      assertFalse(undoToast.open);
      return;
    }
    assertTrue(undoToast.open);
    const actualText = undoToast.querySelector('div')!.textContent!.trim();
    assertEquals(expectedText, actualText);
  }

  function clickGotIt() {
    const button = testElement.shadowRoot!.querySelector<HTMLElement>(
        '.bulk-action-button');
    assertTrue(!!button);
    button.click();
  }

  function clickUndo() {
    testElement.shadowRoot!.querySelector(
                               'cr-toast')!.querySelector('cr-button')!.click();
  }

  function getSiteList() {
    return testElement.shadowRoot!.querySelectorAll('.site-list .site-entry');
  }

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-unused-site-permissions');
    testElement.setModelUpdateDelayMsForTesting(0);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
    document.body.appendChild(testElement);
    // Wait until the element has asked for the list of revoked permissions
    // that will be shown for review.
    await browserProxy.whenCalled('getRevokedUnusedSitePermissionsList');
    flush();
  }

  setup(async function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    browserProxy.setUnusedSitePermissions(mockData);
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    resetRouterForTesting();
    await createPage();
    // Clear the metrics that were recorded as part of the initial creation of
    // the page.
    metricsBrowserProxy.reset();
    assertInitialUi();
  });

  test('Capture metrics on visit', async function() {
    await createPage();
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckUnusedSitePermissionsModuleInteractions.OPEN_REVIEW_UI,
        result);
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
        siteList[0]!.querySelector('.secondary')!.textContent!.trim());

    assertEquals(
        mockData[1]!.origin,
        siteList[1]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone',
        siteList[1]!.querySelector('.secondary')!.textContent!.trim());

    assertEquals(
        mockData[2]!.origin,
        siteList[2]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone, camera',
        siteList[2]!.querySelector('.secondary')!.textContent!.trim());

    assertEquals(
        mockData[3]!.origin,
        siteList[3]!.querySelector(
                        '.site-representation')!.textContent!.trim());
    assertEquals(
        'Removed location, microphone, and 2 more',
        siteList[3]!.querySelector('.secondary')!.textContent!.trim());
  });

  test('Collapsible List', async function() {
    const expandButton =
        testElement.shadowRoot!.querySelector('cr-expand-button');
    assertTrue(!!expandButton);

    const unusedSitePermissionList =
        testElement.shadowRoot!.querySelector('cr-collapse');
    assertTrue(!!unusedSitePermissionList);

    // Button and list start out expanded.
    assertTrue(expandButton.expanded);
    assertTrue(unusedSitePermissionList.opened);

    // User collapses the list.
    expandButton.click();
    await expandButton.updateComplete;

    // Button and list are collapsed.
    assertFalse(expandButton.expanded);
    assertFalse(unusedSitePermissionList.opened);

    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckUnusedSitePermissionsModuleInteractions.MINIMIZE, result);

    // User expands the list.
    expandButton.click();
    await expandButton.updateComplete;

    // Button and list are expanded.
    assertTrue(expandButton.expanded);
    assertTrue(unusedSitePermissionList.opened);
  });

  test('Allow Again Click', async function() {
    const siteList = getSiteList();
    siteList[0]!.querySelector('cr-icon-button')!.click();

    assertAnimation([true, false, false, false]);
    // Ensure the browser proxy call is done.
    const expectedOrigin =
        siteList[0]!.querySelector('.site-representation')!.textContent!.trim();
    const [origin] =
        await browserProxy.whenCalled('allowPermissionsAgainForUnusedSite');
    assertEquals(origin, expectedOrigin);
  });

  test('Undo Allow Again', async function() {
    for (const [i, site] of getSiteList().entries()) {
      browserProxy.resetResolver('undoAllowPermissionsAgainForUnusedSite');
      site!.querySelector('cr-icon-button')!.click();
      const expectedAnimation = [false, false, false, false];
      expectedAnimation[i] = true;
      const expectedOrigin =
          site!.querySelector('.site-representation')!.textContent!.trim();

      assertAnimation(expectedAnimation);
      // Ensure the toast behaves correctly.
      const expectedToastText = testElement.i18n(
          'safetyCheckUnusedSitePermissionsToastLabel', expectedOrigin);
      assertToast(true, expectedToastText);
      // Ensure proxy call for undo is sent correctly.
      clickUndo();
      const [unusedSitePermissions] = await browserProxy.whenCalled(
          'undoAllowPermissionsAgainForUnusedSite');
      assertEquals(unusedSitePermissions.origin, expectedOrigin);
      assertDeepEquals(
          unusedSitePermissions.permissions, mockData[i]!.permissions);
      // UI should be back to its initial state.
      webUIListenerCallback(
          SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
      flush();
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
      const expectedAnimation = [false, false, false, false];
      expectedAnimation[i] = true;
      const expectedOrigin =
          site!.querySelector('.site-representation')!.textContent!.trim();

      assertAnimation(expectedAnimation);
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
      assertInitialUi();
    }
  });

  test('Got It Click', async function() {
    clickGotIt();
    await flushTasks();

    // Ensure the browser proxy call is done.
    await browserProxy.whenCalled(
        'acknowledgeRevokedUnusedSitePermissionsList');
  });

  test('Undo Got It', async function() {
    clickGotIt();
    // Ensure the toast behaves correctly.
    const expectedToastText =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckUnusedSitePermissionsToastBulkLabel', mockData.length);
    assertToast(true, expectedToastText);
    // Ensure proxy call is sent correctly for undo.
    clickUndo();
    const [unusedSitePermissionsList] = await browserProxy.whenCalled(
        'undoAcknowledgeRevokedUnusedSitePermissionsList');
    assertEqualsMockData(unusedSitePermissionsList);
    // UI should be back to its initial state.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, mockData);
    assertInitialUi();
  });

  test('Got It Toast Strings', async function() {
    // Check plural version of the string.
    clickGotIt();
    await flushTasks();
    const expectedPluralToastText =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckUnusedSitePermissionsToastBulkLabel', mockData.length);
    assertToast(true, expectedPluralToastText);

    // Check singular version of the string.
    const oneElementMockData = mockData.slice(0, 1);
    browserProxy.setUnusedSitePermissions(oneElementMockData);
    await createPage();
    assertToast(false);
    clickGotIt();
    await flushTasks();
    const expectedSingularToastText =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckUnusedSitePermissionsToastBulkLabel', 1);
    assertToast(true, expectedSingularToastText);
  });

  test('Allow again record metrics', async function() {
    const siteList = getSiteList();
    siteList[0]!.querySelector('cr-icon-button')!.click();
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckUnusedSitePermissionsModuleInteractions.ALLOW_AGAIN, result);
  });

  test('Undo allow again record metrics', async function() {
    const siteList = getSiteList();
    siteList[0]!.querySelector('cr-icon-button')!.click();
    // Reset the action captured by clicking the block button.
    metricsBrowserProxy.resetResolver(
        'recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram');
    clickUndo();
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckUnusedSitePermissionsModuleInteractions.UNDO_ALLOW_AGAIN,
        result);
  });

  test('Got it record metrics', async function() {
    clickGotIt();
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckUnusedSitePermissionsModuleInteractions.ACKNOWLEDGE_ALL,
        result);
  });

  test('Undo got it record metrics', async function() {
    clickGotIt();
    // Reset the action captured by clicking the got it button.
    metricsBrowserProxy.resetResolver(
        'recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram');
    clickUndo();
    const result = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleInteractionsHistogram');
    assertEquals(
        SafetyCheckUnusedSitePermissionsModuleInteractions.UNDO_ACKNOWLEDGE_ALL,
        result);
  });

  test('Review list size record metrics', async function() {
    browserProxy.setUnusedSitePermissions(mockData);
    await createPage();
    const resultNumSites = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsListCountHistogram');
    assertEquals(mockData.length, resultNumSites);

    metricsBrowserProxy.resetResolver(
        'recordSafetyCheckUnusedSitePermissionsListCountHistogram');

    browserProxy.setUnusedSitePermissions([]);
    await createPage();
    const resultEmpty = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsListCountHistogram');
    assertEquals(0, resultEmpty);
  });
});
