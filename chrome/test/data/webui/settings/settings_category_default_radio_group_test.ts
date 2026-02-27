// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SettingsCategoryDefaultRadioGroupElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, DefaultSettingSource, ContentSettingsTypes, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting, createSiteSettingsPrefs} from './test_util.js';
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
  let browserProxy: TestSiteSettingsBrowserProxy;

  // Initialize a settings-category-default-radio-group before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement =
        document.createElement('settings-category-default-radio-group');
    document.body.appendChild(testElement);
    return microtasksFinished();
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
   * Verifies that the widget works as expected for a given |expectedCategory|,
   * initial |prefs|, and given expectations.
   * @param element The settings-category-default-radio-group element to test.
   * @param proxy The mock proxy object.
   * @param prefs The preference object.
   * @param expectedCategory The category to exercise the |element| with.
   * @param expectedSupportedSettings The category default settings that should
   *     be exercised on the |element|.
   * @param expectedInitialSetting The expected initial setting.
   */
  async function testCategoryEnabled(
      element: SettingsCategoryDefaultRadioGroupElement,
      proxy: TestSiteSettingsBrowserProxy, prefs: SiteSettingsPref,
      expectedCategory: ContentSettingsTypes,
      expectedSupportedSettings: ContentSetting[],
      expectedInitialSetting: ContentSetting) {
    proxy.reset();
    proxy.setPrefs(prefs);
    element.set('category', expectedCategory);

    // Set labels for the options that should be supported and thus displayed.
    if (expectedSupportedSettings.includes(ContentSetting.ALLOW)) {
      element.allowOptionLabel = 'Allow';
    }
    if (expectedSupportedSettings.includes(ContentSetting.ASK)) {
      element.askOptionLabel = 'Ask';
    }
    if (expectedSupportedSettings.includes(ContentSetting.BLOCK)) {
      element.blockOptionLabel = 'Block';
    }

    const initialCategory =
        await proxy.whenCalled('getDefaultValueForContentType');
    await microtasksFinished();

    const radios: Partial<Record<ContentSetting, any>> = {
      [ContentSetting.ALLOW]: element.$.allowRadioOption,
      [ContentSetting.ASK]: element.$.askRadioOption,
      [ContentSetting.BLOCK]: element.$.blockRadioOption,
    };

    assertEquals(expectedCategory, initialCategory);

    for (const [setting, radio] of Object.entries(radios)) {
      assertEquals(
          expectedSupportedSettings.includes(setting as ContentSetting),
          isVisible(radio), `Initial visibility for ${setting}`);
      assertEquals(
          expectedInitialSetting === setting as ContentSetting, radio.checked,
          `Initial checked state for ${setting}`);
    }

    for (const expectedSetting of expectedSupportedSettings) {
      if (expectedSetting === expectedInitialSetting) {
        continue;
      }

      // Click the button specifying the alternative option
      // and verify that the preference value is updated correctly.
      proxy.resetResolver('setDefaultValueForContentType');
      const radioButton = radios[expectedSetting];
      radioButton.click();

      const whenChanged =
          eventToPromise('change', element.$.settingsCategoryDefaultRadioGroup);
      const selectedChangedEventPromise =
          eventToPromise('selected-value-changed', element);
      const [category, setting] =
          await proxy.whenCalled('setDefaultValueForContentType');
      await whenChanged;
      await selectedChangedEventPromise;
      assertEquals(
          expectedCategory, category,
          `Propagated category for ${expectedSetting}`);
      assertEquals(
          expectedSetting, setting, `Propagated value for ${expectedSetting}`);
      assertTrue(radioButton.checked, `Radio selected for ${expectedSetting}`);
    }
  }

  test('ask location disable click triggers update', async function() {
    const enabledPref =
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.ASK);

    await testCategoryEnabled(
        testElement, browserProxy, enabledPref,
        ContentSettingsTypes.GEOLOCATION,
        [ContentSetting.ASK, ContentSetting.BLOCK], ContentSetting.ASK);
  });

  test('block location enable click triggers update', async function() {
    const disabledPref =
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.BLOCK);

    await testCategoryEnabled(
        testElement, browserProxy, disabledPref,
        ContentSettingsTypes.GEOLOCATION,
        [ContentSetting.ASK, ContentSetting.BLOCK], ContentSetting.BLOCK);
  });

  test('allow ads disable click triggers update', async function() {
    const enabledPref =
        createPref(ContentSettingsTypes.ADS, ContentSetting.ALLOW);

    await testCategoryEnabled(
        testElement, browserProxy, enabledPref, ContentSettingsTypes.ADS,
        [ContentSetting.ALLOW, ContentSetting.BLOCK], ContentSetting.ALLOW);
  });

  test('block ads enable click triggers update', async function() {
    const disabledPref =
        createPref(ContentSettingsTypes.ADS, ContentSetting.BLOCK);

    await testCategoryEnabled(
        testElement, browserProxy, disabledPref, ContentSettingsTypes.ADS,
        [ContentSetting.ALLOW, ContentSetting.BLOCK], ContentSetting.BLOCK);
  });

  test('allow ask binary click triggers update', async function() {
    // Note that NOTIFICATIONS do not actually use an allow/ask toggle, but that
    // is not relevant for this test.
    const pref =
        createPref(ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ALLOW);

    await testCategoryEnabled(
        testElement, browserProxy, pref, ContentSettingsTypes.NOTIFICATIONS,
        [ContentSetting.ALLOW, ContentSetting.ASK], ContentSetting.ALLOW);
  });

  test('allow ask block three-state click triggers update', async function() {
    // Note that NOTIFICATIONS do not actually use an allow/ask/block toggle,
    // but that is not relevant for this test.
    const pref =
        createPref(ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ASK);

    await testCategoryEnabled(
        testElement, browserProxy, pref, ContentSettingsTypes.NOTIFICATIONS,
        [ContentSetting.ALLOW, ContentSetting.ASK, ContentSetting.BLOCK],
        ContentSetting.ASK);
  });

  test('radio group is disabled when pref is enforced', async function() {
    const enforcedPrefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.GEOLOCATION, createDefaultContentSetting({
              setting: ContentSetting.BLOCK,
              source: DefaultSettingSource.EXTENSION,
            }))],
        []);
    browserProxy.reset();
    browserProxy.setPrefs(enforcedPrefs);
    testElement.category = ContentSettingsTypes.GEOLOCATION;

    await browserProxy.whenCalled('getDefaultValueForContentType');
    // Wait for all the radio options to update checked/disabled.
    await microtasksFinished();
    assertTrue(testElement.$.blockRadioOption.checked);
    assertTrue(testElement.$.allowRadioOption.disabled);
    assertTrue(testElement.$.askRadioOption.disabled);
    assertTrue(testElement.$.blockRadioOption.disabled);

    // Stop enforcement.
    const enabledPref =
        createPref(ContentSettingsTypes.GEOLOCATION, ContentSetting.ASK);
    browserProxy.setPrefs(enabledPref);

    // Wait for all the radio options to update checked/disabled.
    await microtasksFinished();
    assertTrue(testElement.$.askRadioOption.checked);
    assertFalse(testElement.$.allowRadioOption.disabled);
    assertFalse(testElement.$.askRadioOption.disabled);
    assertFalse(testElement.$.blockRadioOption.disabled);
  });
});
