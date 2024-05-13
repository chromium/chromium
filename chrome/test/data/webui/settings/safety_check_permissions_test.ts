// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSafetyCheckNotificationPermissionsElement, SettingsSafetyCheckPageElement, SettingsSafetyCheckUnusedSitePermissionsElement} from 'chrome://settings/settings.js';
import {resetRouterForTesting, MetricsBrowserProxyImpl, Router, routes, SafetyCheckIconStatus, SafetyCheckInteractions} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {NotificationPermission, UnusedSitePermissions} from 'chrome://settings/lazy_load.js';
import {ContentSettingsTypes, SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {assertSafetyCheckChild} from './safety_check_test_utils.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
// clang-format on

suite('SafetyCheckUnusedSitePermissionsUiTests', function() {
  let page: SettingsSafetyCheckUnusedSitePermissionsElement;
  let browserProxy: TestSafetyHubBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  const origin1 = 'www.example1.com';
  const origin2 = 'www.example2.com';

  setup(function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function createPage(data: UnusedSitePermissions[]) {
    browserProxy.setUnusedSitePermissions(data);

    page =
        document.createElement('settings-safety-check-unused-site-permissions');
    Router.getInstance().navigateTo(routes.PRIVACY);
    document.body.appendChild(page);
    flush();
  }

  teardown(function() {
    page.remove();
  });

  test('unusedSitesPermissionsReviewUiTest', async function() {
    const mockData = [
      {
        origin: origin1,
        permissions: [
          ContentSettingsTypes.GEOLOCATION,
          ContentSettingsTypes.CAMERA,
        ],
        expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
      },
      {
        origin: origin2,
        permissions:
            [ContentSettingsTypes.POPUPS, ContentSettingsTypes.SENSORS],
        expiration: '13348540800000000',  // Represents 2024-01-01T00:00:00.
      },
    ];
    createPage(mockData);

    await browserProxy.whenCalled('getRevokedUnusedSitePermissionsList');
    flush();

    const headerLabel =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckUnusedSitePermissionsHeaderLabel', mockData.length);

    // Ensure the elements are correct.
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.UNUSED_SITE_PERMISSIONS,
      label: headerLabel,
      buttonLabel: loadTimeData.getString('safetyCheckReview'),
      buttonAriaLabel: loadTimeData.getString(
          'safetyCheckUnusedSitePermissionsHeaderAriaLabel'),
    });

    // User clicks review button.
    page.$.safetyCheckChild.shadowRoot!.querySelector<HTMLElement>(
                                           '#button')!.click();

    // Ensure the correct Settings page is shown.
    assertEquals(routes.SITE_SETTINGS, Router.getInstance().getCurrentRoute());

    const resultHistogram = await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckInteractionHistogram');
    assertEquals(
        SafetyCheckInteractions.UNUSED_SITE_PERMISSIONS_REVIEW,
        resultHistogram);
    const resultAction = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'Settings.SafetyCheck.ReviewUnusedSitePermissions', resultAction);
  });
});

suite('SafetyCheckNotificationPermissionsUiTests', function() {
  let page: SettingsSafetyCheckNotificationPermissionsElement;
  let browserProxy: TestSafetyHubBrowserProxy;

  const origin1 = 'www.example1.com';
  const detail1 = 'About 4 notifications a day';
  const origin2 = 'www.example2.com';
  const detail2 = 'About 1 notification a day';

  setup(function() {
    browserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function createPage(data: NotificationPermission[]) {
    browserProxy.setNotificationPermissionReview(data);

    page = document.createElement(
        'settings-safety-check-notification-permissions');
    document.body.appendChild(page);
    flush();
  }

  teardown(function() {
    page.remove();
  });

  test('twoNotificationPermissionsReviewUiTest', async function() {
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
    createPage(mockData);

    await browserProxy.whenCalled('getNotificationPermissionReview');

    await PluralStringProxyImpl.getInstance().getPluralString(
        'safetyCheckNotificationPermissionReviewHeaderLabel', mockData.length);

    await PluralStringProxyImpl.getInstance().getPluralString(
        'safetyCheckNotificationPermissionReviewPrimaryLabel', mockData.length);

    // Ensure the elements are correct.
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.NOTIFICATION_PERMISSIONS,
      label: 'Review <b>2 sites</b> that recently sent a lot of notifications',
      buttonLabel: 'Review',
      buttonAriaLabel: 'Review notification permissions',
    });

    // User clicks review button.
    page.$.safetyCheckChild.shadowRoot!.querySelector<HTMLElement>(
                                           '#button')!.click();

    // Ensure the correct Settings page is shown.
    assertEquals(
        routes.SITE_SETTINGS_NOTIFICATIONS,
        Router.getInstance().getCurrentRoute());
  });

  test('oneNotificationPermissionsReviewUiTest', async function() {
    const mockData = [
      {
        origin: origin1,
        notificationInfoString: detail1,
      },
    ];
    createPage(mockData);

    await browserProxy.whenCalled('getNotificationPermissionReview');

    await PluralStringProxyImpl.getInstance().getPluralString(
        'safetyCheckNotificationPermissionReviewHeaderLabel', mockData.length);

    await PluralStringProxyImpl.getInstance().getPluralString(
        'safetyCheckNotificationPermissionReviewPrimaryLabel', mockData.length);

    // Ensure the elements are correct.
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.NOTIFICATION_PERMISSIONS,
      label: 'Review <b>1 site</b> that recently sent a lot of notifications',
      buttonLabel: 'Review',
      buttonAriaLabel: 'Review notification permissions',
    });

    // User clicks review button.
    page.$.safetyCheckChild.shadowRoot!.querySelector<HTMLElement>(
                                           '#button')!.click();

    // Ensure the correct Settings page is shown.
    assertEquals(
        routes.SITE_SETTINGS_NOTIFICATIONS,
        Router.getInstance().getCurrentRoute());
  });
});

