// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, defaultSettingLabel, NotificationSetting, SettingsSiteSettingsPageElement, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrLinkRowElement} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';

// clang-format on

suite('SiteSettingsPage', function() {
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;
  let page: SettingsSiteSettingsPageElement;

  const testLabels: string[] = ['test label 1', 'test label 2'];

  function setupPage() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
    siteSettingsBrowserProxy.setCookieSettingDescription(testLabels[0]!);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-site-settings-page');
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
    assertEquals('a', defaultSettingLabel(ContentSetting.ALLOW, 'a', 'b'));
    assertEquals('b', defaultSettingLabel(ContentSetting.BLOCK, 'a', 'b'));
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
    const cookiesLinkRow =
        page.shadowRoot!.querySelector('#basicContentList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#cookies')!;
    assertEquals(testLabels[0], cookiesLinkRow.subLabel);

    webUIListenerCallback('cookieSettingDescriptionChanged', testLabels[1]);
    assertEquals(testLabels[1], cookiesLinkRow.subLabel);
  });

  test('NotificationsLinkRowSublabel', async function() {
    const notificationsLinkRow =
        page.shadowRoot!.querySelector('#basicPermissionsList')!.shadowRoot!
            .querySelector<CrLinkRowElement>('#notifications')!;

    page.set('prefs.generated.notification.value', NotificationSetting.BLOCK);
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsNotificationsBlocked'),
        notificationsLinkRow.subLabel);

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
  });

  test('ProtectedContentRow', function() {
    setupPage();
    page.shadowRoot!.querySelector<HTMLElement>('#expandContent')!.click();
    flush();
    assertTrue(isChildVisible(
        page.shadowRoot!.querySelector('#advancedContentList')!,
        '#protected-content'));
  });
});
