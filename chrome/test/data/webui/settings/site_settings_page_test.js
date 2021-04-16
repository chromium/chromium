// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, defaultSettingLabel, NotificationSetting, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';

import {assertEquals, assertTrue} from '../chai_assert.js';
import {eventToPromise,flushTasks, isChildVisible} from '../test_util.m.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

suite('SiteSettingsPage', function() {
  /** @type {?TestSiteSettingsPrefsBrowserProxy} */
  let siteSettingsBrowserProxy = null;

  /** @type {SettingsSiteSettingsPageElement} */
  let page;

  /** @type {Array<string>} */
  const testLabels = ['test label 1', 'test label 2'];

  function setupPage() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = siteSettingsBrowserProxy;
    siteSettingsBrowserProxy.setCookieSettingDescription(testLabels[0]);
    document.body.innerHTML = '';
    page = /** @type {!SettingsSiteSettingsPageElement} */ (
        document.createElement('settings-site-settings-page'));
    page.prefs = {
      generated: {
        notification: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: NotificationSetting.ASK,
        },
      },
    };
    document.body.appendChild(page);
    flush();
  }

  setup(setupPage);

  teardown(function() {
    page.remove();
  });

  test('DefaultLabels', function() {
    assertEquals(
        'a', defaultSettingLabel(ContentSetting.ALLOW, 'a', 'b', null));
    assertEquals(
        'b', defaultSettingLabel(ContentSetting.BLOCK, 'a', 'b', null));
    assertEquals('a', defaultSettingLabel(ContentSetting.ALLOW, 'a', 'b', 'c'));
    assertEquals('b', defaultSettingLabel(ContentSetting.BLOCK, 'a', 'b', 'c'));
    assertEquals(
        'c', defaultSettingLabel(ContentSetting.SESSION_ONLY, 'a', 'b', 'c'));
    assertEquals(
        'c', defaultSettingLabel(ContentSetting.DEFAULT, 'a', 'b', 'c'));
    assertEquals('c', defaultSettingLabel(ContentSetting.ASK, 'a', 'b', 'c'));
    assertEquals(
        'c',
        defaultSettingLabel(ContentSetting.IMPORTANT_CONTENT, 'a', 'b', 'c'));
  });

  test('CookiesLinkRowSublabel', async function() {
    setupPage();
    await siteSettingsBrowserProxy.whenCalled('getCookieSettingDescription');
    flush();
    const cookiesLinkRow = /** @type {!CrLinkRowElement} */ (
        page.$$('#basicContentList').$$('#cookies'));
    assertEquals(testLabels[0], cookiesLinkRow.subLabel);

    webUIListenerCallback('cookieSettingDescriptionChanged', testLabels[1]);
    assertEquals(testLabels[1], cookiesLinkRow.subLabel);
  });

  test('NotificationsLinkRowSublabel_RedesignDisabled', async function() {
    loadTimeData.overrideValues({
      enableContentSettingsRedesign: false,
    });

    const notificationsLinkRow = /** @type {!CrLinkRowElement} */ (
        page.$$('#basicPermissionsList').$$('#notifications'));

    page.set('prefs.generated.notification.value', NotificationSetting.ASK);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsAskBeforeSending'),
        notificationsLinkRow.subLabel);

    page.set('prefs.generated.notification.value', NotificationSetting.BLOCK);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsBlocked'),
        notificationsLinkRow.subLabel);
  });

  test('NotificationsLinkRowSublabel_RedesignEnabled', async function() {
    loadTimeData.overrideValues({
      enableContentSettingsRedesign: true,
    });

    const notificationsLinkRow = /** @type {!CrLinkRowElement} */ (
        page.$$('#basicPermissionsList').$$('#notifications'));

    page.set(
        'prefs.generated.notification.value',
        NotificationSetting.QUIETER_MESSAGING);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsNotificationsPartial'),
        notificationsLinkRow.subLabel);

    page.set('prefs.generated.notification.value', NotificationSetting.ASK);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsNotificationsAllowed'),
        notificationsLinkRow.subLabel);

    page.set('prefs.generated.notification.value', NotificationSetting.BLOCK);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsNotificationsBlocked'),
        notificationsLinkRow.subLabel);
  });

  test('ProtectedContentRow', function() {
    setupPage();
    page.$$('#expandContent').click();
    flush();
    assertTrue(isChildVisible(
        /** @type {!HTMLElement} */ (page.$$('#advancedContentList')),
        '#protected-content'));
  });
});
