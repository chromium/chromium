// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting,ContentSettingProvider,ContentSettingsTypes,SiteSettingSource,SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair,createDefaultContentSetting,createSiteSettingsPrefs} from 'chrome://test/settings/test_util.js';
import {isChildVisible} from 'chrome://test/test_util.m.js';
// clang-format on

/** @fileoverview Suite of tests for category-setting-exceptions. */
suite('CategorySettingExceptions', function() {
  /**
   * A site settings exceptions created before each test.
   * @type {SiteSettingsExceptionsElement}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableContentSettingsRedesign: false,
    });
  });

  // Initialize a category-setting-exceptions before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('category-setting-exceptions');
    document.body.appendChild(testElement);
  });

  test('create category-setting-exceptions', function() {
    // The category-setting-exceptions is mainly a container for site-lists.
    // There's not much that merits testing.
    assertTrue(!!testElement);
  });

  test('header visibility', function() {
    assertFalse(isChildVisible(testElement, '#exceptionHeader'));
  });

  test(
      'allow site list is hidden when showAllowSiteList_ is false', function() {
        testElement.showAllowSiteList_ = false;

        // Flush to be sure that the container is updated.
        flush();

        // Make sure that the Allow and Session Only site lists are hidden.
        const siteListElements = testElement.querySelectorAll('site-list');
        siteListElements.forEach(element => {
          if (element.categorySubtype === ContentSetting.BLOCK) {
            assertFalse(
                element.hidden,
                `site-list for ${
                    element.categorySubtype} should not be hidden`);
          } else {
            assertTrue(
                element.hidden,
                `site-list for ${element.categorySubtype} should be hidden`);
          }
        });
      });

  test(
      'block site list is hidden when showBlockSiteList_ is false', function() {
        testElement.showBlockSiteList_ = false;

        // Flush to be sure that the container is updated.
        flush();

        // Make sure that the Allow and Session Only site lists are hidden.
        const siteListElements = testElement.querySelectorAll('site-list');
        siteListElements.forEach(element => {
          if (element.categorySubtype === ContentSetting.ALLOW) {
            assertFalse(
                element.hidden,
                `site-list for ${
                    element.categorySubtype} should not be hidden`);
          } else {
            assertTrue(
                element.hidden,
                `site-list for ${element.categorySubtype} should be hidden`);
          }
        });
      });

  test('allow site list is hidden for FILE_SYSTEM_WRITE', function() {
    testElement.category = ContentSettingsTypes.FILE_SYSTEM_WRITE;

    // Flush to be sure that the container is updated.
    flush();

    assertFalse(
        testElement.showAllowSiteList_, 'showAllowSiteList_ should be false');
    assertTrue(
        testElement.showBlockSiteList_, 'showBlockSiteList_ should be true');

    // Make sure that the Allow and Session Only site lists are hidden.
    const siteListElements = testElement.querySelectorAll('site-list');
    siteListElements.forEach(element => {
      if (element.categorySubtype === ContentSetting.BLOCK) {
        assertFalse(
            element.hidden,
            `site-list for ${element.categorySubtype} should not be hidden`);
      } else {
        assertTrue(
            element.hidden,
            `site-list for ${element.categorySubtype} should be hidden`);
      }
    });
  });

  test(
      'all lists are read-only if the default policy is set by policy',
      function() {
        PolymerTest.clearBody();
        const policyPref = createSiteSettingsPrefs(
            [
              createContentSettingTypeToValuePair(
                  ContentSettingsTypes.COOKIES, createDefaultContentSetting({
                    setting: ContentSetting.ALLOW,
                    source: SiteSettingSource.POLICY
                  })),
            ],
            []);
        browserProxy.reset();
        browserProxy.setPrefs(policyPref);

        // Creates a new category-setting-exceptions element to that it is
        // initialized with the right value.
        testElement = document.createElement('category-setting-exceptions');
        testElement.category = ContentSettingsTypes.COOKIES;
        document.body.appendChild(testElement);

        const initializationTest =
            browserProxy.whenCalled('getDefaultValueForContentType')
                .then(function() {
                  // Flush the container to ensure that the container is
                  // populated.
                  flush();

                  assertTrue(testElement.getReadOnlyList_());
                  assertTrue(testElement.defaultManaged_);

                  // Make sure that the Allow and Session Only site lists are
                  // hidden.
                  const siteListElements =
                      testElement.shadowRoot.querySelectorAll('site-list');
                  siteListElements.forEach(element => {
                    assertTrue(!!element.readOnlyList);
                  });
                });

        const dummyPref = createSiteSettingsPrefs(
            [
              createContentSettingTypeToValuePair(
                  ContentSettingsTypes.COOKIES, createDefaultContentSetting({
                    setting: ContentSetting.ALLOW,
                  })),
            ],
            []);
        browserProxy.setPrefs(dummyPref);

        const updateTest =
            browserProxy.whenCalled('getDefaultValueForContentType')
                .then(function() {
                  // Flush the container to ensure that the container is
                  // populated.
                  flush();

                  assertFalse(testElement.getReadOnlyList_());
                  assertFalse(testElement.defaultManaged_);

                  // Make sure that the Allow and Session Only site lists are
                  // hidden.
                  const siteListElements =
                      testElement.shadowRoot.querySelectorAll('site-list');
                  siteListElements.forEach(element => {
                    assertTrue(!element.readOnlyList);
                  });
                });
        return Promise.all([initializationTest, updateTest]);
      });
});

suite('ContentSettingsRedesign', function() {
  /**
   * A site settings exceptions created before each test.
   * @type {SiteSettingsExceptionsElement}
   */
  let testElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableContentSettingsRedesign: true,
    });
  });

  // Initialize a category-setting-exceptions before each test.
  setup(function() {
    PolymerTest.clearBody();
    testElement = document.createElement('category-setting-exceptions');
    document.body.appendChild(testElement);
  });

  test('header visibility', function() {
    assertTrue(isChildVisible(testElement, '#exceptionHeader'));
  });
});
