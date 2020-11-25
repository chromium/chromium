// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list. */

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting, ContentSettingsTypes, kControlledByLookup, SITE_EXCEPTION_WILDCARD, SiteException, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs,Router} from 'chrome://settings/settings.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../chai_assert.js';
import {eventToPromise, waitBeforeNextRender} from '../test_util.m.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair, createRawSiteException, createSiteSettingsPrefs, SiteSettingsPref} from './test_util.js';
// clang-format on

/**
 * An example pref with 2 blocked location items and 2 allowed. This pref
 * is also used for the All Sites category and therefore needs values for
 * all types, even though some might be blank.
 * @type {SiteSettingsPref}
 */
let prefsGeolocation;

/**
 * An example pref that is empty.
 * @type {SiteSettingsPref}
 */
let prefsGeolocationEmpty;

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
 * An example pref with file system write
 * @type {SiteSettingsPref}
 */
let prefsFileSystemWrite;

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
 * An example pref with 1 embargoed location item.
 * @type {SiteSettingsPref}
 */
let prefsEmbargo;

/**
 * Creates all the test |SiteSettingsPref|s that are needed for the tests in
 * this file. They are populated after test setup in order to access the
 * |settings| constants required.
 */
function populateTestExceptions() {
  prefsGeolocation = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.GEOLOCATION,
        [
          createRawSiteException('https://bar-allow.com:443'),
          createRawSiteException('https://foo-allow.com:443'),
          createRawSiteException('https://bar-block.com:443', {
            setting: ContentSetting.BLOCK,
          }),
          createRawSiteException('https://foo-block.com:443', {
            setting: ContentSetting.BLOCK,
          })
        ]),
  ]);


  prefsMixedSchemes = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.GEOLOCATION,
        [
          createRawSiteException('https://foo-allow.com', {
            source: SiteSettingSource.POLICY,
          }),
          createRawSiteException('bar-allow.com'),
        ]),
  ]);

  prefsMixedProvider = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.GEOLOCATION,
        [
          createRawSiteException('https://[*.]foo.com', {
            setting: ContentSetting.BLOCK,
            source: SiteSettingSource.POLICY,
          }),
          createRawSiteException('https://bar.foo.com', {
            setting: ContentSetting.BLOCK,
            source: SiteSettingSource.POLICY,
          }),
          createRawSiteException('https://[*.]foo.com', {
            setting: ContentSetting.BLOCK,
            source: SiteSettingSource.POLICY,
          })
        ]),
  ]);

  prefsMixedEmbeddingOrigin = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.IMAGES,
        [
          createRawSiteException('https://foo.com', {
            embeddingOrigin: 'https://example.com',
          }),
          createRawSiteException('https://bar.com', {
            embeddingOrigin: '',
          })
        ]),
  ]);

  prefsVarious = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.GEOLOCATION,
        [
          createRawSiteException('https://foo.com', {
            embeddingOrigin: '',
          }),
          createRawSiteException('https://bar.com', {
            embeddingOrigin: '',
          })
        ]),
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.NOTIFICATIONS,
        [
          createRawSiteException('https://google.com', {
            embeddingOrigin: '',
          }),
          createRawSiteException('https://bar.com', {
            embeddingOrigin: '',
          }),
          createRawSiteException('https://foo.com', {
            embeddingOrigin: '',
          })
        ]),
  ]);

  prefsOneEnabled = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.GEOLOCATION,
        [createRawSiteException('https://foo-allow.com:443', {
          embeddingOrigin: '',
        })]),
  ]);

  prefsOneDisabled = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.GEOLOCATION,
        [createRawSiteException('https://foo-block.com:443', {
          embeddingOrigin: '',
          setting: ContentSetting.BLOCK,
        })]),
  ]);

  prefsSessionOnly = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.COOKIES,
        [
          createRawSiteException('http://foo-block.com', {
            embeddingOrigin: '',
            setting: ContentSetting.BLOCK,
          }),
          createRawSiteException('http://foo-allow.com', {
            embeddingOrigin: '',
          }),
          createRawSiteException('http://foo-session.com', {
            embeddingOrigin: '',
            setting: ContentSetting.SESSION_ONLY,
          })
        ]),
  ]);

  prefsIncognito = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.COOKIES,
        [
          // foo.com is blocked for regular sessions.
          createRawSiteException('http://foo.com', {
            embeddingOrigin: '',
            setting: ContentSetting.BLOCK,
          }),
          // bar.com is an allowed incognito item.
          createRawSiteException('http://bar.com', {
            embeddingOrigin: '',
            incognito: true,
          }),
          // foo.com is allowed in incognito (overridden).
          createRawSiteException('http://foo.com', {
            embeddingOrigin: '',
            incognito: true,
          })
        ]),
  ]);

  prefsChromeExtension = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.JAVASCRIPT,
        [createRawSiteException(
            'chrome-extension://cfhgfbfpcbnnbibfphagcjmgjfjmojfa/', {
              embeddingOrigin: '',
              setting: ContentSetting.BLOCK,
            })]),
  ]);

  prefsGeolocationEmpty = createSiteSettingsPrefs([], []);

  prefsFileSystemWrite = createSiteSettingsPrefs(
      [], [createContentSettingTypeToValuePair(
              ContentSettingsTypes.FILE_SYSTEM_WRITE,
              [createRawSiteException('http://foo.com', {
                setting: ContentSetting.BLOCK,
              })])]);

  prefsEmbargo = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.GEOLOCATION,
        [createRawSiteException('https://foo-block.com:443', {
          embeddingOrigin: '',
          setting: ContentSetting.BLOCK,
          isEmbargoed: true,
        })]),
  ]);
}

