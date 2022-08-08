// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Router, routes, SafetyCheckIconStatus, SettingsSafetyCheckUnusedSitePermissionsElement} from 'chrome://settings/settings.js';
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
    flush();

    // Ensure the elements are correct.
    assertSafetyCheckChild({
      page: page,
      iconStatus: SafetyCheckIconStatus.WARNING,
      label: 'Permissions removed from unused websites.',
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