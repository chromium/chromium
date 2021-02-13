// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {beforeNextRender,flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ContentSetting,ContentSettingsTypes,LocalDataBrowserProxyImpl,SiteSettingsPrefsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, Router,routes} from 'chrome://settings/settings.js';
import {TestLocalDataBrowserProxy} from 'chrome://test/settings/test_local_data_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from 'chrome://test/settings/test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair,createOriginInfo,createRawSiteException,createSiteGroup,createSiteSettingsPrefs} from 'chrome://test/settings/test_util.js';

// clang-format on

suite('AllSites', function() {
  const TEST_COOKIE_LIST = {
    id: 'example',
    children: [
      {domain: 'bar.com'},
      {domain: 'bar.com'},
      {domain: 'bar.com'},
      {domain: 'bar.com'},
      {domain: 'google.com'},
      {domain: 'google.com'},
    ]
  };

  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   * @type {!SiteGroup}
   */
  const TEST_MULTIPLE_SITE_GROUP = createSiteGroup('example.com', [
    'http://example.com',
    'https://www.example.com',
    'https://login.example.com',
  ]);

  /**
   * An example pref with multiple categories and multiple allow/block
   * state.
   * @type {SiteSettingsPref}
   */
  let prefsVarious;

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
   * The mock local data proxy object to use during test.
   * @type {TestLocalDataBrowserProxy}
   */
  let localDataBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  // Initialize a site-list before each test.
  setup(async function() {
    PolymerTest.clearBody();

    prefsVarious = createSiteSettingsPrefs([], [
      createContentSettingTypeToValuePair(
          ContentSettingsTypes.GEOLOCATION,
          [
            createRawSiteException('https://foo.com'),
            createRawSiteException('https://bar.com', {
              setting: ContentSetting.BLOCK,
            })
          ]),
      createContentSettingTypeToValuePair(
          ContentSettingsTypes.NOTIFICATIONS,
          [
            createRawSiteException('https://google.com', {
              setting: ContentSetting.BLOCK,
            }),
            createRawSiteException('https://bar.com', {
              setting: ContentSetting.BLOCK,
            }),
            createRawSiteException('https://foo.com', {
              setting: ContentSetting.BLOCK,
            }),
          ])
    ]);
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    localDataBrowserProxy = new TestLocalDataBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
    LocalDataBrowserProxyImpl.instance_ = localDataBrowserProxy;
    testElement = document.createElement('all-sites');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    Router.getInstance().resetRouteForTesting();
  });

  /**
   * Configures the test element.
   * @param {Array<dictionary>} prefs The prefs to use.
   * @param {string=} sortOrder the URL param used to establish default sort
   * order.
   */
  function setUpAllSites(prefs, sortOrder) {
    browserProxy.setPrefs(prefs);
    if (sortOrder) {
      Router.getInstance().navigateTo(
          routes.SITE_SETTINGS_ALL, new URLSearchParams(`sort=${sortOrder}`));
    } else {
      Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
    }
  }

  test('All sites list populated', async function() {
    setUpAllSites(prefsVarious);
    testElement.populateList_();
    await browserProxy.whenCalled('getAllSites');
    assertEquals(3, testElement.siteGroupMap.size);

    // Flush to be sure list container is populated.
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(3, siteEntries.length);
  });

  test('search query filters list', async function() {
    const SEARCH_QUERY = 'foo';
    setUpAllSites(prefsVarious);
    testElement.populateList_();
    await browserProxy.whenCalled('getAllSites');
    // Flush to be sure list container is populated.
    flush();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(3, siteEntries.length);

    testElement.filter = SEARCH_QUERY;
    flush();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    const hiddenSiteEntries =
        testElement.shadowRoot.querySelectorAll('site-entry[hidden]');
    assertEquals(1, siteEntries.length - hiddenSiteEntries.length);

    for (let i = 0; i < siteEntries; ++i) {
      const entry = siteEntries[i];
      if (!hiddenSiteEntries.includes(entry)) {
        assertTrue(entry.siteGroup.origins.some((origin) => {
          return origin.includes(SEARCH_QUERY);
        }));
      }
    }
  });

  test('can be sorted by most visited', function() {
    setUpAllSites(prefsVarious);
    testElement.populateList_();

    return browserProxy.whenCalled('getAllSites').then(() => {
      // Add additional origins and artificially insert fake engagement scores
      // to sort.
      assertEquals(3, testElement.siteGroupMap.size);
      const fooSiteGroup = testElement.siteGroupMap.get('foo.com');
      fooSiteGroup.origins.push(
          createOriginInfo('https://login.foo.com', {engagement: 20}));
      assertEquals(2, fooSiteGroup.origins.length);
      fooSiteGroup.origins[0].engagement = 50.4;
      const googleSiteGroup = testElement.siteGroupMap.get('google.com');
      assertEquals(1, googleSiteGroup.origins.length);
      googleSiteGroup.origins[0].engagement = 55.1261;
      const barSiteGroup = testElement.siteGroupMap.get('bar.com');
      assertEquals(1, barSiteGroup.origins.length);
      barSiteGroup.origins[0].engagement = 0.5235;

      // 'Most visited' is the default sort method, so sort by a different
      // method first to ensure changing to 'Most visited' works.
      testElement.root.querySelector('select').value = 'name';
      testElement.onSortMethodChanged_();
      flush();
      let siteEntries =
          testElement.$.listContainer.querySelectorAll('site-entry');
      assertEquals('bar.com', siteEntries[0].$.displayName.innerText.trim());
      assertEquals('foo.com', siteEntries[1].$.displayName.innerText.trim());
      assertEquals('google.com', siteEntries[2].$.displayName.innerText.trim());

      testElement.root.querySelector('select').value = 'most-visited';
      testElement.onSortMethodChanged_();
      flush();
      siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
      // Each site entry is sorted by its maximum engagement, so expect
      // 'foo.com' to come after 'google.com'.
      assertEquals('google.com', siteEntries[0].$.displayName.innerText.trim());
      assertEquals('foo.com', siteEntries[1].$.displayName.innerText.trim());
      assertEquals('bar.com', siteEntries[2].$.displayName.innerText.trim());
    });
  });

  test('can be sorted by storage', async function() {
    localDataBrowserProxy.setCookieDetails(TEST_COOKIE_LIST);
    setUpAllSites(prefsVarious);
    testElement.populateList_();
    await browserProxy.whenCalled('getAllSites');
    flush();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    // Add additional origins to SiteGroups with cookies to simulate their
    // being grouped entries, plus add local storage.
    siteEntries[0].siteGroup.origins[0].usage = 900;
    siteEntries[1].siteGroup.origins.push(createOriginInfo('http://bar.com'));
    siteEntries[1].siteGroup.origins[0].usage = 500;
    siteEntries[1].siteGroup.origins[1].usage = 500;
    siteEntries[2].siteGroup.origins.push(
        createOriginInfo('http://google.com'));

    testElement.onSortMethodChanged_();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    // Verify all sites is not sorted by storage.
    assertEquals(3, siteEntries.length);
    assertEquals('foo.com', siteEntries[0].$.displayName.innerText.trim());
    assertEquals('bar.com', siteEntries[1].$.displayName.innerText.trim());
    assertEquals('google.com', siteEntries[2].$.displayName.innerText.trim());

    // Change the sort method, then verify all sites is now sorted by
    // name.
    testElement.root.querySelector('select').value = 'data-stored';
    testElement.onSortMethodChanged_();


    flush();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(
        'bar.com',
        siteEntries[0]
            .root.querySelector('#displayName .url-directionality')
            .innerText.trim());
    assertEquals(
        'foo.com',
        siteEntries[1]
            .root.querySelector('#displayName .url-directionality')
            .innerText.trim());
    assertEquals(
        'google.com',
        siteEntries[2]
            .root.querySelector('#displayName .url-directionality')
            .innerText.trim());
  });

  test('can be sorted by storage by passing URL param', async function() {
    // The default sorting (most visited) will have the ascending storage
    // values. With the URL param, we expect the sites to be sorted by usage in
    // descending order.
    document.body.innerHTML = '';
    setUpAllSites(prefsVarious, 'data-stored');
    testElement = document.createElement('all-sites');
    document.body.appendChild(testElement);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');

    assertEquals(
        'google.com',
        siteEntries[0]
            .root.querySelector('#displayName .url-directionality')
            .innerText.trim());
    assertEquals(
        'bar.com',
        siteEntries[1]
            .root.querySelector('#displayName .url-directionality')
            .innerText.trim());
    assertEquals(
        'foo.com',
        siteEntries[2]
            .root.querySelector('#displayName .url-directionality')
            .innerText.trim());
  });

  test('can be sorted by name', async function() {
    setUpAllSites(prefsVarious);
    testElement.populateList_();
    await browserProxy.whenCalled('getAllSites');
    flush();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');

    // Verify all sites is not sorted by name.
    assertEquals(3, siteEntries.length);
    assertEquals('foo.com', siteEntries[0].$.displayName.innerText.trim());
    assertEquals('bar.com', siteEntries[1].$.displayName.innerText.trim());
    assertEquals('google.com', siteEntries[2].$.displayName.innerText.trim());

    // Change the sort method, then verify all sites is now sorted by name.
    testElement.root.querySelector('select').value = 'name';
    testElement.onSortMethodChanged_();
    flush();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals('bar.com', siteEntries[0].$.displayName.innerText.trim());
    assertEquals('foo.com', siteEntries[1].$.displayName.innerText.trim());
    assertEquals('google.com', siteEntries[2].$.displayName.innerText.trim());
  });

  test('can sort by name by passing URL param', async function() {
    document.body.innerHTML = '';
    setUpAllSites(prefsVarious, 'name');
    testElement = document.createElement('all-sites');
    document.body.appendChild(testElement);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');

    assertEquals('bar.com', siteEntries[0].$.displayName.innerText.trim());
    assertEquals('foo.com', siteEntries[1].$.displayName.innerText.trim());
    assertEquals('google.com', siteEntries[2].$.displayName.innerText.trim());
  });

  test('merging additional SiteGroup lists works', async function() {
    setUpAllSites(prefsVarious);
    testElement.populateList_();
    await browserProxy.whenCalled('getAllSites');
    flush();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(3, siteEntries.length);

    // Pretend an additional set of SiteGroups were added.
    const fooEtldPlus1 = 'foo.com';
    const addEtldPlus1 = 'additional-site.net';
    const fooOrigin = 'https://login.foo.com';
    const addOrigin = 'http://www.additional-site.net';
    const STORAGE_SITE_GROUP_LIST = /** @type {!Array{!SiteGroup}}*/ ([
      {
        // Test merging an existing site works, with overlapping origin lists.
        'etldPlus1': fooEtldPlus1,
        'origins': [
          createOriginInfo(fooOrigin),
          createOriginInfo('https://foo.com'),
        ],
      },
      {
        // Test adding a new site entry works.
        'etldPlus1': addEtldPlus1,
        'origins': [createOriginInfo(addOrigin)],
      }
    ]);
    testElement.onStorageListFetched(STORAGE_SITE_GROUP_LIST);

    flush();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(4, siteEntries.length);

    assertEquals(fooEtldPlus1, siteEntries[0].siteGroup.etldPlus1);
    assertEquals(2, siteEntries[0].siteGroup.origins.length);
    assertEquals(fooOrigin, siteEntries[0].siteGroup.origins[0].origin);
    assertEquals('https://foo.com', siteEntries[0].siteGroup.origins[1].origin);

    assertEquals(addEtldPlus1, siteEntries[3].siteGroup.etldPlus1);
    assertEquals(1, siteEntries[3].siteGroup.origins.length);
    assertEquals(addOrigin, siteEntries[3].siteGroup.origins[0].origin);
  });

  function resetSettingsViaOverflowMenu(buttonType) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);
    const overflowMenuButton = siteEntries[0].$.overflowMenuButton;
    assertFalse(overflowMenuButton.closest('.row-aligned').hidden);
    // Open the reset settings dialog.
    const overflowMenu = testElement.$.menu.get();
    const menuItems = overflowMenu.querySelectorAll('.dropdown-item');

    // Test clicking on the overflow menu button opens the menu.
    assertFalse(overflowMenu.open);
    overflowMenuButton.click();
    assertTrue(overflowMenu.open);

    // Open the reset settings dialog and tap the |buttonType| button.
    assertFalse(testElement.$.confirmResetSettings.get().open);
    menuItems[0].click();
    assertTrue(testElement.$.confirmResetSettings.get().open);
    const actionButtonList =
        testElement.$.confirmResetSettings.get().getElementsByClassName(
            buttonType);
    assertEquals(1, actionButtonList.length);
    actionButtonList[0].click();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(testElement.$.confirmResetSettings.get().open);
    assertFalse(overflowMenu.open);
  }

  test('cancelling the confirm dialog on resetting settings works', function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdate_();
    resetSettingsViaOverflowMenu('cancel-button');
  });

  test('reset settings via overflow menu (no data or cookies)', function() {
    // Test when entire siteGroup has no data or cookies.
    // Clone this object to avoid propagating changes made in this test.
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdate_();
    resetSettingsViaOverflowMenu('action-button');
    // Ensure a call was made to setOriginPermissions for each origin.
    assertEquals(
        TEST_MULTIPLE_SITE_GROUP.origins.length,
        browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(testElement.filteredList_.length, 0);
  });

  test(
      'reset settings via overflow menu (one has data and cookies)',
      function() {
        // Test when one origin has data and cookies.
        // Clone this object to avoid propagating changes made in this test.
        const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        siteGroup.origins[0].hasPermissionSettings = true;
        siteGroup.origins[0].usage = 100;
        siteGroup.origins[0].numCookies = 2;
        testElement.siteGroupMap.set(
            siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
        testElement.forceListUpdate_();
        resetSettingsViaOverflowMenu('action-button');
        assertEquals(testElement.filteredList_.length, 1);
        assertEquals(1, testElement.filteredList_[0].origins.length);
        assertFalse(
            testElement.filteredList_[0].origins[0].hasPermissionSettings);
        assertEquals(testElement.filteredList_[0].origins[0].usage, 100);
        assertEquals(testElement.filteredList_[0].origins[0].numCookies, 2);
      });

  test('reset settings via overflow menu (etld+1 has cookies)', function() {
    // Test when none of origin have data or cookies, but etld+1 has
    // cookies. In this case, a placeholder origin will be created with the
    // Etld+1 cookies number. Clone this object to avoid propagating changes
    // made in this test.
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    siteGroup.numCookies = 5;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdate_();
    resetSettingsViaOverflowMenu('action-button');
    assertEquals(testElement.filteredList_.length, 1);
    assertEquals(1, testElement.filteredList_[0].origins.length);
    assertFalse(testElement.filteredList_[0].origins[0].hasPermissionSettings);
    assertEquals(testElement.filteredList_[0].origins[0].usage, 0);
    assertEquals(testElement.filteredList_[0].origins[0].numCookies, 5);
  });

  function clearDataViaOverflowMenu(buttonType) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);
    const overflowMenuButton = siteEntries[0].$.overflowMenuButton;
    assertFalse(overflowMenuButton.closest('.row-aligned').hidden);

    // Open the clear data dialog.
    const overflowMenu = testElement.$.menu.get();
    const menuItems = overflowMenu.querySelectorAll('.dropdown-item');
    // Test clicking on the overflow menu button opens the menu.
    assertFalse(overflowMenu.open);
    overflowMenuButton.click();
    assertTrue(overflowMenu.open);

    // Open the clear data dialog and tap the |buttonType| button.
    assertFalse(testElement.$.confirmClearDataNew.get().open);
    menuItems[1].click();
    assertTrue(testElement.$.confirmClearDataNew.get().open);
    const actionButtonList =
        testElement.$.confirmClearDataNew.get().getElementsByClassName(
            buttonType);
    assertEquals(1, actionButtonList.length);
    testElement.actionMenuModel_ = {
      index: 0,
      item: testElement.filteredList_[0],
    };
    actionButtonList[0].click();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(testElement.$.confirmClearDataNew.get().open);
    assertFalse(overflowMenu.open);
  }

  test('cancelling the confirm dialog on clear data works', function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdate_();
    clearDataViaOverflowMenu('cancel-button');
  });

  test('clear data via overflow menu (no permission and no data)', function() {
    // Test when all origins has no permission settings and no data.
    // Clone this object to avoid propagating changes made in this test.
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdate_();
    clearDataViaOverflowMenu('action-button');
    // Ensure a call was made to clearEtldPlus1DataAndCookies.
    assertEquals(1, browserProxy.getCallCount('clearEtldPlus1DataAndCookies'));
    assertEquals(testElement.filteredList_.length, 0);
  });

  test('clear data via overflow menu (one origin has permission)', function() {
    // Test when there is one origin has permissions settings.
    // Clone this object to avoid propagating changes made in this test.
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    siteGroup.origins[0].hasPermissionSettings = true;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdate_();
    clearDataViaOverflowMenu('action-button');
    assertEquals(testElement.filteredList_.length, 1);
    assertEquals(testElement.filteredList_[0].origins.length, 1);
  });

  test(
      'clear data via overflow menu (one origin has permission and data)',
      function() {
        // Test when one origin has permission settings and data, clear data
        // only clears the data and cookies.
        const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        siteGroup.origins[0].hasPermissionSettings = true;
        siteGroup.origins[0].usage = 100;
        siteGroup.origins[0].numCookies = 3;
        testElement.siteGroupMap.set(
            siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
        testElement.forceListUpdate_();
        clearDataViaOverflowMenu('action-button');
        assertEquals(testElement.filteredList_.length, 1);
        assertEquals(testElement.filteredList_[0].origins.length, 1);
        assertTrue(
            testElement.filteredList_[0].origins[0].hasPermissionSettings);
        assertEquals(testElement.filteredList_[0].origins[0].usage, 0);
        assertEquals(testElement.filteredList_[0].origins[0].numCookies, 0);
      });

  function clearDataViaClearAllButton(buttonType) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertTrue(siteEntries.length >= 1);
    const clearAllButton =
        testElement.$.clearAllButton.querySelector('cr-button');
    const confirmClearAllData = testElement.$.confirmClearAllData.get();

    // Open the clear all data dialog.
    // Test clicking on the clear all button opens the clear all dialog.
    assertFalse(confirmClearAllData.open);
    clearAllButton.click();
    assertTrue(confirmClearAllData.open);

    // Open the clear data dialog and tap the |buttonType| button.
    const actionButtonList =
        testElement.$.confirmClearAllData.get().getElementsByClassName(
            buttonType);
    assertEquals(1, actionButtonList.length);
    actionButtonList[0].click();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(confirmClearAllData.open);
  }

  test('cancelling the confirm dialog on clear all data works', function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdate_();
    clearDataViaClearAllButton('cancel-button');
  });

  test('clearing data via clear all dialog', function() {
    // Test when all origins has no permission settings and no data.
    // Clone this object to avoid propagating changes made in this test.
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    const googleSiteGroup = createSiteGroup('google.com', [
      'https://www.google.com',
      'https://docs.google.com',
      'https://mail.google.com',
    ]);
    testElement.siteGroupMap.set(googleSiteGroup.etldPlus1, googleSiteGroup);
    testElement.forceListUpdate_();
    clearDataViaClearAllButton('action-button');
    // Ensure a call was made to clearEtldPlus1DataAndCookies.
    assertEquals(2, browserProxy.getCallCount('clearEtldPlus1DataAndCookies'));
    assertEquals(testElement.filteredList_.length, 0);
  });

  test(
      'clear data via clear all button (one origin has permission)',
      function() {
        // Test when there is one origin has permissions settings.
        // Clone this object to avoid propagating changes made in this test.
        const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        siteGroup.origins[0].hasPermissionSettings = true;
        testElement.siteGroupMap.set(
            siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
        const googleSiteGroup = createSiteGroup('google.com', [
          'https://www.google.com',
          'https://docs.google.com',
          'https://mail.google.com',
        ]);
        testElement.siteGroupMap.set(
            googleSiteGroup.etldPlus1, googleSiteGroup);
        testElement.forceListUpdate_();
        assertEquals(testElement.filteredList_.length, 2);
        assertEquals(
            testElement.filteredList_[0].origins.length,
            siteGroup.origins.length);
        clearDataViaClearAllButton('action-button');
        assertEquals(testElement.filteredList_.length, 1);
        assertEquals(testElement.filteredList_[0].origins.length, 1);
      });

  /**
   * Opens the overflow menu for a specific origin within a SiteEntry, clicks
   * on the clear data option, and then clicks on either the cancel or clear
   * data button.
   * @param {string} buttonType The button to click on the clear data dialog
   * @param {!SiteGroup} siteGroup The SiteGroup for which the origin to clear
   *     belongs to.
   * @param {number} originIndex The index of the origin to clear in the
   *     SiteGroup.origins array.
   */
  function clearOriginDataViaOverflowMenu(buttonType, siteGroup, originIndex) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);

    const expandButton = siteEntries[0].$.expandIcon;
    // Open the overflow menu.
    const overflowMenu = testElement.$.menu.get();
    assertFalse(overflowMenu.open);
    testElement.fire('open-menu', {
      target: expandButton,
      index: 0,
      item: siteGroup,
      origin: siteGroup.origins[originIndex].origin,
      actionScope: 'origin',
    });
    assertTrue(overflowMenu.open);

    const menuItems = overflowMenu.querySelectorAll('.dropdown-item');

    // Open the clear data dialog and tap the |buttonType| button.
    assertFalse(testElement.$.confirmClearDataNew.get().open);
    menuItems[1].click();
    assertTrue(testElement.$.confirmClearDataNew.get().open);
    const actionButtonList =
        testElement.$.confirmClearDataNew.get().getElementsByClassName(
            buttonType);
    assertEquals(1, actionButtonList.length);
    testElement.actionMenuModel_ = {
      index: 0,
      item: testElement.filteredList_[0],
      origin: siteGroup.origins[originIndex].origin,
      actionScope: 'origin',
    };
    actionButtonList[0].click();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(testElement.$.confirmClearDataNew.get().open);
    assertFalse(overflowMenu.open);
  }

  test('cancelling the confirm dialog on clear data works', function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testElement.siteGroupMap.set(siteGroup.etldPlus1, siteGroup);
    testElement.forceListUpdate_();
    assertEquals(1, testElement.filteredList_.length);
    assertEquals(3, testElement.filteredList_[0].origins.length);
    clearOriginDataViaOverflowMenu('cancel-button', siteGroup, 0);
    assertEquals(1, testElement.filteredList_.length);
    assertEquals(3, testElement.filteredList_[0].origins.length);
  });

  test('clear single origin data via overflow menu', function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    siteGroup.origins[0].hasPermissionSettings = false;
    siteGroup.origins[0].usage = 100;
    siteGroup.origins[0].numCookies = 3;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdate_();
    clearOriginDataViaOverflowMenu('action-button', siteGroup, 0);
    assertEquals(1, testElement.filteredList_.length);
    assertEquals(2, testElement.filteredList_[0].origins.length);
  });

  test(
      'clear single origin data via overflow menu (has permissions)',
      function() {
        const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        siteGroup.origins[0].hasPermissionSettings = true;
        siteGroup.origins[0].usage = 100;
        siteGroup.origins[0].numCookies = 3;
        testElement.siteGroupMap.set(
            siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
        testElement.forceListUpdate_();
        clearOriginDataViaOverflowMenu('action-button', siteGroup, 0);
        assertEquals(1, testElement.filteredList_.length);
        assertEquals(3, testElement.filteredList_[0].origins.length);

        const updatedOrigin = testElement.filteredList_[0].origins[0];
        assertTrue(updatedOrigin.hasPermissionSettings);
        assertEquals(0, updatedOrigin.usage);
        assertEquals(0, updatedOrigin.numCookies);
      });

  /**
   * Clicks on the overflow menu for a specific origin, hits the reset
   * permissions button on the overflow menu, and takes the specified action on
   * the confirmation dialog.
   * @param {string} buttonType The button to click on the confirmation dialog.
   * @param {!SiteGroup} siteGroup The SiteGroup to which the origin to reset
   *     belongs to.
   * @param {number} originIndex The index in the SiteGroup.origins array of the
   *     origin to reset permissions for.
   */
  function resetOriginSettingsViaOverflowMenu(
      buttonType, siteGroup, originIndex) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);

    const expandButton = siteEntries[0].$.expandIcon;
    // Open the overflow menu.
    const overflowMenu = testElement.$.menu.get();
    assertFalse(overflowMenu.open);
    testElement.fire('open-menu', {
      target: expandButton,
      index: 0,
      item: siteGroup,
      origin: siteGroup.origins[originIndex].origin,
      actionScope: 'origin',
    });
    assertTrue(overflowMenu.open);

    const menuItems = overflowMenu.querySelectorAll('.dropdown-item');

    // Open the clear data dialog and tap the |buttonType| button.
    assertFalse(testElement.$.confirmResetSettings.get().open);
    menuItems[0].click();
    assertTrue(testElement.$.confirmResetSettings.get().open);
    const actionButtonList =
        testElement.$.confirmResetSettings.get().getElementsByClassName(
            buttonType);
    assertEquals(1, actionButtonList.length);
    testElement.actionMenuModel_ = {
      index: 0,
      item: testElement.filteredList_[0],
      origin: siteGroup.origins[originIndex].origin,
      actionScope: 'origin',
    };
    actionButtonList[0].click();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(testElement.$.confirmResetSettings.get().open);
    assertFalse(overflowMenu.open);
  }

  test('cancelling the confirm dialog on resetting settings works', function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testElement.siteGroupMap.set(siteGroup.etldPlus1, siteGroup);
    testElement.forceListUpdate_();
    assertEquals(1, testElement.filteredList_.length);
    assertEquals(3, testElement.filteredList_[0].origins.length);
    resetOriginSettingsViaOverflowMenu('cancel-button', siteGroup, 0);
    assertEquals(1, testElement.filteredList_.length);
    assertEquals(3, testElement.filteredList_[0].origins.length);
  });

  test(
      'clear single origin permissions via overflow menu (no usage/cookies)',
      function() {
        const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        siteGroup.origins[0].hasPermissionSettings = true;
        siteGroup.origins[0].usage = 0;
        siteGroup.origins[0].numCookies = 0;
        testElement.siteGroupMap.set(
            siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
        testElement.forceListUpdate_();
        resetOriginSettingsViaOverflowMenu('action-button', siteGroup, 0);
        assertEquals(1, testElement.filteredList_.length);
        assertEquals(2, testElement.filteredList_[0].origins.length);
      });

  test(
      'clear single origin permissions via overflow menu (has usage/cookies)',
      function() {
        const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        siteGroup.origins[0].hasPermissionSettings = true;
        siteGroup.origins[0].usage = 100;
        siteGroup.origins[0].numCookies = 10;
        testElement.siteGroupMap.set(
            siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
        testElement.forceListUpdate_();
        resetOriginSettingsViaOverflowMenu('action-button', siteGroup, 0);
        assertEquals(1, testElement.filteredList_.length);
        assertEquals(3, testElement.filteredList_[0].origins.length);
      });
});
