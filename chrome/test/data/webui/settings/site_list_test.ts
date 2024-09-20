// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for site-list. */

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {AddSiteDialogElement, SettingsEditExceptionDialogElement, SiteException, SiteListElement} from 'chrome://settings/lazy_load.js';
import {CookiesExceptionType, ContentSetting, ContentSettingsTypes, SITE_EXCEPTION_WILDCARD, SiteSettingSource, SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, Router} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createRawSiteException, createSiteSettingsPrefs} from './test_util.js';
// clang-format on

/**
 * An example pref with 2 blocked location items and 2 allowed. This pref
 * is also used for the All Sites category and therefore needs values for
 * all types, even though some might be blank.
 */
let prefsGeolocation: SiteSettingsPref;

/**
 * An example pref that is empty.
 */
let prefsGeolocationEmpty: SiteSettingsPref;

/**
 * An example pref with mixed schemes (present and absent).
 */
let prefsMixedSchemes: SiteSettingsPref;

/**
 * An example pref with exceptions with origins and patterns from
 * different providers.
 */
let prefsMixedProvider: SiteSettingsPref;

/**
 * An example pref with with and without embeddingOrigin.
 */
let prefsMixedEmbeddingOrigin: SiteSettingsPref;

/**
 * An example pref with file system write
 */
let prefsFileSystemWrite: SiteSettingsPref;

/**
 * An example pref with multiple categories and multiple allow/block
 * state.
 */
let prefsVarious: SiteSettingsPref;

/**
 * An example pref with 1 allowed location item.
 */
let prefsOneEnabled: SiteSettingsPref;

/**
 * An example pref with 1 blocked location item.
 */
let prefsOneDisabled: SiteSettingsPref;

/**
 * An example pref with 1 allowed notification item.
 */
let prefsOneEnabledNotification: SiteSettingsPref;

/**
 * An example pref with 1 blocked notification item.
 */
let prefsOneDisabledNotification: SiteSettingsPref;

/**
 * An example pref with 2 allowed notification item.
 */
let prefsTwoEnabledNotification: SiteSettingsPref;

/**
 * An example pref with 2 blocked notification item.
 */
let prefsTwoDisabledNotification: SiteSettingsPref;

/**
 * An example Cookies pref with 1 in each of the three categories.
 */
let prefsSessionOnly: SiteSettingsPref;

/**
 * An example Cookies pref with mixed incognito and regular settings.
 */
let prefsIncognito: SiteSettingsPref;

/**
 * An example Javascript pref with a chrome-extension:// scheme.
 */
let prefsChromeExtension: SiteSettingsPref;

/**
 * An example pref with 1 embargoed location item.
 */
let prefsEmbargo: SiteSettingsPref;

/**
 * An example pref with Isolated Web App having notification.
 */
let prefsIsolatedWebApp: SiteSettingsPref;

/**
 * An example pref with mixed cookies exception types: 2 exceptions with primary
 * pattern wildcard, 2 exceptions with secondary pattern wildcard and 1
 * exception with both patterns set.
 */
let prefsMixedCookiesExceptionTypes: SiteSettingsPref;

/**
 * An example pref with mixed cookies exception types: 2 each for 1p allow, 1p
 * block, 3p allow, and 3p block.
 */
let prefsMixedCookiesExceptionTypes2: SiteSettingsPref;

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
          }),
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
          }),
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
          }),
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
          }),
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
          }),
        ]),
  ]);

  prefsOneEnabled = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.GEOLOCATION,
        [createRawSiteException('https://foo-allow.com:443', {
          embeddingOrigin: '',
          type: ContentSettingsTypes.GEOLOCATION,
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

  prefsOneEnabledNotification = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.NOTIFICATIONS,
        [createRawSiteException('https://foo-allow.com:443', {
          embeddingOrigin: '',
          type: ContentSettingsTypes.NOTIFICATIONS,
          setting: ContentSetting.ALLOW,
        })]),
  ]);

  prefsOneDisabledNotification = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.NOTIFICATIONS,
        [createRawSiteException('https://foo-block.com:443', {
          embeddingOrigin: '',
          type: ContentSettingsTypes.NOTIFICATIONS,
          setting: ContentSetting.BLOCK,
        })]),
  ]);

  prefsTwoEnabledNotification = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.NOTIFICATIONS,
        [
          createRawSiteException('https://foo-allow1.com:443', {
            embeddingOrigin: '',
            type: ContentSettingsTypes.NOTIFICATIONS,
            setting: ContentSetting.ALLOW,
          }),
          createRawSiteException('https://foo-allow2.com:443', {
            embeddingOrigin: '',
            type: ContentSettingsTypes.NOTIFICATIONS,
            setting: ContentSetting.ALLOW,
          }),
        ]),
  ]);

  prefsTwoDisabledNotification = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.NOTIFICATIONS,
        [
          createRawSiteException('https://foo-block1.com:443', {
            embeddingOrigin: '',
            type: ContentSettingsTypes.NOTIFICATIONS,
            setting: ContentSetting.BLOCK,
          }),
          createRawSiteException('https://foo-block2.com:443', {
            embeddingOrigin: '',
            type: ContentSettingsTypes.NOTIFICATIONS,
            setting: ContentSetting.BLOCK,
          }),
        ]),
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
          }),
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
          }),
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

  const iwaOrigin = 'isolated-app://helloworldhelloworldhelloworldhe';
  const nonIwaOrigin = 'https://bar.com';
  prefsIsolatedWebApp = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.NOTIFICATIONS,
        [
          createRawSiteException(iwaOrigin, {
            embeddingOrigin: '',
            setting: ContentSetting.ALLOW,
            displayName: iwaOrigin,
          }),
          createRawSiteException(nonIwaOrigin, {
            embeddingOrigin: '',
            displayName: nonIwaOrigin,
          }),
        ]),
  ]);

  prefsMixedCookiesExceptionTypes = createSiteSettingsPrefs([], [
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
          createRawSiteException('http://bar-allow.com', {
            embeddingOrigin: '',
          }),
          createRawSiteException('http://baz-allow.com', {
            embeddingOrigin: '',
          }),
          createRawSiteException(SITE_EXCEPTION_WILDCARD, {
            embeddingOrigin: 'http://3pc-block.com',
            setting: ContentSetting.BLOCK,
          }),
          createRawSiteException(SITE_EXCEPTION_WILDCARD, {
            embeddingOrigin: 'http://3pc-allow.com',
          }),
          createRawSiteException('http://mixed-primary-allow.com', {
            embeddingOrigin: 'http://mixed-secondary-allow.com',
          }),
        ]),
  ]);

  prefsMixedCookiesExceptionTypes2 = createSiteSettingsPrefs([], [
    createContentSettingTypeToValuePair(
        ContentSettingsTypes.COOKIES,
        [
          createRawSiteException('http://1p-foo-allow.com', {
            embeddingOrigin: '',
          }),
          createRawSiteException('http://1p-bar-allow.com', {
            embeddingOrigin: '',
          }),
          createRawSiteException('http://1p-foo-block.com', {
            embeddingOrigin: '',
            setting: ContentSetting.BLOCK,
          }),
          createRawSiteException('http://1p-bar-block.com', {
            embeddingOrigin: '',
            setting: ContentSetting.BLOCK,
          }),
          createRawSiteException(SITE_EXCEPTION_WILDCARD, {
            embeddingOrigin: 'http://3p-foo-allow.com',
          }),
          createRawSiteException(SITE_EXCEPTION_WILDCARD, {
            embeddingOrigin: 'http://3p-bar-allow.com',
          }),
          createRawSiteException(SITE_EXCEPTION_WILDCARD, {
            embeddingOrigin: 'http://3p-foo-block.com',
            setting: ContentSetting.BLOCK,
          }),
          createRawSiteException(SITE_EXCEPTION_WILDCARD, {
            embeddingOrigin: 'http://3p-bar-block.com',
            setting: ContentSetting.BLOCK,
          }),
        ]),
  ]);
}

