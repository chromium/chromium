// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list. */

/**
 * An example pref with 2 blocked location items and 2 allowed. This pref
 * is also used for the All Sites category and therefore needs values for
 * all types, even though some might be blank.
 * @type {SiteSettingsPref}
 */
let prefsGeolocation;

/**
 * An example of prefs controlledBy policy.
 * @type {SiteSettingsPref}
 */
let prefsControlled;

/**
 * An example pref with mixed schemes (present and absent).
 * @type {SiteSettingsPref}
 */
let prefsMixedSchemes;

/**
 * An example pref with exceptions with origins and patterns from
 * different providers.
 * @type {SiteSettingsPref}
 */
let prefsMixedProvider;

/**
 * An example pref with with and without embeddingOrigin.
 * @type {SiteSettingsPref}
 */
let prefsMixedEmbeddingOrigin;

/**
 * An example pref with multiple categories and multiple allow/block
 * state.
 * @type {SiteSettingsPref}
 */
let prefsVarious;

/**
 * An example pref with 1 allowed location item.
 * @type {SiteSettingsPref}
 */
let prefsOneEnabled;

/**
 * An example pref with 1 blocked location item.
 * @type {SiteSettingsPref}
 */
let prefsOneDisabled;

/**
 * An example Cookies pref with 1 in each of the three categories.
 * @type {SiteSettingsPref}
 */
let prefsSessionOnly;

/**
 * An example Cookies pref with mixed incognito and regular settings.
 * @type {SiteSettingsPref}
 */
let prefsIncognito;

/**
 * An example Javascript pref with a chrome-extension:// scheme.
 * @type {SiteSettingsPref}
 */
let prefsChromeExtension;

/**
 * An example Javascript pref for android_sms notification setting.
 * @type {SiteSettingsPref}
 */
let prefsAndroidSms;

/**
 * Creates all the test |SiteSettingsPref|s that are needed for the tests in
 * this file. They are populated after test setup in order to access the
 * |settings| constants required.
 */
