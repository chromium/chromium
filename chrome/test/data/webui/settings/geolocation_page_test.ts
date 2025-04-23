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

suite(`GeolocationPage`, function() {
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

  test('locationCPSS', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW));

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_LOCATION);
    await flushTasks();
    const settingsSubpage = page.shadowRoot!.querySelector('settings-subpage');
    assertTrue(!!settingsSubpage);
    assertTrue(isVisible(settingsSubpage));

    const radioGroup = page.shadowRoot!.querySelector<HTMLElement>(
        'settings-category-default-radio-group');
    assertTrue(!!radioGroup);
    assertTrue(isVisible(radioGroup));
    assertTrue(
        isChildVisible(settingsSubpage, '#locationCpssRadioGroup', true));

    const blockLocation = radioGroup.shadowRoot!.querySelector<HTMLElement>(
        '#disabledRadioOption');
    assertTrue(!!blockLocation);
    blockLocation.click();
    await flushTasks();
    assertFalse(
        isChildVisible(settingsSubpage, '#locationCpssRadioGroup', true));
    assertEquals(
        SettingsState.BLOCK, page.get('prefs.generated.geolocation.value'));

    const allowLocation = radioGroup.shadowRoot!.querySelector<HTMLElement>(
        '#enabledRadioOption');
    assertTrue(!!allowLocation);
    allowLocation.click();
    await flushTasks();
    assertTrue(
        isChildVisible(settingsSubpage, '#locationCpssRadioGroup', true));
    assertEquals(
        SettingsState.CPSS, page.get('prefs.generated.geolocation.value'));
  });
});

// TODO(crbug.com/340743074): Remove tests after
// `PermissionSiteSettingsRadioButton` launched.
suite(`GeolocationPageWithNestedRadioButton`, function() {
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

  test('locationCPSS', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW));

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_LOCATION);
    await flushTasks();
    const settingsSubpage = page.shadowRoot!.querySelector('settings-subpage');
    assertTrue(!!settingsSubpage);
    assertTrue(isVisible(settingsSubpage));
    assertTrue(isChildVisible(page, '#locationRadioGroup'));
    assertTrue(isChildVisible(page, '#locationCpssRadioGroup'));

    const blockLocation = page.shadowRoot!.querySelector<HTMLElement>(
        '#location-block-radio-button');
    assertTrue(!!blockLocation);
    blockLocation.click();
    await flushTasks();
    assertFalse(isChildVisible(page, '#locationCpssRadioGroup'));

    const allowLocation =
        page.shadowRoot!.querySelector<HTMLElement>('#locationAskRadioButton');
    assertTrue(!!allowLocation);
    allowLocation.click();
    await flushTasks();
    assertTrue(isChildVisible(page, '#locationCpssRadioGroup'));
  });
});