suite('SiteListEmbargoedOrigin', function() {
  /**
   * A site list element created before each test.
   */
  let testElement: SiteListElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

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
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('site-list');
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
   * @param category The category to set up.
   * @param subtype Type of list to use.
   * @param prefs The prefs to use.
   */
  function setUpCategory(
      category: ContentSettingsTypes, subtype: ContentSetting,
      prefs: SiteSettingsPref) {
    browserProxy.setPrefs(prefs);
    testElement.categorySubtype = subtype;
    testElement.category = category;
  }

  test('embargoed origin site description', async function() {
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.BLOCK, prefsEmbargo);
    const result = await browserProxy.whenCalled('getExceptionList');
    flush();

    assertEquals(contentType, result);

    // Validate that the sites gets populated from pre-canned prefs.
    assertEquals(1, testElement.sites.length);
    assertEquals(
        prefsEmbargo.exceptions[contentType][0]!.origin,
        testElement.sites[0]!.origin);
    assertTrue(testElement.sites[0]!.isEmbargoed);
    // Validate that embargoed site has correct subtitle.
    assertEquals(
        loadTimeData.getString('siteSettingsSourceEmbargo'),
        testElement.$.listContainer.querySelectorAll('site-list-entry')[0]!
            .shadowRoot!.querySelector('#siteDescription')!.innerHTML);
  });
});

suite('SiteListCookiesExceptionTypes', function() {
  /**
   * A site list element created before each test.
   */
  let testElement: SiteListElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

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
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('site-list');
    testElement.searchFilter = '';
    document.body.appendChild(testElement);
  });

  /**
   * Configures the test element for a particular category.
   * @param category The category to set up.
   * @param subtype Type of list to use.
   * @param prefs The prefs to use.
   */
  function setUpCategory(
      category: ContentSettingsTypes, subtype: ContentSetting,
      prefs: SiteSettingsPref) {
    browserProxy.setPrefs(prefs);
    testElement.categorySubtype = subtype;
    testElement.category = category;
  }

  test('only shows third party cookies exceptions', async function() {
    testElement.cookiesExceptionType = CookiesExceptionType.THIRD_PARTY;
    setUpCategory(
        ContentSettingsTypes.COOKIES, ContentSetting.ALLOW,
        prefsMixedCookiesExceptionTypes);
    await browserProxy.whenCalled('getExceptionList');
    assertEquals(1, testElement.sites.length);
    assertEquals(testElement.sites[0]!.embeddingOrigin, 'http://3pc-allow.com');
  });

  test('only shows site data cookies exceptions', async function() {
    testElement.cookiesExceptionType = CookiesExceptionType.SITE_DATA;
    setUpCategory(
        ContentSettingsTypes.COOKIES, ContentSetting.ALLOW,
        prefsMixedCookiesExceptionTypes);
    await browserProxy.whenCalled('getExceptionList');
    assertEquals(4, testElement.sites.length);
    assertEquals(testElement.sites[0]!.origin, 'http://foo-allow.com');
    assertEquals(testElement.sites[1]!.origin, 'http://bar-allow.com');
    assertEquals(testElement.sites[2]!.origin, 'http://baz-allow.com');
    assertEquals(
        testElement.sites[3]!.origin, 'http://mixed-primary-allow.com');
    assertEquals(
        testElement.sites[3]!.embeddingOrigin,
        'http://mixed-secondary-allow.com');
  });

  test('shows all cookies exceptions', async function() {
    testElement.cookiesExceptionType = CookiesExceptionType.COMBINED;
    setUpCategory(
        ContentSettingsTypes.COOKIES, ContentSetting.ALLOW,
        prefsMixedCookiesExceptionTypes);
    await browserProxy.whenCalled('getExceptionList');
    assertEquals(5, testElement.sites.length);
    assertEquals(testElement.sites[0]!.origin, 'http://foo-allow.com');
    assertEquals(testElement.sites[1]!.origin, 'http://bar-allow.com');
    assertEquals(testElement.sites[2]!.origin, 'http://baz-allow.com');
    assertEquals(testElement.sites[3]!.embeddingOrigin, 'http://3pc-allow.com');
    assertEquals(
        testElement.sites[4]!.origin, 'http://mixed-primary-allow.com');
    assertEquals(
        testElement.sites[4]!.embeddingOrigin,
        'http://mixed-secondary-allow.com');
  });
});