function populateTestExceptions() {
  prefsGeolocation = test_util.createSiteSettingsPrefs([], [
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.GEOLOCATION,
        [
          test_util.createRawSiteException('https://bar-allow.com:443'),
          test_util.createRawSiteException('https://foo-allow.com:443'),
          test_util.createRawSiteException('https://bar-block.com:443', {
            setting: settings.ContentSetting.BLOCK,
          }),
          test_util.createRawSiteException('https://foo-block.com:443', {
            setting: settings.ContentSetting.BLOCK,
          })
        ]),
  ]);

  prefsControlled = test_util.createSiteSettingsPrefs(
      [], [test_util.createContentSettingTypeToValuePair(
              settings.ContentSettingsTypes.PLUGINS,
              [test_util.createRawSiteException('http://foo-block.com', {
                embeddingOrigin: '',
                setting: settings.ContentSetting.BLOCK,
                source: settings.SiteSettingSource.POLICY,
              })])]);

  prefsMixedSchemes = test_util.createSiteSettingsPrefs([], [
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.GEOLOCATION,
        [
          test_util.createRawSiteException('https://foo-allow.com', {
            source: settings.SiteSettingSource.POLICY,
          }),
          test_util.createRawSiteException('bar-allow.com'),
        ]),
  ]);

  prefsMixedProvider = test_util.createSiteSettingsPrefs([], [
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.GEOLOCATION,
        [
          test_util.createRawSiteException('https://[*.]foo.com', {
            setting: settings.ContentSetting.BLOCK,
            source: settings.SiteSettingSource.POLICY,
          }),
          test_util.createRawSiteException('https://bar.foo.com', {
            setting: settings.ContentSetting.BLOCK,
            source: settings.SiteSettingSource.POLICY,
          }),
          test_util.createRawSiteException('https://[*.]foo.com', {
            setting: settings.ContentSetting.BLOCK,
            source: settings.SiteSettingSource.POLICY,
          })
        ]),
  ]);

  prefsMixedEmbeddingOrigin = test_util.createSiteSettingsPrefs([], [
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.IMAGES,
        [
          test_util.createRawSiteException('https://foo.com', {
            embeddingOrigin: 'https://example.com',
          }),
          test_util.createRawSiteException('https://bar.com', {
            embeddingOrigin: '',
          })
        ]),
  ]);

  prefsVarious = test_util.createSiteSettingsPrefs([], [
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.GEOLOCATION,
        [
          test_util.createRawSiteException('https://foo.com', {
            embeddingOrigin: '',
          }),
          test_util.createRawSiteException('https://bar.com', {
            embeddingOrigin: '',
          })
        ]),
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.NOTIFICATIONS,
        [
          test_util.createRawSiteException('https://google.com', {
            embeddingOrigin: '',
          }),
          test_util.createRawSiteException('https://bar.com', {
            embeddingOrigin: '',
          }),
          test_util.createRawSiteException('https://foo.com', {
            embeddingOrigin: '',
          })
        ]),
  ]);

  prefsOneEnabled = test_util.createSiteSettingsPrefs([], [
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.GEOLOCATION,
        [test_util.createRawSiteException('https://foo-allow.com:443', {
          embeddingOrigin: '',
        })]),
  ]);

  prefsOneDisabled = test_util.createSiteSettingsPrefs([], [
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.GEOLOCATION,
        [test_util.createRawSiteException('https://foo-block.com:443', {
          embeddingOrigin: '',
          setting: settings.ContentSetting.BLOCK,
        })]),
  ]);

  prefsSessionOnly = test_util.createSiteSettingsPrefs([], [
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.COOKIES,
        [
          test_util.createRawSiteException('http://foo-block.com', {
            embeddingOrigin: '',
            setting: settings.ContentSetting.BLOCK,
          }),
          test_util.createRawSiteException('http://foo-allow.com', {
            embeddingOrigin: '',
          }),
          test_util.createRawSiteException('http://foo-session.com', {
            embeddingOrigin: '',
            setting: settings.ContentSetting.SESSION_ONLY,
          })
        ]),
  ]);

  prefsIncognito = test_util.createSiteSettingsPrefs([], [
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.COOKIES,
        [
          // foo.com is blocked for regular sessions.
          test_util.createRawSiteException('http://foo.com', {
            embeddingOrigin: '',
            setting: settings.ContentSetting.BLOCK,
          }),
          // bar.com is an allowed incognito item.
          test_util.createRawSiteException('http://bar.com', {
            embeddingOrigin: '',
            incognito: true,
          }),
          // foo.com is allowed in incognito (overridden).
          test_util.createRawSiteException('http://foo.com', {
            embeddingOrigin: '',
            incognito: true,
          })
        ]),
  ]);

  prefsChromeExtension = test_util.createSiteSettingsPrefs([], [
    test_util.createContentSettingTypeToValuePair(
        settings.ContentSettingsTypes.JAVASCRIPT,
        [test_util.createRawSiteException(
            'chrome-extension://cfhgfbfpcbnnbibfphagcjmgjfjmojfa/', {
              embeddingOrigin: '',
              setting: settings.ContentSetting.BLOCK,
            })]),
  ]);

  prefsGeolocationEmpty = test_util.createSiteSettingsPrefs([], []);

  prefsAndroidSms = test_util.createSiteSettingsPrefs(
      [], [test_util.createContentSettingTypeToValuePair(
              settings.ContentSettingsTypes.NOTIFICATIONS, [
                // android sms setting.
                test_util.createRawSiteException(
                    multidevice.TEST_ANDROID_SMS_ORIGIN),
                // Non android sms setting that should be handled as usual.
                test_util.createRawSiteException('http://bar.com')
              ])]);

  prefsNativeFileSystemWrite = test_util.createSiteSettingsPrefs(
      [], [test_util.createContentSettingTypeToValuePair(
              settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE,
              [test_util.createRawSiteException('http://foo.com', {
                setting: settings.ContentSetting.BLOCK,
              })])]);
}

