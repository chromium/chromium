// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {ContentSetting, ContentSettingProvider, ContentSettingsTypes, SettingsCategoryDefaultRadioGroupElement, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertNotEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting, createSiteSettingsPrefs, SiteSettingsPref} from './test_util.js';
// clang-format on

/** @fileoverview Suite of tests for settings-category-default-radio-group. */
suite('SettingsCategoryDefaultRadioGroup', function() {
  /**
   * A settings-category-default-radio-group created before each test.
   */
  let testElement: SettingsCategoryDefaultRadioGroupElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  // Initialize a settings-category-default-radio-group before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement =
        document.createElement('settings-category-default-radio-group');
    document.body.appendChild(testElement);
  });

  teardown(function() {
    testElement.remove();
  });

  /**
   * @param category The preference category.
   * @param contentSetting The preference content setting.
   * @return The created preference object.
   */
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
  /**
   * Verifies that the widget works as expected for a given |category|,
   * initial |prefs|, and given expectations.
   * @param element The settings-category-default-radio-group element to test.
   * @param proxy The mock proxy object.
   * @param prefs The preference object.
   * @param expectedCategory The category of the |element|.
   * @param expectedEnabled If the category is enabled by default.
   * @param expectedEnabledContentSetting The enabled content setting value of
   *     the |expectedCategory|.
   */
  async function testCategoryEnabled(
      element: SettingsCategoryDefaultRadioGroupElement,
      proxy: TestSiteSettingsPrefsBrowserProxy, prefs: SiteSettingsPref,
      expectedCategory: ContentSettingsTypes, expectedEnabled: boolean,
      expectedEnabledContentSetting: ContentSetting) {
    proxy.reset();
    proxy.setPrefs(prefs);
    element.set('category', expectedCategory);

    let category = await proxy.whenCalled('getDefaultValueForContentType');
    let categoryEnabled = element.$.enabledRadioOption.checked;
    assertEquals(expectedCategory, category);
    assertEquals(expectedEnabled, categoryEnabled);

    // Click the button specifying the alternative option
    // and verify that the preference value is updated correctly.
    proxy.resetResolver('setDefaultValueForContentType');
    const oppositeRadioButton =
        expectedEnabled ? '#disabledRadioOption' : '#enabledRadioOption';
    element.shadowRoot!.querySelector<HTMLElement>(
                           oppositeRadioButton)!.click();

    let setting;
    [category, setting] =
        await proxy.whenCalled('setDefaultValueForContentType');
    assertEquals(expectedCategory, category);
    const oppositeSetting =
        expectedEnabled ? ContentSetting.BLOCK : expectedEnabledContentSetting;
    categoryEnabled = element.$.enabledRadioOption.checked;
    assertEquals(oppositeSetting, setting);
    assertNotEquals(expectedEnabled, categoryEnabled);

    // Click the initially selected option and verify that the
    // preference value is set back to the initial state.
    proxy.resetResolver('setDefaultValueForContentType');
    const initialRadioButton =
        expectedEnabled ? '#enabledRadioOption' : '#disabledRadioOption';
    element.shadowRoot!.querySelector<HTMLElement>(initialRadioButton)!.click();

    [category, setting] =
        await proxy.whenCalled('setDefaultValueForContentType');
    assertEquals(expectedCategory, category);
    const initialSetting =
        expectedEnabled ? expectedEnabledContentSetting : ContentSetting.BLOCK;
    categoryEnabled = element.$.enabledRadioOption.checked;
    assertEquals(initialSetting, setting);
    assertEquals(expectedEnabled, categoryEnabled);
  }

  test('ask location disable click triggers update', async function() {
    const enabledPref =
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.ASK);

    await testCategoryEnabled(
        testElement, browserProxy, enabledPref,
        ContentSettingsTypes.GEOLOCATION, true, ContentSetting.ASK);
  });

  test('block location enable click triggers update', async function() {
    const disabledPref =
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.BLOCK);

    await testCategoryEnabled(
        testElement, browserProxy, disabledPref,
        ContentSettingsTypes.GEOLOCATION, false, ContentSetting.ASK);
  });

  test('allow ads disable click triggers update', async function() {
    const enabledPref =
        createPref(ContentSettingsTypes.ADS, ContentSetting.ALLOW);

    await testCategoryEnabled(
        testElement, browserProxy, enabledPref, ContentSettingsTypes.ADS, true,
        ContentSetting.ALLOW);
  });

  test('block ads enable click triggers update', async function() {
    const disabledPref =
        createPref(ContentSettingsTypes.ADS, ContentSetting.BLOCK);

    await testCategoryEnabled(
        testElement, browserProxy, disabledPref, ContentSettingsTypes.ADS,
        false, ContentSetting.ALLOW);
  });


  test('radio group is disabled when pref is enforced', async function() {
    const enforcedPrefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.GEOLOCATION, createDefaultContentSetting({
              setting: ContentSetting.BLOCK,
              source: ContentSettingProvider.EXTENSION,
            }))],
        []);
    browserProxy.reset();
    browserProxy.setPrefs(enforcedPrefs);
    testElement.category = ContentSettingsTypes.GEOLOCATION;

    await browserProxy.whenCalled('getDefaultValueForContentType');
    assertTrue(testElement.$.disabledRadioOption.checked);
    assertTrue(testElement.$.enabledRadioOption.disabled);
    assertTrue(testElement.$.disabledRadioOption.disabled);

    // Stop enforcement.
    const enabledPref =
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.ASK);
    browserProxy.setPrefs(enabledPref);

    await flushTasks();
    assertTrue(testElement.$.enabledRadioOption.checked);
    assertFalse(testElement.$.enabledRadioOption.disabled);
    assertFalse(testElement.$.disabledRadioOption.disabled);
  });
});