// TODO(crbug.com/929455, crbug.com/1064002): Flaky test. When it is fixed,
// merge SiteListDisabled back into SiteList.
suite('DISABLED_SiteList', function() {
  /**
   * A site list element created before each test.
   */
  let testElement: SiteListElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

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
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('site-list');
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
   * @param category The category to set up.
   * @param subtype Type of list to use.
   * @param prefs The prefs to use.
   */
  function setUpCategory(
      category: ContentSettingsTypes, subtype: ContentSetting,
      prefs: SiteSettingsPref) {
    browserProxy.setPrefs(prefs);
    testElement.cookiesExceptionType = CookiesExceptionType.COMBINED;
    testElement.categorySubtype = subtype;
    testElement.category = category;
  }

  test('list items shown and clickable when data is present', async function() {
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.ALLOW, prefsGeolocation);
    const actualContentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(contentType, actualContentType);

    // Required for firstItem to be found below.
    flush();

    // Validate that the sites gets populated from pre-canned prefs.
    assertEquals(2, testElement.sites.length);
    assertEquals(
        prefsGeolocation.exceptions[contentType][0]!.origin,
        testElement.sites[0]!.origin);
    assertEquals(
        prefsGeolocation.exceptions[contentType][1]!.origin,
        testElement.sites[1]!.origin);

    // Validate that the sites are shown in UI and can be selected.
    const clickable = testElement.shadowRoot!.querySelector('site-list-entry')!
                          .shadowRoot!.querySelector<HTMLElement>('.middle');
    assertTrue(!!clickable);
    clickable!.click();

    await flushTasks();
    assertEquals(
        prefsGeolocation.exceptions[contentType][0]!.origin,
        Router.getInstance().getQueryParameters().get('site'));
  });
});