suite('SiteListEmbargoedOrigin', function() {
  /**
   * A site list element created before each test.
   * @type {!SiteListElement}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {!TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy;

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
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    testElement =
        /** @type {!SiteListElement} */ (document.createElement('site-list'));
    testElement.searchFilter = '';
    document.body.appendChild(testElement);
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * Configures the test element for a particular category.
   * @param {ContentSettingsTypes} category The category to set up.
   * @param {ContentSetting} subtype Type of list to use.
   * @param {!SiteSettingsPref} prefs The prefs to use.
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

  test('embaroed origin site description', async function() {
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.BLOCK, prefsEmbargo);
    const result = await browserProxy.whenCalled('getExceptionList');
    flush();

    assertEquals(contentType, result);

    // Validate that the sites gets populated from pre-canned prefs.
    assertEquals(1, testElement.sites.length);
    assertEquals(
        prefsEmbargo.exceptions[contentType][0].origin,
        testElement.sites[0].origin);
    assertTrue(testElement.sites[0].isEmbargoed);
    // Validate that embargoed site has correct subtitle.
    assertEquals(
        loadTimeData.getString('siteSettingsSourceEmbargo'),
        testElement.$$('#listContainer')
            .querySelectorAll('site-list-entry')[0]
            .$$('#siteDescription')
            .innerHTML);
  });
});