suite('SiteList', function() {
  /**
   * A site list element created before each test.
   * @type {SiteList}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  /**
   * Mock MultiDeviceBrowserProxy to use during test.
   * @type {TestMultideviceBrowserProxy}
   */
  let multiDeviceBrowserProxy = null;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  // Initialize a site-list before each test.
  setup(function() {
    populateTestExceptions();

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('site-list');
    testElement.searchFilter = '';
    document.body.appendChild(testElement);

    if (cr.isChromeOS) {
      multiDeviceBrowserProxy = new multidevice.TestMultideviceBrowserProxy();
      settings.MultiDeviceBrowserProxyImpl.instance_ = multiDeviceBrowserProxy;
    }
  });

  teardown(function() {
    closeActionMenu();
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    settings.resetRouteForTesting();

    if (cr.isChromeOS) {
      // Reset multidevice enabled flag.
      loadTimeData.overrideValues({
        multideviceAllowedByPolicy: false
      });
    }
  });

  /**
   * Opens the action menu for a particular element in the list.
   * @param {number} index The index of the child element (which site) to
   *     open the action menu for.
   */
  function openActionMenu(index) {
    const actionMenuButton =
        testElement.$.listContainer.querySelectorAll('site-list-entry')[index]
            .$.actionMenuButton;
    actionMenuButton.click();
    Polymer.dom.flush();
  }

  /** Closes the action menu. */
  function closeActionMenu() {
    const menu = testElement.$$('cr-action-menu');
    if (menu.open) {
      menu.close();
    }
  }

  /**
   * Asserts the menu looks as expected.
   * @param {Array<string>} items The items expected to show in the menu.
   */
  function assertMenu(items) {
    const menu = testElement.$$('cr-action-menu');
    assertTrue(!!menu);
    const menuItems = menu.querySelectorAll('button:not([hidden])');
    assertEquals(items.length, menuItems.length);
    for (let i = 0; i < items.length; i++) {
      assertEquals(items[i], menuItems[i].textContent.trim());
    }
  }

  /**
   * @param {HTMLElement} listContainer Node with the exceptions listed.
   * @return {boolean} Whether the entry is incognito only.
   */
  function hasAnIncognito(listContainer) {
    return listContainer.querySelector('iron-list')
        .items.some(item => item.incognito);
  }

  /**
   * Configures the test element for a particular category.
   * @param {settings.ContentSettingsTypes} category The category to set up.
   * @param {settings.ContentSetting} subtype Type of list to use.
   * @param {Array<dictionary>} prefs The prefs to use.
   */
  function setUpCategory(category, subtype, prefs) {
    browserProxy.setPrefs(prefs);
    testElement.categorySubtype = subtype;
    // Some route is needed, but the actual route doesn't matter.
    testElement.currentRoute = {
      page: 'dummy',
      section: 'privacy',
      subpage: ['site-settings', 'site-settings-category-location'],
    };
    testElement.category = category;
  }

  test('read-only attribute', function() {
    setUpCategory(
        settings.ContentSettingsTypes.GEOLOCATION,
        settings.ContentSetting.ALLOW, prefsVarious);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          // Flush to be sure list container is populated.
          Polymer.dom.flush();
          const dotsMenu =
              testElement.$$('site-list-entry').$$('#actionMenuButton');
          assertFalse(dotsMenu.hidden);
          testElement.setAttribute('read-only-list', true);
          Polymer.dom.flush();
          assertTrue(dotsMenu.hidden);
          testElement.removeAttribute('read-only-list');
          Polymer.dom.flush();
          assertFalse(dotsMenu.hidden);
        });
  });

  if (cr.isChromeOS) {
    test('update androidSmsInfo', function() {
      setUpCategory(
          settings.ContentSettingsTypes.NOTIFICATIONS,
          settings.ContentSetting.ALLOW, prefsAndroidSms);
      assertEquals(
          0, multiDeviceBrowserProxy.getCallCount('getAndroidSmsInfo'));

      loadTimeData.overrideValues({multideviceAllowedByPolicy: true});
      setUpCategory(
          settings.ContentSettingsTypes.NOTIFICATIONS,
          settings.ContentSetting.ALLOW, prefsAndroidSms);
      // Assert 2 calls since the observer observes 2 properties.
      assertEquals(
          2, multiDeviceBrowserProxy.getCallCount('getAndroidSmsInfo'));

      return multiDeviceBrowserProxy.whenCalled('getAndroidSmsInfo')
          .then(() => browserProxy.whenCalled('getExceptionList'))
          .then((contentType) => {
            assertEquals(
                settings.ContentSettingsTypes.NOTIFICATIONS, contentType);
            assertEquals(2, testElement.sites.length);

            assertEquals(
                prefsAndroidSms.exceptions[contentType][0].origin,
                testElement.sites[0].origin);
            assertTrue(testElement.sites[0].showAndroidSmsNote);

            assertEquals(
                prefsAndroidSms.exceptions[contentType][1].origin,
                testElement.sites[1].origin);
            assertEquals(undefined, testElement.sites[1].showAndroidSmsNote);

            browserProxy.resetResolver('getExceptionList');
            multiDeviceBrowserProxy.setFeatureEnabledState(
                settings.MultiDeviceFeature.MESSAGES, false);
            return browserProxy.whenCalled('getExceptionList');
          })
          .then((contentType) => {
            assertEquals(
                settings.ContentSettingsTypes.NOTIFICATIONS, contentType);
            assertEquals(2, testElement.sites.length);

            assertEquals(
                prefsAndroidSms.exceptions[contentType][0].origin,
                testElement.sites[0].origin);
            assertEquals(undefined, testElement.sites[0].showAndroidSmsNote);

            assertEquals(
                prefsAndroidSms.exceptions[contentType][1].origin,
                testElement.sites[1].origin);
            assertEquals(undefined, testElement.sites[1].showAndroidSmsNote);
          });
    });
  }

  test('getExceptionList API used', function() {
    setUpCategory(
        settings.ContentSettingsTypes.GEOLOCATION,
        settings.ContentSetting.ALLOW, prefsGeolocationEmpty);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          assertEquals(settings.ContentSettingsTypes.GEOLOCATION, contentType);
        });
  });

  test('Empty list', function() {
    setUpCategory(
        settings.ContentSettingsTypes.GEOLOCATION,
        settings.ContentSetting.ALLOW, prefsGeolocationEmpty);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          assertEquals(settings.ContentSettingsTypes.GEOLOCATION, contentType);

          assertEquals(0, testElement.sites.length);

          assertEquals(
              settings.ContentSetting.ALLOW, testElement.categorySubtype);

          assertFalse(testElement.$.category.hidden);
        });
  });

  test('initial ALLOW state is correct', function() {
    setUpCategory(
        settings.ContentSettingsTypes.GEOLOCATION,
        settings.ContentSetting.ALLOW, prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          assertEquals(settings.ContentSettingsTypes.GEOLOCATION, contentType);

          assertEquals(2, testElement.sites.length);
          assertEquals(
              prefsGeolocation.exceptions[contentType][0].origin,
              testElement.sites[0].origin);
          assertEquals(
              prefsGeolocation.exceptions[contentType][1].origin,
              testElement.sites[1].origin);
          assertEquals(
              settings.ContentSetting.ALLOW, testElement.categorySubtype);
          Polymer.dom.flush();  // Populates action menu.
          openActionMenu(0);
          assertMenu(['Block', 'Edit', 'Remove'], testElement);

          assertFalse(testElement.$.category.hidden);
        });
  });

  test('action menu closes when list changes', function() {
    setUpCategory(
        settings.ContentSettingsTypes.GEOLOCATION,
        settings.ContentSetting.ALLOW, prefsGeolocation);
    const actionMenu = testElement.$$('cr-action-menu');
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          Polymer.dom.flush();  // Populates action menu.
          openActionMenu(0);
          assertTrue(actionMenu.open);

          browserProxy.resetResolver('getExceptionList');
          // Simulate a change in the underlying model.
          cr.webUIListenerCallback(
              'contentSettingSitePermissionChanged',
              settings.ContentSettingsTypes.GEOLOCATION);
          return browserProxy.whenCalled('getExceptionList');
        })
        .then(function() {
          // Check that the action menu was closed.
          assertFalse(actionMenu.open);
        });
  });

  test('exceptions are not reordered in non-ALL_SITES', function() {
    setUpCategory(
        settings.ContentSettingsTypes.GEOLOCATION,
        settings.ContentSetting.BLOCK, prefsMixedProvider);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          assertEquals(settings.ContentSettingsTypes.GEOLOCATION, contentType);
          assertEquals(3, testElement.sites.length);
          for (let i = 0; i < testElement.sites.length; ++i) {
            assertEquals(
                prefsMixedProvider.exceptions[contentType][i].origin,
                testElement.sites[i].origin);
            assertEquals(
                kControlledByLookup[prefsMixedProvider
                                        .exceptions[contentType][i]
                                        .source] ||
                    chrome.settingsPrivate.ControlledBy.PRIMARY_USER,
                testElement.sites[i].controlledBy);
          }
        });
  });

  test('initial BLOCK state is correct', function() {
    const contentType = settings.ContentSettingsTypes.GEOLOCATION;
    const categorySubtype = settings.ContentSetting.BLOCK;
    setUpCategory(contentType, categorySubtype, prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertEquals(categorySubtype, testElement.categorySubtype);

          assertEquals(2, testElement.sites.length);
          assertEquals(
              prefsGeolocation.exceptions[contentType][2].origin,
              testElement.sites[0].origin);
          assertEquals(
              prefsGeolocation.exceptions[contentType][3].origin,
              testElement.sites[1].origin);
          Polymer.dom.flush();  // Populates action menu.
          openActionMenu(0);
          assertMenu(['Allow', 'Edit', 'Remove'], testElement);

          assertFalse(testElement.$.category.hidden);
        });
  });

  test('initial SESSION ONLY state is correct', function() {
    const contentType = settings.ContentSettingsTypes.COOKIES;
    const categorySubtype = settings.ContentSetting.SESSION_ONLY;
    setUpCategory(contentType, categorySubtype, prefsSessionOnly);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertEquals(categorySubtype, testElement.categorySubtype);

          assertEquals(1, testElement.sites.length);
          assertEquals(
              prefsSessionOnly.exceptions[contentType][2].origin,
              testElement.sites[0].origin);

          Polymer.dom.flush();  // Populates action menu.
          openActionMenu(0);
          assertMenu(['Allow', 'Block', 'Edit', 'Remove'], testElement);

          assertFalse(testElement.$.category.hidden);
        });
  });

  test('update lists for incognito', function() {
    const contentType = settings.ContentSettingsTypes.PLUGINS;
    const categorySubtype = settings.ContentSetting.BLOCK;
    setUpCategory(contentType, categorySubtype, prefsControlled);
    const list = testElement.$.listContainer;
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          Polymer.dom.flush();
          assertEquals(1, list.querySelector('iron-list').items.length);
          assertFalse(hasAnIncognito(list));
          browserProxy.resetResolver('getExceptionList');
          browserProxy.setIncognito(true);
          return browserProxy.whenCalled('getExceptionList');
        })
        .then(function() {
          Polymer.dom.flush();
          assertEquals(2, list.querySelector('iron-list').items.length);
          assertTrue(hasAnIncognito(list));
          assertTrue(Array.from(list.querySelectorAll('site-list-entry'))
                         .some(
                             entry => entry.$.siteDescription.textContent ==
                                 'Current incognito session'));
          browserProxy.resetResolver('getExceptionList');
          browserProxy.setIncognito(false);
          return browserProxy.whenCalled('getExceptionList');
        })
        .then(function() {
          Polymer.dom.flush();
          assertEquals(1, list.querySelector('iron-list').items.length);
          assertFalse(hasAnIncognito(list));
          browserProxy.resetResolver('getExceptionList');
          browserProxy.setIncognito(true);
          return browserProxy.whenCalled('getExceptionList');
        })
        .then(function() {
          Polymer.dom.flush();
          assertEquals(2, list.querySelector('iron-list').items.length);
          assertTrue(hasAnIncognito(list));
        });
  });

  test('initial INCOGNITO BLOCK state is correct', function() {
    const contentType = settings.ContentSettingsTypes.COOKIES;
    const categorySubtype = settings.ContentSetting.BLOCK;
    setUpCategory(contentType, categorySubtype, prefsIncognito);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertEquals(categorySubtype, testElement.categorySubtype);

          assertEquals(1, testElement.sites.length);
          assertEquals(
              prefsIncognito.exceptions[contentType][0].origin,
              testElement.sites[0].origin);

          Polymer.dom.flush();  // Populates action menu.
          openActionMenu(0);
          // 'Clear on exit' is visible as this is not an incognito item.
          assertMenu(['Allow', 'Clear on exit', 'Edit', 'Remove'], testElement);

          // Select 'Remove' from menu.
          const remove = testElement.$.reset;
          assertTrue(!!remove);
          remove.click();
          return browserProxy.whenCalled('resetCategoryPermissionForPattern');
        })
        .then(function(args) {
          assertEquals('http://foo.com', args[0]);
          assertEquals('', args[1]);
          assertEquals(contentType, args[2]);
          assertFalse(args[3]);  // Incognito.
        });
  });

  test('initial INCOGNITO ALLOW state is correct', function() {
    const contentType = settings.ContentSettingsTypes.COOKIES;
    const categorySubtype = settings.ContentSetting.ALLOW;
    setUpCategory(contentType, categorySubtype, prefsIncognito);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertEquals(categorySubtype, testElement.categorySubtype);

          assertEquals(2, testElement.sites.length);
          assertEquals(
              prefsIncognito.exceptions[contentType][1].origin,
              testElement.sites[0].origin);
          assertEquals(
              prefsIncognito.exceptions[contentType][2].origin,
              testElement.sites[1].origin);

          Polymer.dom.flush();  // Populates action menu.
          openActionMenu(0);
          // 'Clear on exit' is hidden for incognito items.
          assertMenu(['Block', 'Edit', 'Remove'], testElement);
          closeActionMenu();

          // Select 'Remove' from menu on 'foo.com'.
          openActionMenu(1);
          const remove = testElement.$.reset;
          assertTrue(!!remove);
          remove.click();
          return browserProxy.whenCalled('resetCategoryPermissionForPattern');
        })
        .then(function(args) {
          assertEquals('http://foo.com', args[0]);
          assertEquals('', args[1]);
          assertEquals(contentType, args[2]);
          assertTrue(args[3]);  // Incognito.
        });
  });

  test('reset button works for read-only content types', function() {
    testElement.readOnlyList = true;
    Polymer.dom.flush();

    const contentType = settings.ContentSettingsTypes.GEOLOCATION;
    const categorySubtype = settings.ContentSetting.ALLOW;
    setUpCategory(contentType, categorySubtype, prefsOneEnabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertEquals(categorySubtype, testElement.categorySubtype);

          assertEquals(1, testElement.sites.length);
          assertEquals(
              prefsOneEnabled.exceptions[contentType][0].origin,
              testElement.sites[0].origin);

          Polymer.dom.flush();

          const item = testElement.$$('site-list-entry');

          // Assert action button is hidden.
          const dots = item.$.actionMenuButton;
          assertTrue(!!dots);
          assertTrue(dots.hidden);

          // Assert reset button is visible.
          const resetButton = item.$.resetSite;
          assertTrue(!!resetButton);
          assertFalse(resetButton.hidden);

          resetButton.click();
          return browserProxy.whenCalled('resetCategoryPermissionForPattern');
        })
        .then(function(args) {
          assertEquals('https://foo-allow.com:443', args[0]);
          assertEquals('', args[1]);
          assertEquals(contentType, args[2]);
        });
  });

  test('edit action menu opens edit exception dialog', function() {
    setUpCategory(
        settings.ContentSettingsTypes.COOKIES,
        settings.ContentSetting.SESSION_ONLY, prefsSessionOnly);

    return browserProxy.whenCalled('getExceptionList').then(function() {
      Polymer.dom.flush();  // Populates action menu.

      openActionMenu(0);
      assertMenu(['Allow', 'Block', 'Edit', 'Remove'], testElement);
      const menu = testElement.$$('cr-action-menu');
      assertTrue(menu.open);
      const edit = testElement.$.edit;
      assertTrue(!!edit);
      edit.click();
      Polymer.dom.flush();
      assertFalse(menu.open);

      assertTrue(!!testElement.$$('settings-edit-exception-dialog'));
    });
  });

  test('edit dialog closes when incognito status changes', function() {
    setUpCategory(
        settings.ContentSettingsTypes.COOKIES, settings.ContentSetting.BLOCK,
        prefsSessionOnly);

    return browserProxy.whenCalled('getExceptionList')
        .then(function() {
          Polymer.dom.flush();  // Populates action menu.

          openActionMenu(0);
          testElement.$.edit.click();
          Polymer.dom.flush();

          const dialog = testElement.$$('settings-edit-exception-dialog');
          assertTrue(!!dialog);
          const closeEventPromise = test_util.eventToPromise('close', dialog);
          browserProxy.setIncognito(true);
          return closeEventPromise;
        })
        .then(() => {
          assertFalse(!!testElement.$$('settings-edit-exception-dialog'));
        });
  });

  test('list items shown and clickable when data is present', function() {
    const contentType = settings.ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, settings.ContentSetting.ALLOW, prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);

          // Required for firstItem to be found below.
          Polymer.dom.flush();

          // Validate that the sites gets populated from pre-canned prefs.
          assertEquals(2, testElement.sites.length);
          assertEquals(
              prefsGeolocation.exceptions[contentType][0].origin,
              testElement.sites[0].origin);
          assertEquals(
              prefsGeolocation.exceptions[contentType][1].origin,
              testElement.sites[1].origin);
          assertFalse(!!testElement.selectedOrigin);

          // Validate that the sites are shown in UI and can be selected.
          const clickable = testElement.$$('site-list-entry').$$('.middle');
          assertTrue(!!clickable);
          clickable.click();
          assertEquals(
              prefsGeolocation.exceptions[contentType][0].origin,
              settings.getQueryParameters().get('site'));
        });
  });

  test('Block list open when Allow list is empty', function() {
    // Prefs: One item in Block list, nothing in Allow list.
    const contentType = settings.ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, settings.ContentSetting.BLOCK, prefsOneDisabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          return test_util.waitBeforeNextRender(testElement);
        })
        .then(function() {
          assertFalse(testElement.$.category.hidden);
          assertNotEquals(0, testElement.$.listContainer.offsetHeight);
        });
  });

  test('Block list open when Allow list is not empty', function() {
    // Prefs: Items in both Block and Allow list.
    const contentType = settings.ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, settings.ContentSetting.BLOCK, prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          return test_util.waitBeforeNextRender(testElement);
        })
        .then(function() {
          assertFalse(testElement.$.category.hidden);
          assertNotEquals(0, testElement.$.listContainer.offsetHeight);
        });
  });

  test('Allow list is always open (Block list empty)', function() {
    // Prefs: One item in Allow list, nothing in Block list.
    const contentType = settings.ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, settings.ContentSetting.ALLOW, prefsOneEnabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          return test_util.waitBeforeNextRender(testElement);
        })
        .then(function() {
          assertFalse(testElement.$.category.hidden);
          assertNotEquals(0, testElement.$.listContainer.offsetHeight);
        });
  });

  test('Allow list is always open (Block list non-empty)', function() {
    // Prefs: Items in both Block and Allow list.
    const contentType = settings.ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, settings.ContentSetting.ALLOW, prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          return test_util.waitBeforeNextRender(testElement);
        })
        .then(function() {
          assertFalse(testElement.$.category.hidden);
          assertNotEquals(0, testElement.$.listContainer.offsetHeight);
        });
  });

  test('Block list not hidden when empty', function() {
    // Prefs: One item in Allow list, nothing in Block list.
    const contentType = settings.ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, settings.ContentSetting.BLOCK, prefsOneEnabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertFalse(testElement.$.category.hidden);
        });
  });

  test('Allow list not hidden when empty', function() {
    // Prefs: One item in Block list, nothing in Allow list.
    const contentType = settings.ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, settings.ContentSetting.ALLOW, prefsOneDisabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertFalse(testElement.$.category.hidden);
        });
  });

  test('Mixed embeddingOrigin', function() {
    setUpCategory(
        settings.ContentSettingsTypes.IMAGES, settings.ContentSetting.ALLOW,
        prefsMixedEmbeddingOrigin);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          // Required for firstItem to be found below.
          Polymer.dom.flush();
          // Validate that embeddingOrigin sites cannot be edited.
          const entries = testElement.root.querySelectorAll('site-list-entry');
          const firstItem = entries[0];
          assertTrue(firstItem.$.actionMenuButton.hidden);
          assertFalse(firstItem.$.resetSite.hidden);
          // Validate that non-embeddingOrigin sites can be edited.
          const secondItem = entries[1];
          assertFalse(secondItem.$.actionMenuButton.hidden);
          assertTrue(secondItem.$.resetSite.hidden);
        });
  });

  test('Mixed schemes (present and absent)', function() {
    // Prefs: One item with scheme and one without.
    setUpCategory(
        settings.ContentSettingsTypes.GEOLOCATION,
        settings.ContentSetting.ALLOW, prefsMixedSchemes);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          // No further checks needed. If this fails, it will hang the test.
        });
  });

  test('Select menu item', function() {
    // Test for error: "Cannot read property 'origin' of undefined".
    setUpCategory(
        settings.ContentSettingsTypes.GEOLOCATION,
        settings.ContentSetting.ALLOW, prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          Polymer.dom.flush();
          openActionMenu(0);
          const allow = testElement.$.allow;
          assertTrue(!!allow);
          allow.click();
          return browserProxy.whenCalled('setCategoryPermissionForPattern');
        });
  });

  test('Chrome Extension scheme', function() {
    setUpCategory(
        settings.ContentSettingsTypes.JAVASCRIPT, settings.ContentSetting.BLOCK,
        prefsChromeExtension);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          Polymer.dom.flush();
          openActionMenu(0);
          assertMenu(['Allow', 'Edit', 'Remove'], testElement);

          const allow = testElement.$.allow;
          assertTrue(!!allow);
          allow.click();
          return browserProxy.whenCalled('setCategoryPermissionForPattern');
        })
        .then(function(args) {
          assertEquals(
              'chrome-extension://cfhgfbfpcbnnbibfphagcjmgjfjmojfa/', args[0]);
          assertEquals('', args[1]);
          assertEquals(settings.ContentSettingsTypes.JAVASCRIPT, args[2]);
          assertEquals(settings.ContentSetting.ALLOW, args[3]);
        });
  });

  test('show-tooltip event fires on entry shows common tooltip', function() {
    setUpCategory(
        settings.ContentSettingsTypes.GEOLOCATION,
        settings.ContentSetting.ALLOW, prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList').then(() => {
      Polymer.dom.flush();
      const entry =
          testElement.$.listContainer.querySelector('site-list-entry');
      const tooltip = testElement.$.tooltip;

      const testsParams = [
        ['a', testElement, new MouseEvent('mouseleave')],
        ['b', testElement, new MouseEvent('tap')],
        ['c', testElement, new Event('blur')],
        ['d', tooltip, new MouseEvent('mouseenter')],
      ];
      testsParams.forEach(params => {
        const text = params[0];
        const eventTarget = params[1];
        const event = params[2];
        entry.fire('show-tooltip', {target: testElement, text});
        assertTrue(tooltip._showing);
        assertEquals(text, tooltip.innerHTML.trim());
        eventTarget.dispatchEvent(event);
        assertFalse(tooltip._showing);
      });
    });
  });

  test(
      'Add site button is hidden for content settings that don\'t allow it',
      function() {
        setUpCategory(
            settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE,
            settings.ContentSetting.ALLOW, prefsNativeFileSystemWrite);
        return browserProxy.whenCalled('getExceptionList').then(() => {
          Polymer.dom.flush();
          assertFalse(testElement.showAddSiteButton_);

          const addSiteButton = testElement.$$('#addSite');
          assertTrue(addSiteButton.hidden);
        });
      });
});