suite('SiteList', function() {
  /**
   * A site list element created before each test.
   */
  let testElement: SiteListElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

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
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('site-list');
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
   * @param index The index of the child element (which site) to
   *     open the action menu for.
   */
  function openActionMenu(index: number) {
    const actionMenuButton =
        testElement.$.listContainer.querySelectorAll('site-list-entry')[index]!
            .$.actionMenuButton;
    actionMenuButton.click();
    flush();
  }

  /** Closes the action menu. */
  function closeActionMenu() {
    const menu = testElement.shadowRoot!.querySelector('cr-action-menu')!;
    if (menu.open) {
      menu.close();
    }
  }

  /**
   * Asserts the menu looks as expected.
   * @param items The items expected to show in the menu.
   */
  function assertMenu(items: string[]) {
    const menu = testElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!menu);
    const menuItems = menu!.querySelectorAll('button:not([hidden])');
    assertEquals(items.length, menuItems.length);
    for (let i = 0; i < items.length; i++) {
      assertEquals(items[i], menuItems[i]!.textContent!.trim());
    }
  }

  /**
   * Configures the test element for a particular category.
   * @param category The category to set up.
   * @param subtype Type of list to use.
   * @param prefs The prefs to use.
   */
  function setUpCategory(
      category: ContentSettingsTypes, subtype: ContentSetting,
      prefs: SiteSettingsPref) {
    browserProxy.setPrefs(prefs);
    testElement.cookiesExceptionType = CookiesExceptionType.COMBINED;
    testElement.categorySubtype = subtype;
    testElement.category = category;
  }

  test('read-only attribute', async function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW, prefsVarious);
    await browserProxy.whenCalled('getExceptionList');
    // Flush to be sure list container is populated.
    flush();
    const dotsMenu =
        testElement.shadowRoot!.querySelector(
                                   'site-list-entry')!.$.actionMenuButton;
    assertFalse(dotsMenu.hidden);
    testElement.toggleAttribute('read-only-list', true);
    flush();
    assertTrue(dotsMenu.hidden);
    testElement.removeAttribute('read-only-list');
    flush();
    assertFalse(dotsMenu.hidden);
  });

  test('getExceptionList API used', async function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocationEmpty);
    const contentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(ContentSettingsTypes.GEOLOCATION, contentType);
  });

  /**
   * Creates test |SiteSettingsPref|s with 2 allowed and 2 blocked
   * sites for the given ContentSettingsTypes.
   */
  function create2AllowAnd2BlockPrefs(type: ContentSettingsTypes) {
    return createSiteSettingsPrefs([], [
      createContentSettingTypeToValuePair(
          type,
          [
            createRawSiteException('https://bar-allow.com:443'),
            createRawSiteException('https://foo-allow.com:443'),
            createRawSiteException('https://bar-block.com:443', {
              setting: ContentSetting.BLOCK,
            }),
            createRawSiteException('https://foo-block.com:443', {
              setting: ContentSetting.BLOCK,
            }),
          ]),
    ]);
  }

  // Runs the system permission warning test for a given content type.
  async function systemPermissionWarningTest(
      category: ContentSettingsTypes, categoryName: string) {
    setUpCategory(
        category, ContentSetting.ALLOW, create2AllowAnd2BlockPrefs(category));
    const contentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(category, contentType);
    assertEquals(2, testElement.sites.length);

    for (const disabled of [true, false]) {
      const blockedPermissions = disabled ? [category] : [];
      webUIListenerCallback('osGlobalPermissionChanged', blockedPermissions);

      const warningElement =
          testElement.$.category.querySelector<HTMLDivElement>(
              '#systemPermissionDeclinedWarning');
      assertTrue(!!warningElement);
      const linkElement =
          warningElement.querySelector('#openSystemSettingsLink');
      if (!disabled) {
        assertTrue(warningElement.hidden);
        assertEquals(warningElement.textContent, '');
        assertFalse(!!linkElement);
        return;
      }

      assertFalse(warningElement.hidden);
      const variant =
          warningElement.innerHTML.includes('Chromium') ? 'Chromium' : 'Chrome';
      assertEquals(
          warningElement.textContent,
          `To use your ${categoryName} on these sites,` +
              ` give ${variant} access in system settings`);
      assertTrue(!!linkElement);
      // Check that the link covers the right part of the warning.
      assertEquals('system settings', linkElement.innerHTML);
      // This is needed for the <a> to look like a link.
      assertEquals('#', linkElement.getAttribute('href'));
      // This is needed for accessibility. First letter if the category name is
      // capitalized.
      assertEquals(
          `System Settings: ${
              categoryName.replace(/^\w/, (c) => c.toUpperCase())}`,
          linkElement.getAttribute('aria-label'));

      linkElement.dispatchEvent(new MouseEvent('click'));
      await browserProxy.whenCalled('openSystemPermissionSettings')
          .then((contentType: string) => {
            assertEquals(category, contentType);
          });
    }
  }

  test('System permission warning for camera', async function() {
    await systemPermissionWarningTest(ContentSettingsTypes.CAMERA, 'camera');
  });

  test('System permission warning for microphone', async function() {
    await systemPermissionWarningTest(ContentSettingsTypes.MIC, 'microphone');
  });

  test('System permission warning for location', async function() {
    await systemPermissionWarningTest(
        ContentSettingsTypes.GEOLOCATION, 'location');
  });

  test('Empty list', async function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocationEmpty);
    const contentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(ContentSettingsTypes.GEOLOCATION, contentType);
    assertEquals(0, testElement.sites.length);
    assertEquals(ContentSetting.ALLOW, testElement.categorySubtype);
    assertFalse(testElement.$.category.hidden);
  });

  test('initial ALLOW state is correct', async function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocation);
    const contentType: ContentSettingsTypes =
        await browserProxy.whenCalled('getExceptionList');
    assertEquals(ContentSettingsTypes.GEOLOCATION, contentType);
    assertEquals(2, testElement.sites.length);
    assertEquals(
        prefsGeolocation.exceptions[contentType][0]!.origin,
        testElement.sites[0]!.origin);
    assertEquals(
        prefsGeolocation.exceptions[contentType][1]!.origin,
        testElement.sites[1]!.origin);
    assertEquals(ContentSetting.ALLOW, testElement.categorySubtype);
    flush();  // Populates action menu.
    openActionMenu(0);
    assertMenu(['Block', 'Edit', 'Remove']);
    assertFalse(testElement.$.category.hidden);
  });

  test('action menu closes when list changes', async function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocation);
    const actionMenu = testElement.shadowRoot!.querySelector('cr-action-menu')!;

    await browserProxy.whenCalled('getExceptionList');
    flush();  // Populates action menu.
    openActionMenu(0);
    assertTrue(actionMenu.open);
    browserProxy.resetResolver('getExceptionList');

    // Simulate a change in the underlying model.
    webUIListenerCallback(
        'contentSettingSitePermissionChanged',
        ContentSettingsTypes.GEOLOCATION);
    await browserProxy.whenCalled('getExceptionList');
    // Check that the action menu was closed.
    assertFalse(actionMenu.open);
  });

  test('exceptions are not reordered in non-ALL_SITES', async function() {
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.BLOCK,
        prefsMixedProvider);
    const contentType: ContentSettingsTypes =
        await browserProxy.whenCalled('getExceptionList');
    assertEquals(ContentSettingsTypes.GEOLOCATION, contentType);
    assertEquals(3, testElement.sites.length);
    for (let i = 0; i < testElement.sites.length; ++i) {
      const exception = prefsMixedProvider.exceptions[contentType][i]!;
      assertEquals(exception.origin, testElement.sites[i]!.origin);

      let expectedControlledBy =
          chrome.settingsPrivate.ControlledBy.PRIMARY_USER;
      if (exception.source === SiteSettingSource.EXTENSION ||
          exception.source === SiteSettingSource.HOSTED_APP) {
        expectedControlledBy = chrome.settingsPrivate.ControlledBy.EXTENSION;
      } else if (exception.source === SiteSettingSource.POLICY) {
        expectedControlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
      }

      assertEquals(expectedControlledBy, testElement.sites[i]!.controlledBy);
    }
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
              prefsGeolocation.exceptions[contentType][2]!.origin,
              testElement.sites[0]!.origin);
          assertEquals(
              prefsGeolocation.exceptions[contentType][3]!.origin,
              testElement.sites[1]!.origin);
          flush();  // Populates action menu.
          openActionMenu(0);
          assertMenu(['Allow', 'Edit', 'Remove']);

          assertFalse(testElement.$.category.hidden);
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
              prefsSessionOnly.exceptions[contentType][2]!.origin,
              testElement.sites[0]!.origin);

          flush();  // Populates action menu.
          openActionMenu(0);
          assertMenu(['Allow', 'Block', 'Edit', 'Remove']);

          assertFalse(testElement.$.category.hidden);
        });
  });

  test('initial INCOGNITO BLOCK state is correct', async function() {
    const contentType = ContentSettingsTypes.COOKIES;
    const categorySubtype = ContentSetting.BLOCK;
    setUpCategory(contentType, categorySubtype, prefsIncognito);

    const actualContentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(contentType, actualContentType);
    assertEquals(categorySubtype, testElement.categorySubtype);
    assertEquals(1, testElement.sites.length);
    assertEquals(
        prefsIncognito.exceptions[contentType][0]!.origin,
        testElement.sites[0]!.origin);

    flush();  // Populates action menu.
    openActionMenu(0);
    // 'Clear on exit' is visible as this is not an incognito item.
    assertMenu(['Allow', 'Delete on exit', 'Edit', 'Remove']);

    // Select 'Remove' from menu.
    const remove = testElement.shadowRoot!.querySelector<HTMLElement>('#reset');
    assertTrue(!!remove);
    remove!.click();
    const args =
        await browserProxy.whenCalled('resetCategoryPermissionForPattern');
    assertEquals('http://foo.com', args[0]);
    assertEquals('', args[1]);
    assertEquals(contentType, args[2]);
    assertFalse(args[3]);  // Incognito.
  });

  test('initial INCOGNITO ALLOW state is correct', async function() {
    const contentType = ContentSettingsTypes.COOKIES;
    const categorySubtype = ContentSetting.ALLOW;
    setUpCategory(contentType, categorySubtype, prefsIncognito);

    const actualContentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(contentType, actualContentType);
    assertEquals(categorySubtype, testElement.categorySubtype);
    assertEquals(2, testElement.sites.length);
    assertEquals(
        prefsIncognito.exceptions[contentType][1]!.origin,
        testElement.sites[0]!.origin);
    assertEquals(
        prefsIncognito.exceptions[contentType][2]!.origin,
        testElement.sites[1]!.origin);

    flush();  // Populates action menu.
    openActionMenu(0);
    // 'Clear on exit' is hidden for incognito items.
    assertMenu(['Block', 'Edit', 'Remove']);
    closeActionMenu();

    // Select 'Remove' from menu on 'foo.com'.
    openActionMenu(1);
    const remove = testElement.shadowRoot!.querySelector<HTMLElement>('#reset');
    assertTrue(!!remove);
    remove!.click();
    const args =
        await browserProxy.whenCalled('resetCategoryPermissionForPattern');
    assertEquals('http://foo.com', args[0]);
    assertEquals('', args[1]);
    assertEquals(contentType, args[2]);
    assertTrue(args[3]);  // Incognito.
  });

  test('reset button works for read-only content types', async function() {
    testElement.readOnlyList = true;
    flush();

    const contentType = ContentSettingsTypes.GEOLOCATION;
    const categorySubtype = ContentSetting.ALLOW;
    setUpCategory(contentType, categorySubtype, prefsOneEnabled);
    const actualContentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(contentType, actualContentType);
    assertEquals(categorySubtype, testElement.categorySubtype);

    assertEquals(1, testElement.sites.length);
    assertEquals(
        prefsOneEnabled.exceptions[contentType][0]!.origin,
        testElement.sites[0]!.origin);

    flush();

    const item = testElement.shadowRoot!.querySelector('site-list-entry')!;

    // Assert action button is hidden.
    const dots = item.$.actionMenuButton;
    assertTrue(!!dots);
    assertTrue(dots.hidden);

    // Assert reset button is visible.
    const resetButton =
        item.shadowRoot!.querySelector<HTMLElement>('#resetSite');
    assertTrue(!!resetButton);
    assertFalse(resetButton!.hidden);

    resetButton!.click();
    const args =
        await browserProxy.whenCalled('resetCategoryPermissionForPattern');
    assertEquals('https://foo-allow.com:443', args[0]);
    assertEquals('', args[1]);
    assertEquals(contentType, args[2]);
  });

  test('edit action menu opens edit exception dialog', async function() {
    setUpCategory(
        ContentSettingsTypes.COOKIES, ContentSetting.SESSION_ONLY,
        prefsSessionOnly);

    await browserProxy.whenCalled('getExceptionList');
    flush();  // Populates action menu.

    openActionMenu(0);
    assertMenu(['Allow', 'Block', 'Edit', 'Remove']);
    const menu = testElement.shadowRoot!.querySelector('cr-action-menu')!;
    assertTrue(menu.open);
    const edit = testElement.shadowRoot!.querySelector<HTMLElement>('#edit');
    assertTrue(!!edit);
    edit!.click();
    flush();
    assertFalse(menu.open);
    assertTrue(!!testElement.shadowRoot!.querySelector(
        'settings-edit-exception-dialog'));
  });

  test('edit dialog closes when incognito status changes', async function() {
    setUpCategory(
        ContentSettingsTypes.COOKIES, ContentSetting.BLOCK, prefsSessionOnly);

    await browserProxy.whenCalled('getExceptionList');
    flush();  // Populates action menu.

    openActionMenu(0);
    testElement.shadowRoot!.querySelector<HTMLElement>('#edit')!.click();
    flush();

    const dialog =
        testElement.shadowRoot!.querySelector('settings-edit-exception-dialog');
    assertTrue(!!dialog);
    const closeEventPromise = eventToPromise('close', dialog!);
    browserProxy.setIncognito(true);

    await closeEventPromise;
    assertFalse(!!testElement.shadowRoot!.querySelector(
        'settings-edit-exception-dialog'));
  });

  test('Block list open when Allow list is empty', async function() {
    // Prefs: One item in Block list, nothing in Allow list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.BLOCK, prefsOneDisabled);
    const actualContentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(contentType, actualContentType);
    await flushTasks();

    assertFalse(testElement.$.category.hidden);
    assertNotEquals(0, testElement.$.listContainer.offsetHeight);
  });

  test('Block list open when Allow list is not empty', async function() {
    // Prefs: Items in both Block and Allow list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.BLOCK, prefsGeolocation);
    const actualContentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(contentType, actualContentType);
    await flushTasks();

    assertFalse(testElement.$.category.hidden);
    assertNotEquals(0, testElement.$.listContainer.offsetHeight);
  });

  test('Allow list is always open (Block list empty)', async function() {
    // Prefs: One item in Allow list, nothing in Block list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.ALLOW, prefsOneEnabled);
    const actualContentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(contentType, actualContentType);
    await flushTasks();

    assertFalse(testElement.$.category.hidden);
    assertNotEquals(0, testElement.$.listContainer.offsetHeight);
  });

  test('Allow list is always open (Block list non-empty)', async function() {
    // Prefs: Items in both Block and Allow list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.ALLOW, prefsGeolocation);
    const actualContentType = await browserProxy.whenCalled('getExceptionList');
    assertEquals(contentType, actualContentType);
    await flushTasks();

    assertFalse(testElement.$.category.hidden);
    assertNotEquals(0, testElement.$.listContainer.offsetHeight);
  });

  test('Block list not hidden when empty', function() {
    // Prefs: One item in Allow list, nothing in Block list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.BLOCK, prefsOneEnabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertFalse(testElement.$.category.hidden);
        });
  });

  test('Allow list not hidden when empty', function() {
    // Prefs: One item in Block list, nothing in Allow list.
    const contentType = ContentSettingsTypes.GEOLOCATION;
    setUpCategory(contentType, ContentSetting.ALLOW, prefsOneDisabled);
    return browserProxy.whenCalled('getExceptionList')
        .then(function(actualContentType) {
          assertEquals(contentType, actualContentType);
          assertFalse(testElement.$.category.hidden);
        });
  });

  test('Mixed embeddingOrigin', async function() {
    setUpCategory(
        ContentSettingsTypes.IMAGES, ContentSetting.ALLOW,
        prefsMixedEmbeddingOrigin);
    await browserProxy.whenCalled('getExceptionList');
    // Required for firstItem to be found below.
    flush();
    // Validate that embeddingOrigin sites cannot be edited.
    const entries = testElement.shadowRoot!.querySelectorAll('site-list-entry');
    const firstItem = entries[0]!;
    assertTrue(firstItem.$.actionMenuButton.hidden);
    assertFalse(
        firstItem.shadowRoot!.querySelector<HTMLElement>('#resetSite')!.hidden);
    // Validate that non-embeddingOrigin sites can be edited.
    const secondItem = entries[1]!;
    assertFalse(secondItem.$.actionMenuButton.hidden);
    assertTrue(secondItem.shadowRoot!.querySelector<HTMLElement>(
                                         '#resetSite')!.hidden);
  });

  test('Isolated Web Apps', async function() {
    setUpCategory(
        ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ALLOW,
        prefsIsolatedWebApp);
    await browserProxy.whenCalled('getExceptionList');

    // Required for firstItem to be found below.
    flush();

    // Validate that IWAs cannot be edited.
    const entries = testElement.shadowRoot!.querySelectorAll('site-list-entry');
    const firstItem = entries[0]!;
    assertTrue(firstItem.$.actionMenuButton.hidden);
    assertFalse(
        firstItem.shadowRoot!.querySelector<HTMLElement>('#resetSite')!.hidden);

    // Validate that IWA displays app name and not origin.
    assertEquals(
        firstItem.shadowRoot!.querySelector<HTMLElement>(
                                 '.url-directionality')!.textContent!.trim(),
        prefsIsolatedWebApp!.exceptions!.notifications[0]!.displayName);

    // Validate that non-IWAs can be edited.
    const secondItem = entries[1]!;
    assertFalse(secondItem.$.actionMenuButton.hidden);
    assertTrue(secondItem.shadowRoot!.querySelector<HTMLElement>(
                                         '#resetSite')!.hidden);

    // Validate that non-IWA displays the displayName (in most cases same as
    // the origin).
    assertEquals(
        secondItem.shadowRoot!
            .querySelector<HTMLElement>(
                '.url-directionality')!.textContent!.trim(),
        prefsIsolatedWebApp!.exceptions!.notifications[1]!.displayName);
  });

  test('Mixed schemes (present and absent)', async function() {
    // Prefs: One item with scheme and one without.
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsMixedSchemes);
    // No further checks needed. If this fails, it will hang the test.
    await browserProxy.whenCalled('getExceptionList');
  });

  test('Select menu item', async function() {
    // Test for error: "Cannot read property 'origin' of undefined".
    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsGeolocation);
    await browserProxy.whenCalled('getExceptionList');
    flush();
    openActionMenu(0);
    const allow = testElement.shadowRoot!.querySelector<HTMLElement>('#allow');
    assertTrue(!!allow);
    allow!.click();
    await browserProxy.whenCalled('setCategoryPermissionForPattern');
  });

  test('Chrome Extension scheme', async function() {
    setUpCategory(
        ContentSettingsTypes.JAVASCRIPT, ContentSetting.BLOCK,
        prefsChromeExtension);
    await browserProxy.whenCalled('getExceptionList');
    flush();
    openActionMenu(0);
    assertMenu(['Allow', 'Edit', 'Remove']);

    const allow = testElement.shadowRoot!.querySelector<HTMLElement>('#allow');
    assertTrue(!!allow);
    allow!.click();
    const args =
        await browserProxy.whenCalled('setCategoryPermissionForPattern');
    assertEquals(
        'chrome-extension://cfhgfbfpcbnnbibfphagcjmgjfjmojfa/', args[0]);
    assertEquals('', args[1]);
    assertEquals(ContentSettingsTypes.JAVASCRIPT, args[2]);
    assertEquals(ContentSetting.ALLOW, args[3]);
  });

  test(
      'show-tooltip event fires on entry show common tooltip',
      async function() {
        setUpCategory(
            ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
            prefsGeolocation);
        await browserProxy.whenCalled('getExceptionList');
        flush();
        const entry =
            testElement.$.listContainer.querySelector('site-list-entry')!;
        const tooltip = testElement.$.tooltip;

        const testsParams = [
          ['a', testElement, new MouseEvent('mouseleave')],
          ['b', testElement, new MouseEvent('click')],
          ['c', testElement, new Event('blur')],
          ['d', tooltip, new MouseEvent('mouseenter')],
        ];
        for (const params of testsParams) {
          const text = params[0] as string;
          const eventTarget = params[1] as HTMLElement;
          const event = params[2] as MouseEvent;
          entry.fire('show-tooltip', {target: testElement, text});
          await microtasksFinished();
          assertFalse(tooltip.$.tooltip.hidden);
          assertEquals(text, tooltip.innerHTML.trim());
          eventTarget.dispatchEvent(event);
          await microtasksFinished();
          assertTrue(tooltip.$.tooltip.hidden);
        }
      });

  test(
      'Add site button is hidden for content settings that don\'t allow it',
      async function() {
        setUpCategory(
            ContentSettingsTypes.FILE_SYSTEM_WRITE, ContentSetting.ALLOW,
            prefsFileSystemWrite);
        await browserProxy.whenCalled('getExceptionList');
        flush();
        assertTrue(testElement.$.addSite.hidden);
      });

  test('Reset the last entry moves focus', async function() {
    setUpCategory(
        ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ALLOW,
        prefsOneEnabledNotification);
    await browserProxy.whenCalled('getExceptionList');

    await microtasksFinished();
    flush();  // Populates action menu.
    openActionMenu(0);
    await microtasksFinished();

    // Select 'Remove' from menu.
    const remove = testElement.shadowRoot!.querySelector<HTMLElement>('#reset');
    assertTrue(!!remove);
    remove.click();
    await browserProxy.whenCalled('resetCategoryPermissionForPattern');
    await microtasksFinished();

    // Resetting the last element should move the focus to the list's header.
    assertEquals(
        testElement.$.listHeader, testElement.shadowRoot!.activeElement);
  });

  test('Block the last allowed entry moves focus', async function() {
    setUpCategory(
        ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ALLOW,
        prefsOneEnabledNotification);
    await browserProxy.whenCalled('getExceptionList');

    await microtasksFinished();
    flush();  // Populates action menu.
    openActionMenu(0);
    await microtasksFinished();

    // Select 'block' from menu.
    const block = testElement.shadowRoot!.querySelector<HTMLElement>('#block');
    assertTrue(!!block);
    block.click();
    await browserProxy.whenCalled('setCategoryPermissionForPattern');
    await microtasksFinished();

    // Resetting the last element should move the focus to the list's header.
    assertEquals(
        testElement.$.listHeader, testElement.shadowRoot!.activeElement);
  });

  test('Allow the last blocked entry moves focus', async function() {
    setUpCategory(
        ContentSettingsTypes.NOTIFICATIONS, ContentSetting.BLOCK,
        prefsOneDisabledNotification);
    await browserProxy.whenCalled('getExceptionList');

    await microtasksFinished();
    flush();  // Populates action menu.
    openActionMenu(0);
    await microtasksFinished();

    // Select 'allow' from menu.
    const allow = testElement.shadowRoot!.querySelector<HTMLElement>('#allow');
    assertTrue(!!allow);
    allow.click();
    await browserProxy.whenCalled('setCategoryPermissionForPattern');
    await microtasksFinished();

    // Resetting the last element should move the focus to the list's header.
    assertEquals(
        testElement.$.listHeader, testElement.shadowRoot!.activeElement);
  });

  test('Reset not the last entry focuses the next entry', async function() {
    setUpCategory(
        ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ALLOW,
        prefsTwoEnabledNotification);
    await browserProxy.whenCalled('getExceptionList');

    await microtasksFinished();
    flush();  // Populates action menu.
    openActionMenu(0);
    await microtasksFinished();

    // Select 'Remove' from menu.
    const remove = testElement.shadowRoot!.querySelector<HTMLElement>('#reset');
    assertTrue(!!remove);
    remove.click();
    await browserProxy.whenCalled('resetCategoryPermissionForPattern');
    await microtasksFinished();

    const firstListEntry =
        testElement.$.listContainer.querySelectorAll('site-list-entry')[0];
    assertTrue(!!firstListEntry);

    // Focus a site’s list entry.
    assertEquals(firstListEntry, testElement.shadowRoot!.activeElement);
  });

  test(
      'Block not the last allowed entry focuses the next entry',
      async function() {
        setUpCategory(
            ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ALLOW,
            prefsTwoEnabledNotification);
        await browserProxy.whenCalled('getExceptionList');

        await microtasksFinished();
        flush();  // Populates action menu.
        openActionMenu(0);
        await microtasksFinished();

        // Select 'block' from menu.
        const block =
            testElement.shadowRoot!.querySelector<HTMLElement>('#block');
        assertTrue(!!block);
        block.click();
        await browserProxy.whenCalled('setCategoryPermissionForPattern');
        await microtasksFinished();

        const firstListEntry =
            testElement.$.listContainer.querySelectorAll('site-list-entry')[0];
        assertTrue(!!firstListEntry);

        // Focus a site’s list entry.
        assertEquals(firstListEntry, testElement.shadowRoot!.activeElement);
      });

  test(
      'Allow not the last blocked entry focuses the next entry',
      async function() {
        setUpCategory(
            ContentSettingsTypes.NOTIFICATIONS, ContentSetting.BLOCK,
            prefsTwoDisabledNotification);
        await browserProxy.whenCalled('getExceptionList');

        await microtasksFinished();
        flush();  // Populates action menu.
        openActionMenu(0);
        await microtasksFinished();

        // Select 'allow' from menu.
        const allow =
            testElement.shadowRoot!.querySelector<HTMLElement>('#allow');
        assertTrue(!!allow);
        allow.click();
        await browserProxy.whenCalled('setCategoryPermissionForPattern');
        await microtasksFinished();

        const firstListEntry =
            testElement.$.listContainer.querySelectorAll('site-list-entry')[0];
        assertTrue(!!firstListEntry);

        // Focus a site’s list entry.
        assertEquals(firstListEntry, testElement.shadowRoot!.activeElement);
      });

  test('Reset the last Geolocation entry moves focus', async function() {
    testElement.readOnlyList = true;
    flush();

    setUpCategory(
        ContentSettingsTypes.GEOLOCATION, ContentSetting.ALLOW,
        prefsOneEnabled);
    await browserProxy.whenCalled('getExceptionList');
    flush();

    const item = testElement.shadowRoot!.querySelector('site-list-entry')!;

    const resetButton =
        item.shadowRoot!.querySelector<HTMLElement>('#resetSite');
    assertTrue(!!resetButton);
    resetButton.click();
    await browserProxy.whenCalled('resetCategoryPermissionForPattern');
    await microtasksFinished();

    // Resetting the last element should move the focus to the list's header.
    assertEquals(
        testElement.$.listHeader, testElement.shadowRoot!.activeElement);
  });
});

