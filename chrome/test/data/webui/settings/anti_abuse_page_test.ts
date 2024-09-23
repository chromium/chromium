// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsAntiAbusePageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, DefaultSettingSource, ContentSettingsTypes, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertNotEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createSiteSettingsPrefs} from './test_util.js';
// clang-format on

/** @fileoverview Suite of tests for settings-anti-abuse-page. */
suite('SettingsAntiAbusePage', function() {
  /**
   * A settings-anti-abuse-page created before each test.
   */
  let testElement: SettingsAntiAbusePageElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  // Initialize a settings-anti-abuse-page before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-anti-abuse-page');
    document.body.appendChild(testElement);
  });

  /**
   * @param contentSetting The preference content setting.
   * @return The created preference object.
   */
  function createAntiAbusePref(contentSetting: ContentSetting):
      SiteSettingsPref {
    return createSiteSettingsPrefs(
        [
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.ANTI_ABUSE, {setting: contentSetting}),
        ],
        []);
  }
  /**
   * Verifies that the widget works as expected for a given |category|,
   * initial |prefs|, and given expectations.
   */
  async function testCategoryEnabled(
      element: SettingsAntiAbusePageElement,
      proxy: TestSiteSettingsPrefsBrowserProxy, prefs: SiteSettingsPref,
      expectedEnabled: boolean) {
    proxy.reset();
    proxy.setPrefs(prefs);

    const toggleElement = element.$.toggleButton;

    let category = await proxy.whenCalled('getDefaultValueForContentType');
    let categoryEnabled = toggleElement.checked;
    assertEquals(category, ContentSettingsTypes.ANTI_ABUSE);
    assertEquals(expectedEnabled, categoryEnabled);
    assertFalse(toggleElement.disabled);


    // Click the toggle and verify that the preference value is
    // updated correctly.
    proxy.resetResolver('setDefaultValueForContentType');
    toggleElement.click();

    let setting;
    [category, setting] =
        await proxy.whenCalled('setDefaultValueForContentType');

    const oppositeSetting =
        expectedEnabled ? ContentSetting.BLOCK : ContentSetting.ALLOW;
    categoryEnabled = toggleElement.checked;
    assertEquals(category, ContentSettingsTypes.ANTI_ABUSE);
    assertEquals(oppositeSetting, setting);
    assertNotEquals(expectedEnabled, categoryEnabled);

    // Click the toggle again and verify that the preference value
    // is set back to the initial state.
    proxy.resetResolver('setDefaultValueForContentType');
    toggleElement.click();

    [category, setting] =
        await proxy.whenCalled('setDefaultValueForContentType');
    const initialSetting =
        expectedEnabled ? ContentSetting.ALLOW : ContentSetting.BLOCK;
    categoryEnabled = toggleElement.checked;
    assertEquals(category, ContentSettingsTypes.ANTI_ABUSE);
    assertEquals(initialSetting, setting);
    assertEquals(expectedEnabled, categoryEnabled);
  }

  test('allow anti_abuse disable click triggers update', async function() {
    const enabledPref = createAntiAbusePref(ContentSetting.ALLOW);
    await testCategoryEnabled(testElement, browserProxy, enabledPref, true);
  });


  test('toggle is disabled when pref is enforced', async function() {
    const enforcedPrefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(ContentSettingsTypes.ANTI_ABUSE, {
          setting: ContentSetting.BLOCK,
          source: DefaultSettingSource.EXTENSION,
        })],
        []);
    browserProxy.reset();
    browserProxy.setPrefs(enforcedPrefs);
    const toggleElement = testElement.$.toggleButton;

    await browserProxy.whenCalled('getDefaultValueForContentType');
    assertFalse(toggleElement.checked);
    assertTrue(toggleElement.disabled);

    // Stop enforcement.
    const enabledPref = createAntiAbusePref(ContentSetting.ALLOW);
    browserProxy.reset();
    browserProxy.setPrefs(enabledPref);

    await flushTasks();
    assertTrue(toggleElement.checked);
    assertFalse(toggleElement.disabled);
  });
});
