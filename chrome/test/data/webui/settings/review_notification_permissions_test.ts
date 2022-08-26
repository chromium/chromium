// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrivacyPageBrowserProxyImpl, SettingsReviewNotificationPermissionsElement} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';

// clang-format on

suite('CrSettingsReviewNotificationPermissionsTest', function() {
  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestPrivacyPageBrowserProxy;

  let testElement: SettingsReviewNotificationPermissionsElement;

  const origin_1 = 'www.example1.com';
  const detail_1 = 'About 4 notifications a day';
  const origin_2 = 'www.example2.com';
  const detail_2 = 'About 1 notification a day';

  setup(function() {
    browserProxy = new TestPrivacyPageBrowserProxy();
    const mock_data = [
      {origin: origin_1, notificationInfoString: detail_1},
      {origin: origin_2, notificationInfoString: detail_2},
    ];
    browserProxy.setReviewNotificationPermissions(mock_data);
    PrivacyPageBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = '';
    testElement = document.createElement('review-notification-permissions');
    document.body.appendChild(testElement);
    flush();
  });

  teardown(function() {
    testElement.remove();
  });

  test('Notification Permission strings', async function() {
    await browserProxy.whenCalled('getReviewNotificationPermissions');
    flush();

    const entries = testElement.shadowRoot!.querySelectorAll('.cr-row');
    assertEquals(2, entries.length);

    // Check that the text describing the changed permissions is correct.
    assertEquals(
        origin_1,
        entries[0]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        detail_1,
        entries[0]!.querySelector('.second-line')!.textContent!.trim());
    assertEquals(
        origin_2,
        entries[1]!.querySelector('.site-representation')!.textContent!.trim());
    assertEquals(
        detail_2,
        entries[1]!.querySelector('.second-line')!.textContent!.trim());
  });
});
