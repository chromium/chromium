// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/os_settings.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';

suite('AppNotificationsSubpageTests', function() {
  /** @type {AppNotificationsSubpage} */
  let page;

  setup(function() {
    PolymerTest.clearBody;
    loadTimeData.overrideValues({showOsSettingsAppNotificationsRow: true});
    page = document.createElement('settings-app-notifications-subpage');
    document.body.appendChild(page);
    flush();
  });

  test('Each app-notification-row displays correctly', function() {
    assertTrue(!!page);
    flush();
    assertEquals(
        'Chrome',
        page.$.appNotificationsList.firstElementChild.$.appTitle.textContent
            .trim());
  });
});