suite('SiteListSearchTests', function() {
  /** A site list element created before each test. */
  let testElement: SiteListElement;

  /** The mock proxy object to use during test. */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

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
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('site-list');
    document.body.appendChild(testElement);
  });

  /**
   * Configures the test element for a particular category.
   * @param category The category to set up.
   * @param subtype Type of list to use.
   * @param prefs The prefs to use.
   */
  function setUpCategory(
      category: ContentSettingsTypes, subtype: ContentSetting,
      prefs: SiteSettingsPref) {
    browserProxy.setPrefs(prefs);
    testElement.categorySubtype = subtype;
    testElement.category = category;
  }

  test('no search lists all 1p and 3p allow exceptions', async function() {
    testElement.cookiesExceptionType = CookiesExceptionType.COMBINED;
    testElement.searchFilter = '';
    setUpCategory(
        ContentSettingsTypes.COOKIES, ContentSetting.ALLOW,
        prefsMixedCookiesExceptionTypes2);
    await browserProxy.whenCalled('getExceptionList');
    flush();

    // The mock data contains 4 allow exceptions.
    assertEquals(
        4,
        testElement.$.listContainer.querySelectorAll('site-list-entry').length);
  });

  test('search lists matching 1p and 3p allow exceptions', async function() {
    testElement.cookiesExceptionType = CookiesExceptionType.COMBINED;
    testElement.searchFilter = 'foo';
    setUpCategory(
        ContentSettingsTypes.COOKIES, ContentSetting.ALLOW,
        prefsMixedCookiesExceptionTypes2);
    await browserProxy.whenCalled('getExceptionList');
    flush();

    // The mock data contains 2 foo allow exceptions.
    assertEquals(
        2,
        testElement.$.listContainer.querySelectorAll('site-list-entry').length);
  });
});

