// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GeolocationPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SettingsState, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting, createSiteSettingsPrefs} from './test_util.js';

const GEOLOCATION_PREF = 'generated.geolocation';

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: GEOLOCATION_PREF,
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

suite('GeolocationPage', function() {
  let page: GeolocationPageElement;
  let prefService: PrefService;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;

  function createPage() {
    page = document.createElement('settings-geolocation-page');
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

  test('LocationPage', function() {
    assertTrue(isChildVisible(page, '#locationDefaultRadioGroup'));
    const categorySettingExceptions =
        page.shadowRoot.querySelector('category-setting-exceptions');
    assertTrue(!!categorySettingExceptions);
    assertTrue(isVisible(categorySettingExceptions));
    assertEquals(
        ContentSettingsTypes.GEOLOCATION, categorySettingExceptions.category);
  });

  test('locationCPSS', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.ASK));
    await microtasksFinished();

    const radioGroup =
        page.shadowRoot.querySelector('settings-category-default-radio-group');
    assertTrue(!!radioGroup);
    assertTrue(isVisible(radioGroup));
    assertTrue(isChildVisible(page, '#locationCpssRadioGroup'));
    assertEquals(
        SettingsState.CPSS, prefService.getPref(GEOLOCATION_PREF).value);

    radioGroup.$.blockRadioOption.click();
    await microtasksFinished();
    assertFalse(isChildVisible(page, '#locationCpssRadioGroup'));

    radioGroup.$.askRadioOption.click();
    await microtasksFinished();
    assertTrue(isChildVisible(page, '#locationCpssRadioGroup'));
    assertEquals(
        SettingsState.CPSS, prefService.getPref(GEOLOCATION_PREF).value);
  });

  test('cpssRadioGroup', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.ASK));
    await microtasksFinished();

    const cpssRadioGroup =
        page.shadowRoot.querySelector('settings-radio-group');
    assertTrue(!!cpssRadioGroup);
    assertEquals(SettingsState.CPSS.toString(), cpssRadioGroup.selected);
    assertEquals(
        SettingsState.CPSS, prefService.getPref(GEOLOCATION_PREF).value);

    const quietRadioButton = cpssRadioGroup.querySelector<HTMLElement>(
        `settings-collapse-radio-button[name="${SettingsState.QUIET}"]`);
    assertTrue(!!quietRadioButton);
    quietRadioButton.click();
    await microtasksFinished();
    assertEquals(
        SettingsState.QUIET, prefService.getPref(GEOLOCATION_PREF).value);
    assertEquals(SettingsState.QUIET.toString(), cpssRadioGroup.selected);

    prefService.setPrefValue(GEOLOCATION_PREF, SettingsState.LOUD);
    await microtasksFinished();
    assertEquals(SettingsState.LOUD.toString(), cpssRadioGroup.selected);
  });
});
