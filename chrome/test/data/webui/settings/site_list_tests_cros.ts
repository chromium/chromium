// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AndroidInfoBrowserProxyImpl, ContentSetting, ContentSettingsTypes, SiteListElement, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, Router} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TEST_ANDROID_SMS_ORIGIN, TestAndroidInfoBrowserProxy} from './test_android_info_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair, createRawSiteException, createSiteSettingsPrefs, SiteSettingsPref} from './test_util.js';

// clang-format on

suite('SiteListChromeOS', function() {
  /**
   * A site list element created before each test.
   */
  let testElement: SiteListElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  /**
   * Mock AndroidInfoBrowserProxy to use during test.
   */
  let androidInfoBrowserProxy: TestAndroidInfoBrowserProxy;

  /**
   * An example Javascript pref for android_sms notification setting.
   */
  let prefsAndroidSms: SiteSettingsPref;

  // Initialize a site-list before each test.
  setup(function() {
    prefsAndroidSms = createSiteSettingsPrefs(
        [], [createContentSettingTypeToValuePair(
                ContentSettingsTypes.NOTIFICATIONS, [
                  // android sms setting.
                  createRawSiteException(TEST_ANDROID_SMS_ORIGIN),
                  // Non android sms setting that should be handled as usual.
                  createRawSiteException('http://bar.com'),
                ])]);

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    androidInfoBrowserProxy = new TestAndroidInfoBrowserProxy();
    AndroidInfoBrowserProxyImpl.setInstance(androidInfoBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('site-list');
    testElement.searchFilter = '';
    document.body.appendChild(testElement);
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    Router.getInstance().resetRouteForTesting();

    // Reset multidevice enabled flag.
    loadTimeData.overrideValues({multideviceAllowedByPolicy: false});
  });

  /** Configures the test element. */
  function setUpAndroidSmsNotifications() {
    browserProxy.setPrefs(prefsAndroidSms);
    testElement.categorySubtype = ContentSetting.ALLOW;
    testElement.category = ContentSettingsTypes.NOTIFICATIONS;
  }

  test('update androidSmsInfo', async function() {
    setUpAndroidSmsNotifications();
    assertEquals(0, androidInfoBrowserProxy.getCallCount('getAndroidSmsInfo'));

    loadTimeData.overrideValues({multideviceAllowedByPolicy: true});
    setUpAndroidSmsNotifications();
    // Assert 2 calls since the observer observes 2 properties.
    assertEquals(2, androidInfoBrowserProxy.getCallCount('getAndroidSmsInfo'));

    const results = await Promise.all([
      androidInfoBrowserProxy.whenCalled('getAndroidSmsInfo'),
      browserProxy.whenCalled('getExceptionList'),
    ]);

    const contentType = results[1] as ContentSettingsTypes;
    flush();
    assertEquals(ContentSettingsTypes.NOTIFICATIONS, contentType);
    assertEquals(2, testElement.sites.length);

    assertEquals(
        prefsAndroidSms.exceptions[contentType][0]!.origin,
        testElement.sites[0]!.origin);
    assertTrue(testElement.sites[0]!.showAndroidSmsNote!);

    assertEquals(
        prefsAndroidSms.exceptions[contentType][1]!.origin,
        testElement.sites[1]!.origin);
    assertEquals(undefined, testElement.sites[1]!.showAndroidSmsNote);
  });
});
