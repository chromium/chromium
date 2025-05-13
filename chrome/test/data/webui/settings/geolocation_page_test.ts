// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {GeolocationPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsPrefsBrowserProxyImpl, SettingsState} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
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
  let page: GeolocationPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let siteSettingsBrowserProxy: TestSiteSettingsPrefsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    page = document.createElement('settings-geolocation-page');
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

  test('LocationPage', function() {
    assertTrue(isChildVisible(page, '#locationDefaultRadioGroup'));
    const categorySettingExceptions =
        page.shadowRoot!.querySelector('category-setting-exceptions');
    assertTrue(!!categorySettingExceptions);
    assertTrue(isVisible(categorySettingExceptions));
    assertEquals(
        ContentSettingsTypes.GEOLOCATION, categorySettingExceptions.category);
  });

  test('locationCPSS', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW));

    const radioGroup = page.shadowRoot!.querySelector<HTMLElement>(
        'settings-category-default-radio-group');
    assertTrue(!!radioGroup);
    assertTrue(isVisible(radioGroup));
    assertTrue(isChildVisible(page, '#locationCpssRadioGroup'));

    const blockLocation = radioGroup.shadowRoot!.querySelector<HTMLElement>(
        '#disabledRadioOption');
    assertTrue(!!blockLocation);
    blockLocation.click();
    await flushTasks();
    assertFalse(isChildVisible(page, '#locationCpssRadioGroup'));
    assertEquals(
        SettingsState.BLOCK, page.get('prefs.generated.geolocation.value'));

    const allowLocation = radioGroup.shadowRoot!.querySelector<HTMLElement>(
        '#enabledRadioOption');
    assertTrue(!!allowLocation);
    allowLocation.click();
    await flushTasks();
    assertTrue(isChildVisible(page, '#locationCpssRadioGroup'));
    assertEquals(
        SettingsState.CPSS, page.get('prefs.generated.geolocation.value'));
  });
});

// TODO(crbug.com/340743074): Remove tests after
// `PermissionSiteSettingsRadioButton` launched.
suite(`GeolocationPageWithNestedRadioButton`, function() {
  let page: GeolocationPageElement;
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
    page = document.createElement('settings-geolocation-page');
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

  test('LocationPage', function() {
    assertTrue(isChildVisible(page, '#locationRadioGroup'));
    const categorySettingExceptions =
        page.shadowRoot!.querySelector('category-setting-exceptions');
    assertTrue(!!categorySettingExceptions);
    assertTrue(isVisible(categorySettingExceptions));
    assertEquals(
        ContentSettingsTypes.GEOLOCATION, categorySettingExceptions.category);
  });

  test('locationCPSS', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW));

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
