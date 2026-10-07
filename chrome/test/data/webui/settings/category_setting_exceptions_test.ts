// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {CategorySettingExceptionsElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, DefaultSettingSource, ContentSettingsTypes, SiteSettingsBrowserProxyImpl} from'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import {createContentSettingTypeToValuePair,createDefaultContentSetting,createSiteSettingsPrefs} from './test_util.js';
// clang-format on

/** @fileoverview Suite of tests for category-setting-exceptions. */
suite('CategorySettingExceptions', function() {
  /**
   * A site settings exceptions created before each test.
   */
  let testElement: CategorySettingExceptionsElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsBrowserProxy;

  // Initialize a category-setting-exceptions before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('category-setting-exceptions');
    document.body.appendChild(testElement);
  });

  test('allow site list is hidden for FILE_SYSTEM_WRITE', async function() {
    testElement.category = ContentSettingsTypes.FILE_SYSTEM_WRITE;

    // Flush to be sure that the container is updated.
    await microtasksFinished();

    // Make sure that the Allow and Session Only site lists are hidden.
    const siteListElements =
        testElement.shadowRoot.querySelectorAll('site-list');
    assertEquals(3, siteListElements.length);
    siteListElements.forEach(element => {
      if (element.categorySubtype === ContentSetting.BLOCK) {
        assertFalse(
            element.hidden,
            `site-list for ${element.categorySubtype} should not be hidden`);
      } else if (element.categorySubtype === ContentSetting.ALLOW) {
        assertTrue(
            element.hidden,
            `site-list for ${element.categorySubtype} should be hidden`);
      } else {
        assertEquals(ContentSetting.SESSION_ONLY, element.categorySubtype);
        assertTrue(
            element.$.category.hidden,
            `site-list for ${element.categorySubtype} should be hidden`);
      }
    });
  });

  test('allow site list is hidden for INLINE_CUE_MENU', async function() {
    testElement.category = ContentSettingsTypes.INLINE_CUE_MENU;

    // Flush to be sure that the container is updated.
    await microtasksFinished();

    // Make sure that the Allow and Session Only site lists are hidden.
    const siteListElements =
        testElement.shadowRoot.querySelectorAll('site-list');
    assertEquals(3, siteListElements.length);
    siteListElements.forEach(element => {
      if (element.categorySubtype === ContentSetting.BLOCK) {
        assertFalse(
            element.hidden,
            `site-list for ${element.categorySubtype} should not be hidden`);
      } else if (element.categorySubtype === ContentSetting.ALLOW) {
        assertTrue(
            element.hidden,
            `site-list for ${element.categorySubtype} should be hidden`);
      } else {
        assertEquals(ContentSetting.SESSION_ONLY, element.categorySubtype);
        assertTrue(
            element.$.category.hidden,
            `site-list for ${element.categorySubtype} should be hidden`);
      }
    });
  });

  test(
      'all lists are read-only if the default policy is set by policy',
      async function() {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        const policyPref = createSiteSettingsPrefs(
            [
              createContentSettingTypeToValuePair(
                  ContentSettingsTypes.COOKIES, createDefaultContentSetting({
                    setting: ContentSetting.ALLOW,
                    source: DefaultSettingSource.POLICY,
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
        await microtasksFinished();

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
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
        await microtasksFinished();

        const siteListElements =
            testElement.shadowRoot.querySelectorAll('site-list');
        assertEquals(3, siteListElements.length);
        siteListElements.forEach(element => {
          assertTrue(!element.readOnlyList);
        });
      });
});
