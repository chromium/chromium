// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NotificationsPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SettingsState, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting, createSiteSettingsPrefs} from './test_util.js';

const NOTIFICATIONS_PREF = 'generated.notification';

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: NOTIFICATIONS_PREF,
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: SettingsState.CPSS,
    },
  ];
}

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

suite('NotificationsPage', function() {
  let page: NotificationsPageElement;
  let prefService: PrefService;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;

  function createPage() {
    page = document.createElement('settings-notifications-page');
    document.body.appendChild(page);
    return microtasksFinished();
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    return createPage();
  });

  teardown(function() {
    page.remove();
  });

  test('NotificationPage', function() {
    const notificationDefaultRadioGroup =
        page.shadowRoot.querySelector('#notificationDefaultRadioGroup');
    assertTrue(!!notificationDefaultRadioGroup);

    const categorySettingExceptions =
        page.shadowRoot.querySelector('category-setting-exceptions');
    assertTrue(!!categorySettingExceptions);
    assertTrue(isVisible(categorySettingExceptions));
    assertEquals(
        ContentSettingsTypes.NOTIFICATIONS, categorySettingExceptions.category);
  });

  test('notificationCPSS', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ASK));
    await microtasksFinished();

    const cpssRadioGroup =
        page.shadowRoot.querySelector('settings-radio-group');
    assertTrue(!!cpssRadioGroup);

    const radioGroup =
        page.shadowRoot.querySelector('settings-category-default-radio-group');
    assertTrue(!!radioGroup);
    assertTrue(isVisible(radioGroup));
    assertTrue(isVisible(cpssRadioGroup));
    assertEquals(
        SettingsState.CPSS, prefService.getPref(NOTIFICATIONS_PREF).value);

    radioGroup.$.blockRadioOption.click();
    await microtasksFinished();
    assertFalse(isVisible(cpssRadioGroup));

    radioGroup.$.askRadioOption.click();
    await microtasksFinished();
    assertTrue(isVisible(cpssRadioGroup));
    assertEquals(
        SettingsState.CPSS, prefService.getPref(NOTIFICATIONS_PREF).value);
  });

  test('cpssRadioGroup', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ASK));
    await microtasksFinished();

    const cpssRadioGroup =
        page.shadowRoot.querySelector('settings-radio-group');
    assertTrue(!!cpssRadioGroup);
    assertEquals(SettingsState.CPSS.toString(), cpssRadioGroup.selected);
    assertEquals(
        SettingsState.CPSS, prefService.getPref(NOTIFICATIONS_PREF).value);

    const quietRadioButton = cpssRadioGroup.querySelector<HTMLElement>(
        `settings-collapse-radio-button[name="${SettingsState.QUIET}"]`);
    assertTrue(!!quietRadioButton);
    quietRadioButton.click();
    await microtasksFinished();
    assertEquals(
        SettingsState.QUIET, prefService.getPref(NOTIFICATIONS_PREF).value);
    assertEquals(SettingsState.QUIET.toString(), cpssRadioGroup.selected);

    prefService.setPrefValue(NOTIFICATIONS_PREF, SettingsState.LOUD);
    await microtasksFinished();
    assertEquals(SettingsState.LOUD.toString(), cpssRadioGroup.selected);
  });
});
