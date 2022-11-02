// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Router, routes, SafetyCheckIconStatus, SettingsSafetyCheckNotificationPermissionsElement, SettingsSafetyCheckUnusedSitePermissionsElement} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {NotificationPermission, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

import {assertSafetyCheckChild} from './safety_check_test_utils.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
// clang-format on

suite('SafetyCheckUnusedSitePermissionsUiTests', function() {
  let page: SettingsSafetyCheckUnusedSitePermissionsElement;

  setup(function() {
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    page =
        document.createElement('settings-safety-check-unused-site-permissions');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('unusedSitesPermissionsReviewUiTest', function() {
    // Ensure the elements are correct.
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.UNUSED_SITE_PERMISSIONS,
      label: 'Permissions removed from unused websites',
      buttonLabel: 'Review',
      buttonAriaLabel: 'Review',
    });

    // User clicks review button.
    page.$.safetyCheckChild.shadowRoot!.querySelector<HTMLElement>(
                                           '#button')!.click();

    // Ensure the correct Settings page is shown.
    assertEquals(routes.SITE_SETTINGS, Router.getInstance().getCurrentRoute());
  });
});

suite('SafetyCheckNotificationPermissionsUiTests', function() {
  let page: SettingsSafetyCheckNotificationPermissionsElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  const origin1 = 'www.example1.com';
  const detail1 = 'About 4 notifications a day';
  const origin2 = 'www.example2.com';
  const detail2 = 'About 1 notification a day';

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
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
      buttonAriaLabel:
          'Review 2 sites that recently sent a lot of notifications',
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
      buttonAriaLabel:
          'Review 1 site that recently sent a lot of notifications',
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