suite('EditExceptionDialog', function() {
  let dialog: SettingsEditExceptionDialogElement;

  /**
   * The dialog tests don't call |getExceptionList| so the exception needs to
   * be processed as a |SiteException|.
   */
  let cookieException: SiteException;

  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

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
      description: '',
    };

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-edit-exception-dialog');
    dialog.model = cookieException;
    document.body.appendChild(dialog);
  });

  teardown(function() {
    dialog.remove();
  });

  test('invalid input', async function() {
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    assertFalse(input!.invalid);

    const actionButton = dialog.$.actionButton;
    assertTrue(!!actionButton);
    assertFalse(actionButton.disabled);

    // Simulate user input of whitespace only text.
    input!.value = '  ';
    await input.updateComplete;
    input!.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    flush();
    assertTrue(actionButton.disabled);
    assertTrue(input!.invalid);

    // Simulate user input of invalid text.
    browserProxy.setIsPatternValidForType(false);
    const expectedPattern = '*';
    input!.value = expectedPattern;
    await input.updateComplete;
    input!.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));

    const [pattern, _category] =
        await browserProxy.whenCalled('isPatternValidForType');
    assertEquals(expectedPattern, pattern);
    assertTrue(actionButton.disabled);
    assertTrue(input!.invalid);
  });

  test('action button calls proxy', async function() {
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    // Simulate user edit.
    const newValue = input!.value + ':1234';
    input!.value = newValue;
    await input.updateComplete;

    const actionButton = dialog.$.actionButton;
    assertTrue(!!actionButton);
    assertFalse(actionButton.disabled);

    actionButton.click();
    const args1 =
        await browserProxy.whenCalled('resetCategoryPermissionForPattern');
    assertEquals(cookieException.origin, args1[0]);
    assertEquals(cookieException.embeddingOrigin, args1[1]);
    assertEquals(ContentSettingsTypes.COOKIES, args1[2]);
    assertEquals(cookieException.incognito, args1[3]);

    const args2 =
        await browserProxy.whenCalled('setCategoryPermissionForPattern');
    assertEquals(newValue, args2[0]);
    assertEquals(SITE_EXCEPTION_WILDCARD, args2[1]);
    assertEquals(ContentSettingsTypes.COOKIES, args2[2]);
    assertEquals(cookieException.setting, args2[3]);
    assertEquals(cookieException.incognito, args2[4]);

    assertFalse(dialog.$.dialog.open);
  });
});

