// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsPrivacyHubGeolocationSubpage} from 'chrome://os-settings/lazy_load.js';
import {GeolocationAccessLevel, Router, routes} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';

import {createFakeMetricsPrivate} from './privacy_hub_app_permission_test_util.js';


suite('<settings-privacy-hub-geolocation-subpage>', () => {
  let metrics: FakeMetricsPrivate;
  let privacyHubGeolocationSubpage: SettingsPrivacyHubGeolocationSubpage;

  async function initPage() {
    privacyHubGeolocationSubpage =
        document.createElement('settings-privacy-hub-geolocation-subpage');
    const prefs = {
      ash: {
        user: {
          geolocation_access_level: {
            key: 'ash.user.geolocation_access_level',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: GeolocationAccessLevel.ALLOWED,
          },
        },
      },
    };
    privacyHubGeolocationSubpage.prefs = prefs;
    document.body.appendChild(privacyHubGeolocationSubpage);
    flush();
  }


  setup(() => {
    metrics = createFakeMetricsPrivate();
    Router.getInstance().navigateTo(routes.PRIVACY_HUB_GEOLOCATION);
  });

  teardown(() => {
    privacyHubGeolocationSubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function histogram(): string {
    return 'ChromeOS.PrivacyHub.Geolocation.AccessLevelChanged.SystemSettings';
  }

  function setGeolocationAccessLevel(accessLevel: GeolocationAccessLevel) {
    const dropdown = privacyHubGeolocationSubpage.shadowRoot!
                         .querySelector('settings-dropdown-menu')!.shadowRoot!
                         .querySelector<HTMLSelectElement>('#dropdownMenu')!;
    assertTrue(!!dropdown);

    dropdown.value = accessLevel.toString();
    dropdown.dispatchEvent(new CustomEvent('change'));
    flush();
  }

  function getGeolocationAccessLevel(): GeolocationAccessLevel {
    const dropdown = privacyHubGeolocationSubpage.shadowRoot!
                         .querySelector('settings-dropdown-menu')!.shadowRoot!
                         .querySelector<HTMLSelectElement>('#dropdownMenu')!;
    assertTrue(!!dropdown);

    switch (dropdown.value) {
      case '0':
        return GeolocationAccessLevel.DISALLOWED;
      case '1':
        return GeolocationAccessLevel.ALLOWED;
      case '2':
        return GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM;
    }

    assertNotReached('Invalid GeolocationAccessLevel value detected');
  }

  test('Metric recorded when clicked', async () => {
    await initPage();

    assertEquals(
        0,
        metrics.countMetricValue(
            histogram(), GeolocationAccessLevel.DISALLOWED));
    assertEquals(
        0,
        metrics.countMetricValue(histogram(), GeolocationAccessLevel.ALLOWED));
    assertEquals(
        0,
        metrics.countMetricValue(
            histogram(), GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM));

    // Default access level should be ALLOWED.
    assertEquals(GeolocationAccessLevel.ALLOWED, getGeolocationAccessLevel());

    // Change dropdown and check the corresponding metric is recorded.
    setGeolocationAccessLevel(GeolocationAccessLevel.DISALLOWED);
    assertEquals(
        1,
        metrics.countMetricValue(
            histogram(), GeolocationAccessLevel.DISALLOWED));

    // Change dropdown and check the corresponding metric is recorded.
    setGeolocationAccessLevel(GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    assertEquals(
        1,
        metrics.countMetricValue(
            histogram(), GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM));

    // Change dropdown and check the corresponding metric is recorded.
    setGeolocationAccessLevel(GeolocationAccessLevel.ALLOWED);
    assertEquals(
        1,
        metrics.countMetricValue(histogram(), GeolocationAccessLevel.ALLOWED));
  });
});
