// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {CategoryDefaultSettingElement,ContentSetting,ContentSettingProvider,ContentSettingsTypes,SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair,createDefaultContentSetting,createSiteSettingsPrefs,SiteSettingsPref} from './test_util.js';
// clang-format on

/** @fileoverview Suite of tests for category-default-setting. */
suite('CategoryDefaultSetting', function() {
  /**
   * A site settings category created before each test.
   */
  let testElement: CategoryDefaultSettingElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  // Initialize a site-settings-category before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('category-default-setting');
    testElement.subOptionLabel = 'test label';
    document.body.appendChild(testElement);
  });

  test('browserProxy APIs used on startup', async function() {
    const category = ContentSettingsTypes.JAVASCRIPT;
    testElement.category = category;
    const args = await Promise.all([
      browserProxy.whenCalled('getDefaultValueForContentType'),
      browserProxy.whenCalled('setDefaultValueForContentType'),
    ]);
    // Test |getDefaultValueForContentType| args.
    assertEquals(category, args[0]);

    // Test |setDefaultValueForContentType| args. Ensure that on
    // initialization the same value returned from
    // |getDefaultValueForContentType| is passed to
    // |setDefaultValueForContentType|.
    // TODO(dpapad): Ideally 'category-default-setting' should not call
    // |setDefaultValueForContentType| on startup. Until that is fixed, at
    // least ensure that it does not accidentally change the default
    // value, crbug.com/897236.
    assertEquals(category, args[1][0]);
    assertEquals(ContentSetting.ALLOW, args[1][1]);
    assertEquals(1, browserProxy.getCallCount('setDefaultValueForContentType'));
  });

  // Verifies that the widget works as expected for a given |category|, initial
  // |prefs|, and given expectations.
  async function testCategoryEnabled(
      testElement: CategoryDefaultSettingElement,
      category: ContentSettingsTypes, prefs: SiteSettingsPref,
      expectedEnabled: boolean, expectedEnabledContentSetting: ContentSetting) {
    testElement.category = category;
    browserProxy.reset();
    browserProxy.setPrefs(prefs);

    const contentType =
        await browserProxy.whenCalled('getDefaultValueForContentType');
    assertEquals(category, contentType);
    assertEquals(expectedEnabled, testElement.categoryEnabled);
    browserProxy.resetResolver('setDefaultValueForContentType');
    testElement.$.toggle.click();
    const args = await browserProxy.whenCalled('setDefaultValueForContentType');
    assertEquals(category, args[0]);
    const oppositeSetting =
        expectedEnabled ? ContentSetting.BLOCK : expectedEnabledContentSetting;
    assertEquals(oppositeSetting, args[1]);
    assertNotEquals(expectedEnabled, testElement.categoryEnabled);
  }

  test(
      'categoryEnabled correctly represents prefs (enabled)', async function() {
        /**
         * An example pref where the location category is enabled.
         */
        const prefsLocationEnabled: SiteSettingsPref = createSiteSettingsPrefs(
            [
              createContentSettingTypeToValuePair(
                  ContentSettingsTypes.GEOLOCATION,
                  createDefaultContentSetting({
                    setting: ContentSetting.ALLOW,
                  })),
            ],
            []);

        await testCategoryEnabled(
            testElement, ContentSettingsTypes.GEOLOCATION, prefsLocationEnabled,
            true, ContentSetting.ASK);
      });

  test(
      'categoryEnabled correctly represents prefs (disabled)',
      async function() {
        /**
         * An example pref where the location category is disabled.
         */
        const prefsLocationDisabled: SiteSettingsPref = createSiteSettingsPrefs(
            [createContentSettingTypeToValuePair(
                ContentSettingsTypes.GEOLOCATION, createDefaultContentSetting({
                  setting: ContentSetting.BLOCK,
                }))],
            []);

        await testCategoryEnabled(
            testElement, ContentSettingsTypes.GEOLOCATION,
            prefsLocationDisabled, false, ContentSetting.ASK);
      });


  test('test content setting from extension', async function() {
    testElement.category = ContentSettingsTypes.MIC;
    const defaultValue =
        await browserProxy.getDefaultValueForContentType(testElement.category);
    // Sanity check - make sure the default content setting is not the
    // value the extension is about to set.
    assertEquals(ContentSetting.ASK, defaultValue.setting);
    browserProxy.resetResolver('getDefaultValueForContentType');

    const prefs = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.MIC, createDefaultContentSetting({
              setting: ContentSetting.BLOCK,
              source: ContentSettingProvider.EXTENSION,
            }))],
        []);
    browserProxy.reset();
    browserProxy.setPrefs(prefs);

    // Test that extension-enforced content settings don't override
    // user-set content settings.
    browserProxy.whenCalled('setDefaultValueForContentType').then(() => {
      assertNotReached();
    });
    await browserProxy.whenCalled('getDefaultValueForContentType');
    assertEquals(false, testElement.categoryEnabled);
  });

  test('test popups content setting default value', async function() {
    testElement.category = ContentSettingsTypes.POPUPS;
    const defaultValue =
        await browserProxy.getDefaultValueForContentType(testElement.category);
    assertEquals(ContentSetting.BLOCK, defaultValue.setting);
    browserProxy.resetResolver('getDefaultValueForContentType');
  });

  test('test popups content setting in BLOCKED state', async function() {
    const prefs: SiteSettingsPref = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.POPUPS, createDefaultContentSetting({
              setting: ContentSetting.BLOCK,
            }))],
        []);

    await testCategoryEnabled(
        testElement, ContentSettingsTypes.POPUPS, prefs, false,
        ContentSetting.ALLOW);
  });

  test('test popups content setting in ALLOWED state', async function() {
    const prefs: SiteSettingsPref = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.POPUPS, createDefaultContentSetting({
              setting: ContentSetting.ALLOW,
            }))],
        []);

    await testCategoryEnabled(
        testElement, ContentSettingsTypes.POPUPS, prefs, true,
        ContentSetting.ALLOW);
  });
});
