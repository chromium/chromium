// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsPrefsBrowserProxyImpl, SettingsState} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsPrivacyPageElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting, createSiteSettingsPrefs} from './test_util.js';

// clang-format on

function createPref(
    category: ContentSettingsTypes,
    contentSetting: ContentSetting): SiteSettingsPref {
  return createSiteSettingsPrefs(
      [
        createContentSettingTypeToValuePair(
            category, createDefaultContentSetting({
              setting: contentSetting,
            })),
      ],
      []);
}

suite(`NotificationsPage`, function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  }

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    return createPage();
  });

  teardown(function() {
    page.remove();
  });

  test('notificationCPSS', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ALLOW));

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_NOTIFICATIONS);
    await flushTasks();

    const settingsSubpage = page.shadowRoot!.querySelector('settings-subpage');
    assertTrue(!!settingsSubpage);
    assertTrue(isVisible(settingsSubpage));

    const radioGroup = page.shadowRoot!.querySelector<HTMLElement>(
        'settings-category-default-radio-group');
    assertTrue(!!radioGroup);
    assertTrue(isVisible(radioGroup));
    assertTrue(isChildVisible(page, '#notification-ask-quiet'));

    const blockNotification = radioGroup.shadowRoot!.querySelector<HTMLElement>(
        '#disabledRadioOption');
    assertTrue(!!blockNotification);
    blockNotification.click();
    await flushTasks();
    assertFalse(isChildVisible(page, '#notification-ask-quiet'));
    assertEquals(
        SettingsState.BLOCK, page.get('prefs.generated.notification.value'));

    const allowNotification = radioGroup.shadowRoot!.querySelector<HTMLElement>(
        '#enabledRadioOption');
    assertTrue(!!allowNotification);
    allowNotification.click();
    await flushTasks();
    assertTrue(isChildVisible(page, '#notification-ask-quiet'));
    assertEquals(
        SettingsState.CPSS, page.get('prefs.generated.notification.value'));
  });
});

// TODO(crbug.com/340743074): Remove tests after
// `PermissionSiteSettingsRadioButton` launched.
suite(`NotificationsPageWithNestedRadioButton`, function() {
  let page: SettingsPrivacyPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enablePermissionSiteSettingsRadioButton: false,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    page = document.createElement('settings-privacy-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  }

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    return createPage();
  });

  teardown(function() {
    page.remove();
  });

  test('notificationCPSS', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ALLOW));

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_NOTIFICATIONS);
    await flushTasks();

    const settingsSubpage = page.shadowRoot!.querySelector('settings-subpage');
    assertTrue(!!settingsSubpage);
    assertTrue(isVisible(settingsSubpage));
    assertTrue(isChildVisible(page, '#notificationRadioGroup'));
    assertTrue(isChildVisible(page, '#notification-ask-quiet'));

    const blockNotification =
        page.shadowRoot!.querySelector<HTMLElement>('#notification-block');
    assertTrue(!!blockNotification);
    blockNotification.click();
    await flushTasks();
    assertFalse(isChildVisible(page, '#notification-ask-quiet'));

    const allowNotification = page.shadowRoot!.querySelector<HTMLElement>(
        '#notification-ask-radio-button');
    assertTrue(!!allowNotification);
    allowNotification.click();
    await flushTasks();
    assertTrue(isChildVisible(page, '#notification-ask-quiet'));
  });
});
