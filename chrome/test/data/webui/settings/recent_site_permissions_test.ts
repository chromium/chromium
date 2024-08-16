// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsRecentSitePermissionsElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createRawSiteException} from './test_util.js';

// clang-format on

suite('CrSettingsRecentSitePermissionsTest', function() {
  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  let testElement: SettingsRecentSitePermissionsElement;

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-recent-site-permissions');
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

  test('Content setting strings', async function() {
    // Ensure no errors are generated for recent permissions for any content
    // settings type. Any JS errors are treated as a test failure, so no
    // explicit assertions are included.
    const origin = 'https://bar.com';
    for (const contentSettingType of Object.values(ContentSettingsTypes)) {
      if (contentSettingType === ContentSettingsTypes.STORAGE_ACCESS ||
          contentSettingType ===
              ContentSettingsTypes.TOP_LEVEL_STORAGE_ACCESS) {
        // Skip permission types that are not single origin; in the actual
        // implementation, getRecentSitePermissions only lists single origin
        // permissions.
        continue;
      }
      Router.getInstance().navigateTo(routes.BASIC);
      await flushTasks();
      const mockData = [{
        origin,
        displayName: 'bar.com',
        incognito: false,
        recentPermissions: [createRawSiteException(origin, {
          setting: ContentSetting.BLOCK,
          type: contentSettingType,
        })],
      }];
      browserProxy.setRecentSitePermissions(mockData);
      Router.getInstance().navigateTo(routes.SITE_SETTINGS);
      await browserProxy.whenCalled('getRecentSitePermissions');
      browserProxy.reset();
    }
  });

  test('Various recent permissions', async function() {
    const scheme = 'https://';
    const host1 = 'bar.com';
    const host2 = 'foo.com';
    const origin1 = `${scheme}${host1}`;
    const origin2 = `${scheme}${host2}`;
    const mockData = [
      {
        origin: origin1,
        displayName: host1,
        incognito: true,
        recentPermissions: [
          createRawSiteException(origin1, {
            setting: ContentSetting.BLOCK,
            type: ContentSettingsTypes.GEOLOCATION,
          }),
          createRawSiteException(origin1, {
            setting: ContentSetting.BLOCK,
            type: ContentSettingsTypes.NOTIFICATIONS,
          }),
          createRawSiteException(origin1, {
            setting: ContentSetting.BLOCK,
            type: ContentSettingsTypes.MIC,
          }),
          createRawSiteException(origin1, {
            setting: ContentSetting.ALLOW,
            type: ContentSettingsTypes.CAMERA,
          }),
          createRawSiteException(origin1, {
            setting: ContentSetting.ALLOW,
            type: ContentSettingsTypes.ADS,
          }),
          createRawSiteException(origin1, {
            setting: ContentSetting.BLOCK,
            source: SiteSettingSource.EMBARGO,
            type: ContentSettingsTypes.MIDI_DEVICES,
          }),
        ],
      },
      {
        origin: origin2,
        displayName: host2,
        incognito: false,
        recentPermissions: [
          createRawSiteException(origin2, {
            setting: ContentSetting.BLOCK,
            type: ContentSettingsTypes.POPUPS,
          }),
          createRawSiteException(origin2, {
            setting: ContentSetting.BLOCK,
            source: SiteSettingSource.EMBARGO,
            type: ContentSettingsTypes.CLIPBOARD,
          }),
        ],
      },
    ];
    browserProxy.setRecentSitePermissions(mockData);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS);
    await browserProxy.whenCalled('getRecentSitePermissions');
    flush();

    assertFalse(testElement.noRecentPermissions);
    assertFalse(isChildVisible(testElement, '#noPermissionsText'));

    const siteEntries =
        testElement.shadowRoot!.querySelectorAll('.link-button');
    assertEquals(2, siteEntries.length);

    assertEquals(
        host1,
        siteEntries[0]!.querySelector(
                           '.url-directionality')!.textContent!.trim());
    assertEquals(
        host2,
        siteEntries[1]!.querySelector(
                           '.url-directionality')!.textContent!.trim());

    const incognitoIcons =
        testElement.shadowRoot!.querySelectorAll<HTMLElement>(
            '.incognito-icon');
    assertTrue(isVisible(incognitoIcons[0]!));
    assertFalse(isVisible(incognitoIcons[1]!));

    // Check that the text describing the changed permissions is correct.
    const i18n = testElement.i18n.bind(testElement);

    let allowed = i18n(
        'recentPermissionAllowedTwoItems',
        i18n('siteSettingsCameraMidSentence'),
        i18n('siteSettingsAdsMidSentence'));
    const autoBlocked = i18n(
        'recentPermissionAutoBlockedOneItem',
        i18n('siteSettingsMidiDevicesMidSentence'));
    let blocked = i18n(
        'recentPermissionBlockedMoreThanTwoItems',
        i18n('siteSettingsLocationMidSentence'), 2);

    const expectedPermissionString1 = `${allowed}${i18n('sentenceEnd')} ${
        autoBlocked}${i18n('sentenceEnd')} ${blocked}${i18n('sentenceEnd')}`;

    allowed = i18n(
        'recentPermissionAutoBlockedOneItem',
        i18n('siteSettingsClipboardMidSentence'));
    blocked = i18n(
        'recentPermissionBlockedOneItem',
        i18n('siteSettingsPopupsMidSentence'));

    const expectedPermissionString3 =
        `${allowed}${i18n('sentenceEnd')} ${blocked}${i18n('sentenceEnd')}`;

    assertEquals(
        expectedPermissionString1,
        siteEntries[0]!.querySelector('.second-line')!.textContent!.trim());
    assertEquals(
        expectedPermissionString3,
        siteEntries[1]!.querySelector('.second-line')!.textContent!.trim());
  });
});