suite('EditExceptionDialog', function() {
  /** @type {SettingsEditExceptionDialogElement} */ let dialog;

  /**
   * The dialog tests don't call |getExceptionList| so the exception needs to
   * be processed as a |SiteException|.
   * @type {SiteException}
   */
  let cookieException;

  setup(function() {
    cookieException = {
      category: settings.ContentSettingsTypes.COOKIES,
      embeddingOrigin: 'http://foo.com',
      incognito: false,
      setting: settings.ContentSetting.BLOCK,
      enforcement: '',
      controlledBy: 'USER_POLICY',
    };

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-edit-exception-dialog');
    dialog.model = cookieException;
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  test('invalid input', function() {
    const input = dialog.$$('cr-input');
    assertTrue(!!input);
    assertFalse(input.invalid);

    const actionButton = dialog.$.actionButton;
    assertTrue(!!actionButton);
    assertFalse(actionButton.disabled);

    // Simulate user input of whitespace only text.
    input.value = '  ';
    input.fire('input');
    Polymer.dom.flush();
    assertTrue(actionButton.disabled);
    assertTrue(input.invalid);

    // Simulate user input of invalid text.
    browserProxy.setIsPatternValidForType(false);
    const expectedPattern = '*';
    input.value = expectedPattern;
    input.fire('input');

    return browserProxy.whenCalled('isPatternValidForType')
        .then(function(pattern, category) {
          assertEquals(expectedPattern, pattern);
          assertTrue(actionButton.disabled);
          assertTrue(input.invalid);
        });
  });

  test('action button calls proxy', function() {
    const input = dialog.$$('cr-input');
    assertTrue(!!input);
    // Simulate user edit.
    const newValue = input.value + ':1234';
    input.value = newValue;

    const actionButton = dialog.$.actionButton;
    assertTrue(!!actionButton);
    assertFalse(actionButton.disabled);

    actionButton.click();
    return browserProxy.whenCalled('resetCategoryPermissionForPattern')
        .then(function(args) {
          assertEquals(cookieException.origin, args[0]);
          assertEquals(cookieException.embeddingOrigin, args[1]);
          assertEquals(settings.ContentSettingsTypes.COOKIES, args[2]);
          assertEquals(cookieException.incognito, args[3]);

          return browserProxy.whenCalled('setCategoryPermissionForPattern');
        })
        .then(function(args) {
          assertEquals(newValue, args[0]);
          assertEquals(newValue, args[1]);
          assertEquals(settings.ContentSettingsTypes.COOKIES, args[2]);
          assertEquals(cookieException.setting, args[3]);
          assertEquals(cookieException.incognito, args[4]);

          assertFalse(dialog.$.dialog.open);
        });
  });
});

suite('AddExceptionDialog', function() {
  /** @type {AddSiteDialogElement} */ let dialog;

  setup(function() {
    populateTestExceptions();

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('add-site-dialog');
    dialog.category = settings.ContentSettingsTypes.GEOLOCATION;
    dialog.contentSetting = settings.ContentSetting.ALLOW;
    dialog.hasIncognito = false;
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  test('incognito', function() {
    dialog.set('hasIncognito', true);
    Polymer.dom.flush();
    assertFalse(dialog.$.incognito.checked);
    dialog.$.incognito.checked = true;
    // Changing the incognito status will reset the checkbox.
    dialog.set('hasIncognito', false);
    Polymer.dom.flush();
    assertFalse(dialog.$.incognito.checked);
  });

  test('invalid input', function() {
    // Initially the action button should be disabled, but the error warning
    // should not be shown for an empty input.
    const input = dialog.$$('cr-input');
    assertTrue(!!input);
    assertFalse(input.invalid);

    const actionButton = dialog.$.add;
    assertTrue(!!actionButton);
    assertTrue(actionButton.disabled);

    // Simulate user input of invalid text.
    browserProxy.setIsPatternValidForType(false);
    const expectedPattern = 'foobarbaz';
    input.value = expectedPattern;
    input.fire('input');

    return browserProxy.whenCalled('isPatternValidForType')
        .then(function(pattern) {
          assertEquals(expectedPattern, pattern);
          assertTrue(actionButton.disabled);
          assertTrue(input.invalid);
        });
  });
});