suite('SiteList', function() {
  /**
   * A site list element created before each test.
   * @type {!SiteListElement}
   */
  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {!TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy;

  suiteSetup(function() {
    // clang-format off
          CrSettingsPrefs.setInitialized();
    // clang-format on
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  // Initialize a site-list before each test.
  setup(function() {
    populateTestExceptions();

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    testElement =
        /** @type {!SiteListElement} */ (document.createElement('site-list'));
    testElement.searchFilter = '';
    document.body.appendChild(testElement);
  });

  teardown(function() {
    closeActionMenu();
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * Opens the action menu for a particular element in the list.
   * @param {number} index The index of the child element (which site) to
   *     open the action menu for.
   */
  function openActionMenu(index) {
    const actionMenuButton = testElement.$$('#listContainer')
                                 .querySelectorAll('site-list-entry')[index]
                                 .$$('#actionMenuButton');
    actionMenuButton.click();
    flush();
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
   * @param {!Array<string>} items The items expected to show in the menu.
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
   * @param {!Element} listContainer Node with the exceptions listed.
   * @return {boolean} Whether the entry is incognito only.
   */
  function hasAnIncognito(listContainer) {
    return listContainer.querySelector('iron-list')
        .items.some(item => item.incognito);
  }

  /**
   * Configures the test element for a particular category.
   * @param {ContentSettingsTypes} category The category to set up.
   * @param {ContentSetting} subtype Type of list to use.
   * @param {!SiteSettingsPref} prefs The prefs to use.
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
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW, prefsVarious);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          // Flush to be sure list container is populated.
          flush();
          const dotsMenu =
              testElement.$$('site-list-entry').$$('#actionMenuButton');
          assertFalse(dotsMenu.hidden);
          testElement.setAttribute('read-only-list', true);
          flush();
          assertTrue(dotsMenu.hidden);
          testElement.removeAttribute('read-only-list');
          flush();
          assertFalse(dotsMenu.hidden);
        });
  });

  test('getExceptionList API used', function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocationEmpty);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          assertEquals(ContentSettingsTypes.GEOLOCATION, contentType);
        });
  });

  test('Empty list', function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocationEmpty);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          assertEquals(ContentSettingsTypes.GEOLOCATION, contentType);

          assertEquals(0, testElement.sites.length);

          assertEquals(ContentSetting.ALLOW, testElement.categorySubtype);

          assertFalse(testElement.$$('#category').hidden);
        });
  });

  test('initial ALLOW state is correct', function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          assertEquals(ContentSettingsTypes.GEOLOCATION, contentType);

          assertEquals(2, testElement.sites.length);
          assertEquals(
              prefsGeolocation.exceptions[contentType][0].origin,
              testElement.sites[0].origin);
          assertEquals(
              prefsGeolocation.exceptions[contentType][1].origin,
              testElement.sites[1].origin);
          assertEquals(ContentSetting.ALLOW, testElement.categorySubtype);
          flush();  // Populates action menu.
          openActionMenu(0);
          assertMenu(['Block', 'Edit', 'Remove']);

          assertFalse(testElement.$$('#category').hidden);
        });
  });

  test('action menu closes when list changes', function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocation);
    const actionMenu = testElement.$$('cr-action-menu');
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          flush();  // Populates action menu.
          openActionMenu(0);
          assertTrue(actionMenu.open);

          browserProxy.resetResolver('getExceptionList');
          // Simulate a change in the underlying model.
          webUIListenerCallback(
              'contentSettingSitePermissionChanged',
              ContentSettingsTypes.GEOLOCATION);
          return browserProxy.whenCalled('getExceptionList');
        })
        .then(function() {
          // Check that the action menu was closed.
          assertFalse(actionMenu.open);
        });
  });

  test('exceptions are not reordered in non-ALL_SITES', function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.BLOCK,
        prefsMixedProvider);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          assertEquals(ContentSettingsTypes.GEOLOCATION, contentType);
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
    const contentType = ContentSettingsTypes.GEOLOCATION;
    const categorySubtype = ContentSetting.BLOCK;
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
          flush();  // Populates action menu.
          openActionMenu(0);
          assertMenu(['Allow', 'Edit', 'Remove']);

          assertFalse(testElement.$$('#category').hidden);
        });
  });

  test('initial SESSION ONLY state is correct', function() {
    const contentType = ContentSettingsTypes.COOKIES;
    const categorySubtype = ContentSetting.SESSION_ONLY;
    setUpCategory(contentType, categorySubtype, prefsSessionOnly);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertEquals(categorySubtype, testElement.categorySubtype);

          assertEquals(1, testElement.sites.length);
          assertEquals(
              prefsSessionOnly.exceptions[contentType][2].origin,
              testElement.sites[0].origin);

          flush();  // Populates action menu.
          openActionMenu(0);
          assertMenu(['Allow', 'Block', 'Edit', 'Remove']);

          assertFalse(testElement.$$('#category').hidden);
        });
  });

  test('initial INCOGNITO BLOCK state is correct', function() {
    const contentType = ContentSettingsTypes.COOKIES;
    const categorySubtype = ContentSetting.BLOCK;
    setUpCategory(contentType, categorySubtype, prefsIncognito);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertEquals(categorySubtype, testElement.categorySubtype);

          assertEquals(1, testElement.sites.length);
          assertEquals(
              prefsIncognito.exceptions[contentType][0].origin,
              testElement.sites[0].origin);

          flush();  // Populates action menu.
          openActionMenu(0);
          // 'Clear on exit' is visible as this is not an incognito item.
          assertMenu(['Allow', 'Clear on exit', 'Edit', 'Remove']);

          // Select 'Remove' from menu.
          const remove = testElement.$$('#reset');
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
    const contentType = ContentSettingsTypes.COOKIES;
    const categorySubtype = ContentSetting.ALLOW;
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

          flush();  // Populates action menu.
          openActionMenu(0);
          // 'Clear on exit' is hidden for incognito items.
          assertMenu(['Block', 'Edit', 'Remove']);
          closeActionMenu();

          // Select 'Remove' from menu on 'foo.com'.
          openActionMenu(1);
          const remove = testElement.$$('#reset');
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
    flush();

    const contentType = ContentSettingsTypes.GEOLOCATION;
    const categorySubtype = ContentSetting.ALLOW;
    setUpCategory(contentType, categorySubtype, prefsOneEnabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertEquals(categorySubtype, testElement.categorySubtype);

          assertEquals(1, testElement.sites.length);
          assertEquals(
              prefsOneEnabled.exceptions[contentType][0].origin,
              testElement.sites[0].origin);

          flush();

          const item = testElement.$$('site-list-entry');

          // Assert action button is hidden.
          const dots = item.$$('#actionMenuButton');
          assertTrue(!!dots);
          assertTrue(dots.hidden);

          // Assert reset button is visible.
          const resetButton = item.$$('#resetSite');
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
        ContentSettingsTypes.COOKIES, ContentSetting.SESSION_ONLY,
        prefsSessionOnly);

    return browserProxy.whenCalled('getExceptionList').then(function() {
      flush();  // Populates action menu.

      openActionMenu(0);
      assertMenu(['Allow', 'Block', 'Edit', 'Remove']);
      const menu = testElement.$$('cr-action-menu');
      assertTrue(menu.open);
      const edit = testElement.$$('#edit');
      assertTrue(!!edit);
      edit.click();
      flush();
      assertFalse(menu.open);

      assertTrue(!!testElement.$$('settings-edit-exception-dialog'));
    });
  });

  test('edit dialog closes when incognito status changes', function() {
    setUpCategory(
        ContentSettingsTypes.COOKIES, ContentSetting.BLOCK, prefsSessionOnly);

    return browserProxy.whenCalled('getExceptionList')
        .then(function() {
          flush();  // Populates action menu.

          openActionMenu(0);
          testElement.$$('#edit').click();
          flush();

          const dialog = /** @type {!HTMLElement} */ (
              testElement.$$('settings-edit-exception-dialog'));
          assertTrue(!!dialog);
          const closeEventPromise = eventToPromise('close', dialog);
          browserProxy.setIncognito(true);
          return closeEventPromise;
        })
        .then(() => {
          assertFalse(!!testElement.$$('settings-edit-exception-dialog'));
        });
  });

  test('list items shown and clickable when data is present', function() {
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.ALLOW, prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);

          // Required for firstItem to be found below.
          flush();

          // Validate that the sites gets populated from pre-canned prefs.
          assertEquals(2, testElement.sites.length);
          assertEquals(
              prefsGeolocation.exceptions[contentType][0].origin,
              testElement.sites[0].origin);
          assertEquals(
              prefsGeolocation.exceptions[contentType][1].origin,
              testElement.sites[1].origin);

          // Validate that the sites are shown in UI and can be selected.
          const clickable = testElement.$$('site-list-entry').$$('.middle');
          assertTrue(!!clickable);
          clickable.click();
          assertEquals(
              prefsGeolocation.exceptions[contentType][0].origin,
              Router.getInstance().getQueryParameters().get('site'));
        });
  });

  test('Block list open when Allow list is empty', function() {
    // Prefs: One item in Block list, nothing in Allow list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.BLOCK, prefsOneDisabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          return waitBeforeNextRender(testElement);
        })
        .then(function() {
          assertFalse(testElement.$$('#category').hidden);
          assertNotEquals(0, testElement.$$('#listContainer').offsetHeight);
        });
  });

  test('Block list open when Allow list is not empty', function() {
    // Prefs: Items in both Block and Allow list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.BLOCK, prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          return waitBeforeNextRender(testElement);
        })
        .then(function() {
          assertFalse(testElement.$$('#category').hidden);
          assertNotEquals(0, testElement.$$('#listContainer').offsetHeight);
        });
  });

  test('Allow list is always open (Block list empty)', function() {
    // Prefs: One item in Allow list, nothing in Block list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.ALLOW, prefsOneEnabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          return waitBeforeNextRender(testElement);
        })
        .then(function() {
          assertFalse(testElement.$$('#category').hidden);
          assertNotEquals(0, testElement.$$('#listContainer').offsetHeight);
        });
  });

  test('Allow list is always open (Block list non-empty)', function() {
    // Prefs: Items in both Block and Allow list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.ALLOW, prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          return waitBeforeNextRender(testElement);
        })
        .then(function() {
          assertFalse(testElement.$$('#category').hidden);
          assertNotEquals(0, testElement.$$('#listContainer').offsetHeight);
        });
  });

  test('Block list not hidden when empty', function() {
    // Prefs: One item in Allow list, nothing in Block list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.BLOCK, prefsOneEnabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertFalse(testElement.$$('#category').hidden);
        });
  });

  test('Allow list not hidden when empty', function() {
    // Prefs: One item in Block list, nothing in Allow list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.ALLOW, prefsOneDisabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertFalse(testElement.$$('#category').hidden);
        });
  });

  test('Mixed embeddingOrigin', function() {
    setUpCategory(
        ContentSettingsTypes.IMAGES, ContentSetting.ALLOW,
        prefsMixedEmbeddingOrigin);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          // Required for firstItem to be found below.
          flush();
          // Validate that embeddingOrigin sites cannot be edited.
          const entries = testElement.root.querySelectorAll('site-list-entry');
          const firstItem = entries[0];
          assertTrue(firstItem.$$('#actionMenuButton').hidden);
          assertFalse(firstItem.$$('#resetSite').hidden);
          // Validate that non-embeddingOrigin sites can be edited.
          const secondItem = entries[1];
          assertFalse(secondItem.$$('#actionMenuButton').hidden);
          assertTrue(secondItem.$$('#resetSite').hidden);
        });
  });

  test('Mixed schemes (present and absent)', function() {
    // Prefs: One item with scheme and one without.
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsMixedSchemes);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          // No further checks needed. If this fails, it will hang the test.
        });
  });

  test('Select menu item', function() {
    // Test for error: "Cannot read property 'origin' of undefined".
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          flush();
          openActionMenu(0);
          const allow = testElement.$$('#allow');
          assertTrue(!!allow);
          allow.click();
          return browserProxy.whenCalled('setCategoryPermissionForPattern');
        });
  });

  test('Chrome Extension scheme', function() {
    setUpCategory(
        ContentSettingsTypes.JAVASCRIPT, ContentSetting.BLOCK,
        prefsChromeExtension);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(contentType) {
          flush();
          openActionMenu(0);
          assertMenu(['Allow', 'Edit', 'Remove']);

          const allow = testElement.$$('#allow');
          assertTrue(!!allow);
          allow.click();
          return browserProxy.whenCalled('setCategoryPermissionForPattern');
        })
        .then(function(args) {
          assertEquals(
              'chrome-extension://cfhgfbfpcbnnbibfphagcjmgjfjmojfa/', args[0]);
          assertEquals('', args[1]);
          assertEquals(ContentSettingsTypes.JAVASCRIPT, args[2]);
          assertEquals(ContentSetting.ALLOW, args[3]);
        });
  });

  test('show-tooltip event fires on entry shows common tooltip', function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocation);
    return browserProxy.whenCalled('getExceptionList').then(() => {
      flush();
      const entry =
          testElement.$$('#listContainer').querySelector('site-list-entry');
      const tooltip = testElement.$$('#tooltip');

      const testsParams = [
        ['a', testElement, new MouseEvent('mouseleave')],
        ['b', testElement, new MouseEvent('click')],
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
            ContentSettingsTypes.FILE_SYSTEM_WRITE, ContentSetting.ALLOW,
            prefsFileSystemWrite);
        return browserProxy.whenCalled('getExceptionList').then(() => {
          flush();
          assertTrue(testElement.$$('#addSite').hidden);
        });
      });
});

