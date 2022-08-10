// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Router, routes, SafetyCheckIconStatus, SettingsSafetyCheckNotificationPermissionsElement, SettingsSafetyCheckUnusedSitePermissionsElement} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {assertSafetyCheckChild} from './safety_check_test_utils.js';
// clang-format on

suite('SafetyCheckUnusedSitePermissionsUiTests', function() {
  let page: SettingsSafetyCheckUnusedSitePermissionsElement;

  setup(function() {
    document.body.innerHTML = '';
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
      iconStatus: SafetyCheckIconStatus.WARNING,
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

  setup(function() {
    document.body.innerHTML = '';
    page = document.createElement(
        'settings-safety-check-notification-permissions');
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
  });

  test('notificationPermissionsReviewUiTest', function() {
    // Ensure the elements are correct.
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.WARNING,
      label: 'Review sites that recently sent a lot of notifications',
      buttonLabel: 'Review',
      buttonAriaLabel: 'Review',
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