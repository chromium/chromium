// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for category-default-setting. */
suite('CategoryDefaultSetting', function() {
  /**
   * A site settings category created before each test.
   * @type {SiteSettingsCategory}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  // Initialize a site-settings-category before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('category-default-setting');
    testElement.subOptionLabel = 'test label';
    document.body.appendChild(testElement);
  });

  test('browserProxy APIs used on startup', function() {
    const category = settings.ContentSettingsTypes.COOKIES;
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
          assertEquals(settings.ContentSetting.ALLOW, args[1][1]);
          assertEquals(
              1, browserProxy.getCallCount('setDefaultValueForContentType'));
        });
  });

  // Verifies that the widget works as expected for a given |category|, initial
  // |prefs|, and given expectations.
  function testCategoryEnabled(
      testElement, category, prefs, expectedEnabled,
      expectedEnabledContentSetting) {
    testElement.category = category;
    browserProxy.reset();
    browserProxy.setPrefs(prefs);

    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then(function(contentType) {
          assertEquals(category, contentType);
          assertEquals(expectedEnabled, testElement.categoryEnabled);
          browserProxy.resetResolver('setDefaultValueForContentType');
          testElement.$.toggle.click();
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          assertEquals(category, args[0]);
          const oppositeSetting = expectedEnabled ?
              settings.ContentSetting.BLOCK :
              expectedEnabledContentSetting;
          assertEquals(oppositeSetting, args[1]);
          assertNotEquals(expectedEnabled, testElement.categoryEnabled);
        });
  }

  test('categoryEnabled correctly represents prefs (enabled)', function() {
    /**
     * An example pref where the location category is enabled.
     * @type {SiteSettingsPref}
     */
    const prefsLocationEnabled = test_util.createSiteSettingsPrefs(
        [
          test_util.createContentSettingTypeToValuePair(
              settings.ContentSettingsTypes.GEOLOCATION,
              test_util.createDefaultContentSetting({
                setting: settings.ContentSetting.ALLOW,
              })),
        ],
        []);

    return testCategoryEnabled(
        testElement, settings.ContentSettingsTypes.GEOLOCATION,
        prefsLocationEnabled, true, settings.ContentSetting.ASK);
  });

  test('categoryEnabled correctly represents prefs (disabled)', function() {
    /**
     * An example pref where the location category is disabled.
     * @type {SiteSettingsPref}
     */
    const prefsLocationDisabled = test_util.createSiteSettingsPrefs(
        [test_util.createContentSettingTypeToValuePair(
            settings.ContentSettingsTypes.GEOLOCATION,
            test_util.createDefaultContentSetting({
              setting: settings.ContentSetting.BLOCK,
            }))],
        []);

    return testCategoryEnabled(
        testElement, settings.ContentSettingsTypes.GEOLOCATION,
        prefsLocationDisabled, false, settings.ContentSetting.ASK);
  });

  test('test Flash content setting in DETECT/ASK setting', function() {
    const prefsFlash = test_util.createSiteSettingsPrefs(
        [test_util.createContentSettingTypeToValuePair(
            settings.ContentSettingsTypes.PLUGINS,
            test_util.createDefaultContentSetting({
              setting: settings.ContentSetting.IMPORTANT_CONTENT,
            }))],
        []);

    return testCategoryEnabled(
        testElement, settings.ContentSettingsTypes.PLUGINS, prefsFlash, true,
        settings.ContentSetting.IMPORTANT_CONTENT);
  });

  test('test Flash content setting in legacy ALLOW setting', function() {
    const prefsFlash = test_util.createSiteSettingsPrefs(
        [test_util.createContentSettingTypeToValuePair(
            settings.ContentSettingsTypes.PLUGINS,
            test_util.createDefaultContentSetting({
              setting: settings.ContentSetting.ALLOW,
            }))],
        []);

    return testCategoryEnabled(
        testElement, settings.ContentSettingsTypes.PLUGINS, prefsFlash, true,
        settings.ContentSetting.IMPORTANT_CONTENT);
  });

  test('test Flash content setting in BLOCK setting', function() {
    const prefsFlash = test_util.createSiteSettingsPrefs(
        [test_util.createContentSettingTypeToValuePair(
            settings.ContentSettingsTypes.PLUGINS,
            test_util.createDefaultContentSetting({
              setting: settings.ContentSetting.BLOCK,
            }))],
        []);

    return testCategoryEnabled(
        testElement, settings.ContentSettingsTypes.PLUGINS, prefsFlash, false,
        settings.ContentSetting.IMPORTANT_CONTENT);
  });

  test('test content setting from extension', function() {
    testElement.category = settings.ContentSettingsTypes.MIC;
    return browserProxy.getDefaultValueForContentType(testElement.category)
        .then((defaultValue) => {
          // Sanity check - make sure the default content setting is not the
          // value the extension is about to set.
          assertEquals(settings.ContentSetting.ASK, defaultValue.setting);
          browserProxy.resetResolver('getDefaultValueForContentType');

          const prefs = test_util.createSiteSettingsPrefs(
              [test_util.createContentSettingTypeToValuePair(
                  settings.ContentSettingsTypes.MIC,
                  test_util.createDefaultContentSetting({
                    setting: settings.ContentSetting.BLOCK,
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

  function testTristateCategory(
      prefs, category, thirdState, secondaryToggleId) {
    testElement.category = category;
    browserProxy.setPrefs(prefs);

    let secondaryToggle = null;

    return browserProxy.whenCalled('getDefaultValueForContentType')
        .then(function(contentType) {
          Polymer.dom.flush();
          secondaryToggle = testElement.$$(secondaryToggleId);
          assertTrue(!!secondaryToggle);

          assertEquals(category, contentType);
          assertTrue(testElement.categoryEnabled);
          assertFalse(secondaryToggle.disabled);
          assertTrue(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          testElement.$.toggle.click();
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check THIRD_STATE => BLOCK transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(settings.ContentSetting.BLOCK, args[1]);
          assertFalse(testElement.categoryEnabled);
          assertTrue(secondaryToggle.disabled);
          assertTrue(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          testElement.$.toggle.click();
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check BLOCK => THIRD_STATE transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(thirdState, args[1]);
          assertTrue(testElement.categoryEnabled);
          assertFalse(secondaryToggle.disabled);
          assertTrue(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          secondaryToggle.click();
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check THIRD_STATE => ALLOW transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(settings.ContentSetting.ALLOW, args[1]);
          assertTrue(testElement.categoryEnabled);
          assertFalse(secondaryToggle.disabled);
          assertFalse(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          testElement.$.toggle.click();
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check ALLOW => BLOCK transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(settings.ContentSetting.BLOCK, args[1]);
          assertFalse(testElement.categoryEnabled);
          assertTrue(secondaryToggle.disabled);
          assertFalse(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          testElement.$.toggle.click();
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check BLOCK => ALLOW transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(settings.ContentSetting.ALLOW, args[1]);
          assertTrue(testElement.categoryEnabled);
          assertFalse(secondaryToggle.disabled);
          assertFalse(secondaryToggle.checked);

          browserProxy.resetResolver('setDefaultValueForContentType');
          secondaryToggle.click();
          return browserProxy.whenCalled('setDefaultValueForContentType');
        })
        .then(function(args) {
          // Check ALLOW => THIRD_STATE transition succeeded.
          Polymer.dom.flush();

          assertEquals(category, args[0]);
          assertEquals(thirdState, args[1]);
          assertTrue(testElement.categoryEnabled);
          assertFalse(secondaryToggle.disabled);
          assertTrue(secondaryToggle.checked);
        });
  }

  test('test special tri-state Cookies category', function() {
    /**
     * An example pref where the Cookies category is set to delete when
     * session ends.
     */
    const prefsCookiesSessionOnly = test_util.createSiteSettingsPrefs(
        [test_util.createContentSettingTypeToValuePair(
            settings.ContentSettingsTypes.COOKIES,
            test_util.createDefaultContentSetting({
              setting: settings.ContentSetting.SESSION_ONLY,
            }))],
        []);

    return testTristateCategory(
        prefsCookiesSessionOnly, settings.ContentSettingsTypes.COOKIES,
        settings.ContentSetting.SESSION_ONLY, '#subOptionToggle');
  });
});
