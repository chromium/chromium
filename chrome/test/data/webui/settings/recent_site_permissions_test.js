// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {isChildVisible, isVisible} from '../test_util.m.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

suite('CrSettingsRecentSitePermissionsTest', function() {
  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  /** @type {!SettingsRecentSitePermissionsElement} */
  let testElement;

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;

    document.body.innerHTML = '';
    testElement =
        /** @type {!SettingsRecentSitePermissionsElement} */
        (document.createElement('settings-recent-site-permissions'));
    document.body.appendChild(testElement);
    flush();
  });

  teardown(function() {
    testElement.remove();
    Router.getInstance().navigateTo(routes.BASIC);
  });

  test('No recent permissions', async function() {
    browserProxy.setRecentSitePermissions([]);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
    await browserProxy.whenCalled('getRecentSitePermissions');
    flush();
    assertTrue(isChildVisible(testElement, '#noPermissionsText'));
  });

  test('Various recent permissions', async function() {
    const mockData = [
      {
        origin: 'https://bar.com',
        incognito: true,
        recentPermissions:
            [{setting: ContentSetting.BLOCK, displayName: 'location'}]
      },
      {
        origin: 'https://bar.com',
        recentPermissions:
            [{setting: ContentSetting.ALLOW, displayName: 'notifications'}]
      },
      {
        origin: 'http://foo.com',
        recentPermissions: [
          {setting: ContentSetting.BLOCK, displayName: 'popups'}, {
            setting: ContentSetting.BLOCK,
            displayName: 'clipboard',
            source: SiteSettingSource.EMBARGO
          }
        ]
      },
    ];
    browserProxy.setRecentSitePermissions(mockData);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
    await browserProxy.whenCalled('getRecentSitePermissions');
    flush();

    assertFalse(testElement.noRecentPermissions);
    assertFalse(isChildVisible(testElement, '#noPermissionsText'));

    const siteEntries = testElement.shadowRoot.querySelectorAll('.link-button');
    assertEquals(3, siteEntries.length);

    const incognitoIcons = /** @type !NodeList<!HTMLElement> */ (
        testElement.shadowRoot.querySelectorAll('.incognito-icon'));
    assertTrue(isVisible(incognitoIcons[0]));
    assertFalse(isVisible(incognitoIcons[1]));
    assertFalse(isVisible(incognitoIcons[2]));
  });
});
