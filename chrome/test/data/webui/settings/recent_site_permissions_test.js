// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
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
        recentPermissions: [
          {
            setting: ContentSetting.BLOCK,
            type: ContentSettingsTypes.GEOLOCATION,
          },
          {
            setting: ContentSetting.BLOCK,
            type: ContentSettingsTypes.NOTIFICATIONS,
          },
          {
            setting: ContentSetting.BLOCK,
            type: ContentSettingsTypes.MIC,
          },
          {
            setting: ContentSetting.ALLOW,
            type: ContentSettingsTypes.CAMERA,
          },
          {
            setting: ContentSetting.ALLOW,
            type: ContentSettingsTypes.ADS,
          },
          {
            setting: ContentSetting.BLOCK,
            source: SiteSettingSource.EMBARGO,
            type: ContentSettingsTypes.MIDI_DEVICES,
          },
        ],
      },
      {
        origin: 'https://bar.com',
        recentPermissions: [
          {
            setting: ContentSetting.ALLOW,
            type: ContentSettingsTypes.PROTOCOL_HANDLERS,
          },
        ]
      },
      {
        origin: 'http://foo.com',
        recentPermissions: [
          {
            setting: ContentSetting.BLOCK,
            type: ContentSettingsTypes.POPUPS,
          },
          {
            setting: ContentSetting.BLOCK,
            source: SiteSettingSource.EMBARGO,
            type: ContentSettingsTypes.CLIPBOARD,
          },
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

    // Check that the text describing the changed permissions is correct.
    const i18n = testElement.i18n.bind(testElement);

    const expectedPermissionString1 = i18n(
                                          'recentPermissionAllowedTwoItems',
                                          i18n('siteSettingsCameraMidSentence'),
                                          i18n('siteSettingsAdsMidSentence')) +
        `${i18n('sentenceEnd')} ` +
        i18n('recentPermissionAutoBlockedOneItem',
             i18n('siteSettingsMidiDevicesMidSentence')) +
        `${i18n('sentenceEnd')} ` +
        i18n('recentPermissionBlockedMoreThanTwoItems',
             i18n('siteSettingsLocationMidSentence'), 2) +
        i18n('sentenceEnd');

    const expectedPermissionString2 = i18n(
        'recentPermissionAllowedOneItem',
        i18n('siteSettingsHandlersMidSentence'));

    const expectedPermissionString3 =
        i18n(
            'recentPermissionAutoBlockedOneItem',
            i18n('siteSettingsClipboardMidSentence')) +
        `${i18n('sentenceEnd')} ` +
        i18n(
            'recentPermissionBlockedOneItem',
            i18n('siteSettingsPopupsMidSentence')) +
        i18n('sentenceEnd');

    assertEquals(
        expectedPermissionString1,
        siteEntries[0].querySelector('.second-line').textContent.trim());
    assertEquals(
        expectedPermissionString2,
        siteEntries[1].querySelector('.second-line').textContent.trim());
    assertEquals(
        expectedPermissionString3,
        siteEntries[2].querySelector('.second-line').textContent.trim());
  });
});
