// Copyright 2016 The Chromium Authors. All rights reserved.
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
    document.body.innerHTML = '';
    testElement = document.createElement('category-default-setting');
    testElement.subOptionLabel = 'test label';
    document.body.appendChild(testElement);
  });

  test('browserProxy APIs used on startup', function() {
    const category = ContentSettingsTypes.JAVASCRIPT;
    testElement.category = category;
    return Promise
        .all([
          browserProxy.whenCalled('getDefaultValueForContentType'),
          browserProxy.whenCalled('setDefaultValueForContentType'),
        ])
        .then(args => {
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
          assertEquals(
              1, browserProxy.getCallCount('setDefaultValueForContentType'));
        });
  });

  // Verifies that the widget works as expected for a given |category|, initial
  // |prefs|, and given expectations.
  function testCategoryEnabled(
      testElement: CategoryDefaultSettingElement,
      category: ContentSettingsTypes, prefs: SiteSettingsPref,
      expectedEnabled: boolean, expectedEnabledContentSetting: ContentSetting) {
    testElement.category = category;
    browserProxy.reset();
    browserProxy.setPrefs(prefs);

    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then(function(contentType: ContentSettingsTypes) {
          assertEquals(category, contentType);
          assertEquals(expectedEnabled, testElement.categoryEnabled);
          browserProxy.resetResolver('setDefaultValueForContentType');
          testElement.$.toggle.click();
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args: ContentSettingsTypes[]) {
          assertEquals(category, args[0]);
          const oppositeSetting = expectedEnabled ?
              ContentSetting.BLOCK :
              expectedEnabledContentSetting;
          assertEquals(oppositeSetting, args[1]);
          assertNotEquals(expectedEnabled, testElement.categoryEnabled);
        });
  }

  test('categoryEnabled correctly represents prefs (enabled)', function() {
    /**
     * An example pref where the location category is enabled.
     */
    const prefsLocationEnabled: SiteSettingsPref = createSiteSettingsPrefs(
        [
          createContentSettingTypeToValuePair(
              ContentSettingsTypes.GEOLOCATION, createDefaultContentSetting({
                setting: ContentSetting.ALLOW,
              })),
        ],
        []);

    return testCategoryEnabled(
        testElement, ContentSettingsTypes.GEOLOCATION, prefsLocationEnabled,
        true, ContentSetting.ASK);
  });

  test('categoryEnabled correctly represents prefs (disabled)', function() {
    /**
     * An example pref where the location category is disabled.
     */
    const prefsLocationDisabled: SiteSettingsPref = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.GEOLOCATION, createDefaultContentSetting({
              setting: ContentSetting.BLOCK,
            }))],
        []);

    return testCategoryEnabled(
        testElement, ContentSettingsTypes.GEOLOCATION, prefsLocationDisabled,
        false, ContentSetting.ASK);
  });


  test('test content setting from extension', function() {
    testElement.category = ContentSettingsTypes.MIC;
    return browserProxy.getDefaultValueForContentType(testElement.category)
        .then((defaultValue) => {
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
          return browserProxy.whenCalled('getDefaultValueForContentType');
        })
        .then(() => {
          assertEquals(false, testElement.categoryEnabled);
        });
  });

  test('test popups content setting default value', function() {
    testElement.category = ContentSettingsTypes.POPUPS;
    return browserProxy.getDefaultValueForContentType(testElement.category)
        .then((defaultValue) => {
          assertEquals(ContentSetting.BLOCK, defaultValue.setting);
          browserProxy.resetResolver('getDefaultValueForContentType');
        });
  });

  test('test popups content setting in BLOCKED state', function() {
    const prefs: SiteSettingsPref = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.POPUPS, createDefaultContentSetting({
              setting: ContentSetting.BLOCK,
            }))],
        []);

    return testCategoryEnabled(
        testElement, ContentSettingsTypes.POPUPS, prefs, false,
        ContentSetting.ALLOW);
  });

  test('test popups content setting in ALLOWED state', function() {
    const prefs: SiteSettingsPref = createSiteSettingsPrefs(
        [createContentSettingTypeToValuePair(
            ContentSettingsTypes.POPUPS, createDefaultContentSetting({
              setting: ContentSetting.ALLOW,
            }))],
        []);

    return testCategoryEnabled(
        testElement, ContentSettingsTypes.POPUPS, prefs, true,
        ContentSetting.ALLOW);
  });
});
