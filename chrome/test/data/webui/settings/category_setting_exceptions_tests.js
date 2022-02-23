// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting,ContentSettingProvider,ContentSettingsTypes,SiteSettingSource,SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair,createDefaultContentSetting,createSiteSettingsPrefs} from './test_util.js';
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

  // Initialize a category-setting-exceptions before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    PolymerTest.clearBody();
    testElement = document.createElement('category-setting-exceptions');
    document.body.appendChild(testElement);
  });

  test('allow site list is hidden for FILE_SYSTEM_WRITE', function() {
    testElement.category = ContentSettingsTypes.FILE_SYSTEM_WRITE;

    // Flush to be sure that the container is updated.
    flush();

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
      async function() {
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

        await browserProxy.whenCalled('getDefaultValueForContentType');
        // Flush the container to ensure that the container is populated.
        flush();

        const siteListElements =
            testElement.shadowRoot.querySelectorAll('site-list');
        assertEquals(3, siteListElements.length);
        siteListElements.forEach(element => {
          assertTrue(element.readOnlyList);
        });
      });

  test(
      'all lists are not read-only if the default policy is set by user',
      async function() {
        PolymerTest.clearBody();
        const dummyPref = createSiteSettingsPrefs(
            [
              createContentSettingTypeToValuePair(
                  ContentSettingsTypes.COOKIES, createDefaultContentSetting({
                    setting: ContentSetting.ALLOW,
                  })),
            ],
            []);
        browserProxy.reset();
        browserProxy.setPrefs(dummyPref);

        // Creates a new category-setting-exceptions element to that it is
        // initialized with the right value.
        testElement = document.createElement('category-setting-exceptions');
        testElement.category = ContentSettingsTypes.COOKIES;
        document.body.appendChild(testElement);

        await browserProxy.whenCalled('getDefaultValueForContentType');
        // Flush the container to ensure that the container is populated.
        flush();

        const siteListElements =
            testElement.shadowRoot.querySelectorAll('site-list');
        assertEquals(3, siteListElements.length);
        siteListElements.forEach(element => {
          assertTrue(!element.readOnlyList);
        });
      });
});