suite('AddExceptionDialog', function() {
  let dialog: AddSiteDialogElement;
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  async function inputText(expectedPattern: string) {
    const actionButton = dialog.$.add;
    assertTrue(!!actionButton);
    assertTrue(actionButton.disabled);

    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    input.value = expectedPattern;
    await input.updateComplete;
    input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));

    const [pattern, _category] =
        await browserProxy.whenCalled('isPatternValidForType');
    assertEquals(expectedPattern, pattern);
    assertFalse(actionButton.disabled);
  }

  setup(function() {
    populateTestExceptions();

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('add-site-dialog');
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
    assertFalse(dialog.$.incognito.checked);
    dialog.$.incognito.checked = true;
    // Changing the incognito status will reset the checkbox.
    dialog.set('hasIncognito', false);
    flush();
    assertFalse(dialog.$.incognito.checked);
  });

  test('invalid input', async function() {
    // Initially the action button should be disabled, but the error warning
    // should not be shown for an empty input.
    const input = dialog.shadowRoot!.querySelector('cr-input');
    assertTrue(!!input);
    assertFalse(input!.invalid);

    const actionButton = dialog.$.add;
    assertTrue(!!actionButton);
    assertTrue(actionButton.disabled);

    // Simulate user input of invalid text.
    browserProxy.setIsPatternValidForType(false);
    const expectedPattern = 'foobarbaz';
    input!.value = expectedPattern;
    await input.updateComplete;
    input!.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));

    const [pattern, _category] =
        await browserProxy.whenCalled('isPatternValidForType');
    assertEquals(expectedPattern, pattern);
    assertTrue(actionButton.disabled);
    assertTrue(input!.invalid);
  });

  test(
      'add cookie exception for combined cookie exception type',
      async function() {
        dialog.set('category', ContentSettingsTypes.COOKIES);
        dialog.set('cookiesExceptionType', CookiesExceptionType.COMBINED);
        flush();

        // Enter a pattern and click the button.
        const expectedPattern = 'foo-bar.com';
        await inputText(expectedPattern);
        dialog.$.add.click();

        // The created exception has secondary pattern wildcard
        // (created site data cookie exception).
        const [primaryPattern, secondaryPattern] =
            await browserProxy.whenCalled('setCategoryPermissionForPattern');
        assertEquals(primaryPattern, expectedPattern);
        assertEquals(secondaryPattern, SITE_EXCEPTION_WILDCARD);
      });

  test('add third party cookie exception', async function() {
    dialog.set('category', ContentSettingsTypes.COOKIES);
    dialog.set('cookiesExceptionType', CookiesExceptionType.THIRD_PARTY);
    flush();

    // Enter a pattern and click the button.
    const expectedPattern = 'foo-bar.com';
    await inputText(expectedPattern);
    dialog.$.add.click();

    // The created exception has primary pattern wildcard (third party
    // exception).
    const [primaryPattern, secondaryPattern] =
        await browserProxy.whenCalled('setCategoryPermissionForPattern');
    assertEquals(primaryPattern, SITE_EXCEPTION_WILDCARD);
    assertEquals(secondaryPattern, expectedPattern);
  });

  test('add site data cookie exception', async function() {
    dialog.set('category', ContentSettingsTypes.COOKIES);
    dialog.set('cookiesExceptionType', CookiesExceptionType.SITE_DATA);
    flush();

    // Enter a pattern and click the button.
    const expectedPattern = 'foo-bar.com';
    await inputText(expectedPattern);
    dialog.$.add.click();

    // The created exception has secondary pattern wildcard (site data
    // exception).
    const [primaryPattern, secondaryPattern] =
        await browserProxy.whenCalled('setCategoryPermissionForPattern');
    assertEquals(primaryPattern, expectedPattern);
    assertEquals(secondaryPattern, SITE_EXCEPTION_WILDCARD);
  });

  test('add tracking protection exception', async function() {
    dialog.set('category', ContentSettingsTypes.TRACKING_PROTECTION);
    flush();

    // Enter a pattern and click the button.
    const expectedPattern = 'foo-bar.com';
    await inputText(expectedPattern);
    dialog.$.add.click();

    // The created exception has primary pattern wildcard.
    const [primaryPattern, secondaryPattern] =
        await browserProxy.whenCalled('setCategoryPermissionForPattern');
    assertEquals(primaryPattern, SITE_EXCEPTION_WILDCARD);
    assertEquals(secondaryPattern, expectedPattern);
  });
});
