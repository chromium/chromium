// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for category-setting-exceptions. */
suite('CategorySettingExceptions', function() {
  /**
   * A site settings exceptions created before each test.
   * @type {SiteSettingsExceptionsElement}
   */
  let testElement;

  // Initialize a category-setting-exceptions before each test.
  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('category-setting-exceptions');
    document.body.appendChild(testElement);
  });

  test('create category-setting-exceptions', function() {
    // The category-setting-exceptions is mainly a container for site-lists.
    // There's not much that merits testing.
    assertTrue(!!testElement);
  });

  test(
      'allow site list is hidden when showAllowSiteList_ is false', function() {
        testElement.showAllowSiteList_ = false;

        // Flush to be sure that the container is updated.
        Polymer.dom.flush();

        // Make sure that the Allow and Session Only site lists are hidden.
        const siteListElements = testElement.querySelectorAll('site-list');
        siteListElements.forEach(element => {
          if (element.categorySubtype == ContentSetting.BLOCK) {
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
        Polymer.dom.flush();

        // Make sure that the Allow and Session Only site lists are hidden.
        const siteListElements = testElement.querySelectorAll('site-list');
        siteListElements.forEach(element => {
          if (element.categorySubtype == ContentSetting.ALLOW) {
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

  test('allow site list is hidden for NATIVE_FILE_SYSTEM_WRITE', function() {
    testElement.category =
        settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE;

    // Flush to be sure that the container is updated.
    Polymer.dom.flush();

    assertFalse(
        testElement.showAllowSiteList_, 'showAllowSiteList_ should be false');
    assertTrue(
        testElement.showBlockSiteList_, 'showBlockSiteList_ should be true');

    // Make sure that the Allow and Session Only site lists are hidden.
    const siteListElements = testElement.querySelectorAll('site-list');
    siteListElements.forEach(element => {
      if (element.categorySubtype == ContentSetting.BLOCK) {
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
        const policyPref = test_util.createSiteSettingsPrefs(
            [
              test_util.createContentSettingTypeToValuePair(
                  settings.ContentSettingsTypes.COOKIES,
                  test_util.createDefaultContentSetting({
                    setting: settings.ContentSetting.ALLOW,
                    source: settings.SiteSettingSource.POLICY
                  })),
            ],
            []);
        browserProxy.reset();
        browserProxy.setPrefs(policyPref);

        // Creates a new category-setting-exceptions element to that it is
        // initialized with the right value.
        testElement = document.createElement('category-setting-exceptions');
        testElement.category = settings.ContentSettingsTypes.COOKIES;
        document.body.appendChild(testElement);

        const initializationTest =
            browserProxy.whenCalled('getDefaultValueForContentType')
                .then(function() {
                  // Flush the container to ensure that the container is
                  // populated.
                  Polymer.dom.flush();

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

        const dummyPref = test_util.createSiteSettingsPrefs(
            [
              test_util.createContentSettingTypeToValuePair(
                  settings.ContentSettingsTypes.COOKIES,
                  test_util.createDefaultContentSetting({
                    setting: settings.ContentSetting.ALLOW,
                  })),
            ],
            []);
        browserProxy.setPrefs(dummyPref);

        const updateTest =
            browserProxy.whenCalled('getDefaultValueForContentType')
                .then(function() {
                  // Flush the container to ensure that the container is
                  // populated.
                  Polymer.dom.flush();

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