suite('EditExceptionDialog', function() {
  /** @type {!SettingsEditExceptionDialogElement} */
  let dialog;

  /**
   * The dialog tests don't call |getExceptionList| so the exception needs to
   * be processed as a |SiteException|.
   * @type {SiteException}
   */
  let cookieException;

  /** @type {!TestSiteSettingsPrefsBrowserProxy} */
  let browserProxy;

  setup(function() {
    cookieException = {
      category: ContentSettingsTypes.COOKIES,
      embeddingOrigin: SITE_EXCEPTION_WILDCARD,
      isEmbargoed: false,
      incognito: false,
      setting: ContentSetting.BLOCK,
      enforcement: null,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      displayName: 'foo.com',
      origin: 'foo.com',
    };

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    dialog = /** @type {!SettingsEditExceptionDialogElement} */ (
        document.createElement('settings-edit-exception-dialog'));
    dialog.model = cookieException;
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  test('invalid input', function() {
    const input = /** @type {!CrInputElement} */ (dialog.$$('cr-input'));
    assertTrue(!!input);
    assertFalse(input.invalid);

    const actionButton = dialog.$$('#actionButton');
    assertTrue(!!actionButton);
    assertFalse(actionButton.disabled);

    // Simulate user input of whitespace only text.
    input.value = '  ';
    input.fire('input');
    flush();
    assertTrue(actionButton.disabled);
    assertTrue(input.invalid);

    // Simulate user input of invalid text.
    browserProxy.setIsPatternValidForType(false);
    const expectedPattern = '*';
    input.value = expectedPattern;
    input.fire('input');

    return browserProxy.whenCalled('isPatternValidForType').then(function([
      pattern, category
    ]) {
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

    const actionButton = dialog.$$('#actionButton');
    assertTrue(!!actionButton);
    assertFalse(actionButton.disabled);

    actionButton.click();
    return browserProxy.whenCalled('resetCategoryPermissionForPattern')
        .then(function(args) {
          assertEquals(cookieException.origin, args[0]);
          assertEquals(cookieException.embeddingOrigin, args[1]);
          assertEquals(ContentSettingsTypes.COOKIES, args[2]);
          assertEquals(cookieException.incognito, args[3]);

          return browserProxy.whenCalled('setCategoryPermissionForPattern');
        })
        .then(function(args) {
          assertEquals(newValue, args[0]);
          assertEquals(SITE_EXCEPTION_WILDCARD, args[1]);
          assertEquals(ContentSettingsTypes.COOKIES, args[2]);
          assertEquals(cookieException.setting, args[3]);
          assertEquals(cookieException.incognito, args[4]);

          assertFalse(dialog.$$('#dialog').open);
        });
  });
});

suite('AddExceptionDialog', function() {
  /** @type {!AddSiteDialogElement} */
  let dialog;

  /** @type {!TestSiteSettingsPrefsBrowserProxy} */
  let browserProxy;

  setup(function() {
    populateTestExceptions();

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    document.body.innerHTML = '';
    dialog = /** @type {!AddSiteDialogElement} */ (
        document.createElement('add-site-dialog'));
    dialog.category = ContentSettingsTypes.GEOLOCATION;
    dialog.contentSetting = ContentSetting.ALLOW;
    dialog.hasIncognito = false;
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  test('incognito', function() {
    dialog.set('hasIncognito', true);
    flush();
    assertFalse(dialog.$$('#incognito').checked);
    dialog.$$('#incognito').checked = true;
    // Changing the incognito status will reset the checkbox.
    dialog.set('hasIncognito', false);
    flush();
    assertFalse(dialog.$$('#incognito').checked);
  });

  test('invalid input', function() {
    // Initially the action button should be disabled, but the error warning
    // should not be shown for an empty input.
    const input = dialog.$$('cr-input');
    assertTrue(!!input);
    assertFalse(input.invalid);

    const actionButton = dialog.$$('#add');
    assertTrue(!!actionButton);
    assertTrue(actionButton.disabled);

    // Simulate user input of invalid text.
    browserProxy.setIsPatternValidForType(false);
    const expectedPattern = 'foobarbaz';
    input.value = expectedPattern;
    input.fire('input');

    return browserProxy.whenCalled('isPatternValidForType').then(function([
      pattern, category
    ]) {
      assertEquals(expectedPattern, pattern);
      assertTrue(actionButton.disabled);
      assertTrue(input.invalid);
    });
  });
});