suite('SafetyCheckPagePermissionModulesTest', function() {
  let page: SettingsSafetyCheckPageElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let permissionsBrowserProxy: TestSafetyHubBrowserProxy;
  const notificationElementName =
      'settings-safety-check-notification-permissions';
  const unusedSiteElementName = 'settings-safety-check-unused-site-permissions';
  const notificationMockData = [
    {
      origin: 'www.example1.com',
      notificationInfoString: 'About 4 notifications a day',
    },
  ];
  const unusedSiteMockData = [
    {
      origin: 'www.example1.com',
      permissions:
          [ContentSettingsTypes.GEOLOCATION, ContentSettingsTypes.CAMERA],
      expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
    },
  ];

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    permissionsBrowserProxy = new TestSafetyHubBrowserProxy();
    SafetyHubBrowserProxyImpl.setInstance(permissionsBrowserProxy);
    resetRouterForTesting();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function createPage() {
    page = document.createElement('settings-safety-check-page');
    Router.getInstance().navigateTo(routes.PRIVACY);
    document.body.appendChild(page);
    flush();
  }

  async function createPageForNotificationPermissions(
      mockData: NotificationPermission[]) {
    permissionsBrowserProxy.setNotificationPermissionReview(mockData);
    createPage();
    await permissionsBrowserProxy.whenCalled('getNotificationPermissionReview');
    await flushTasks();
  }

  async function createPageForUnusedSitePermissions(
      mockData: UnusedSitePermissions[]) {
    permissionsBrowserProxy.setUnusedSitePermissions(mockData);
    createPage();
    await permissionsBrowserProxy.whenCalled(
        'getRevokedUnusedSitePermissionsList');
    await flushTasks();
  }

  teardown(function() {
    page.remove();
  });

  test('notificationPermissionModuleVisible', async () => {
    await createPageForNotificationPermissions(notificationMockData);

    assertTrue(
        isVisible(page.shadowRoot!.querySelector(notificationElementName)));

    assertTrue(await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckNotificationsModuleEntryPointShown'));

    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();

    assertFalse(
        isVisible(page.shadowRoot!.querySelector(notificationElementName)));
  });

  test('notificationPermissionModuleEmptyList', async () => {
    await createPageForNotificationPermissions([]);

    assertFalse(
        isVisible(page.shadowRoot!.querySelector(notificationElementName)));
    assertFalse(await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckNotificationsModuleEntryPointShown'));

    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        notificationMockData);
    await flushTasks();

    assertTrue(
        isVisible(page.shadowRoot!.querySelector(notificationElementName)));
  });

  test('unusedSitePermissionsModuleEntryPointShown', async () => {
    await createPageForUnusedSitePermissions(unusedSiteMockData);

    assertTrue(
        isVisible(page.shadowRoot!.querySelector(unusedSiteElementName)));
    assertTrue(await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleEntryPointShown'));
  });

  test('unusedSitePermissionsModuleFeatureDisabled', async () => {
    loadTimeData.overrideValues(
        {safetyCheckUnusedSitePermissionsEnabled: false});
    await createPageForUnusedSitePermissions(unusedSiteMockData);

    assertFalse(
        isVisible(page.shadowRoot!.querySelector(unusedSiteElementName)));
    assertFalse(await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleEntryPointShown'));

    // Re-enable the unused site permissions feature.
    loadTimeData.overrideValues(
        {safetyCheckUnusedSitePermissionsEnabled: true});
  });

  test('unusedSitePermissionsModuleEmptyList', async () => {
    await createPageForUnusedSitePermissions([]);

    assertFalse(
        isVisible(page.shadowRoot!.querySelector(unusedSiteElementName)));
    assertFalse(await metricsBrowserProxy.whenCalled(
        'recordSafetyCheckUnusedSitePermissionsModuleEntryPointShown'));
  });
});
