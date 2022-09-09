// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AllSitesElement, ContentSetting, ContentSettingsTypes, LocalDataBrowserProxyImpl, SiteGroup, SiteSettingsPrefsBrowserProxyImpl, SortMethod} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isChildVisible} from 'chrome://webui-test/test_util.js';

import {TestLocalDataBrowserProxy} from './test_local_data_browser_proxy.js';
import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import {createContentSettingTypeToValuePair, createOriginInfo, createRawSiteException, createSiteGroup, createSiteSettingsPrefs, SiteSettingsPref} from './test_util.js';

// clang-format on

suite('AllSites_DisabledConsolidatedControls', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   */
  const TEST_MULTIPLE_SITE_GROUP = createSiteGroup('example.com', [
    'http://example.com',
    'https://www.example.com',
    'https://login.example.com',
  ]);

  /**
   * An example pref with multiple categories and multiple allow/block
   * state.
   */
  let prefsVarious: SiteSettingsPref;

  let testElement: AllSitesElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  /**
   * The mock local data proxy object to use during test.
   */
  let localDataBrowserProxy: TestLocalDataBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      consolidatedSiteStorageControlsEnabled: false,
    });
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  // Initialize a site-list before each test.
  setup(async function() {
    document.body.innerHTML = '';

    prefsVarious = createSiteSettingsPrefs([], [
      createContentSettingTypeToValuePair(
          ContentSettingsTypes.GEOLOCATION,
          [
            createRawSiteException('https://foo.com'),
            createRawSiteException('https://bar.com', {
              setting: ContentSetting.BLOCK,
            }),
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
          ]),
    ]);
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    localDataBrowserProxy = new TestLocalDataBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    LocalDataBrowserProxyImpl.setInstance(localDataBrowserProxy);
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
   * @param prefs The prefs to use.
   * @param sortOrder the URL param used to establish default sort order.
   */
  function setUpAllSites(prefs: SiteSettingsPref, sortOrder?: SortMethod) {
    browserProxy.setPrefs(prefs);
    if (sortOrder) {
      Router.getInstance().navigateTo(
          routes.SITE_SETTINGS_ALL, new URLSearchParams(`sort=${sortOrder}`));
    } else {
      Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
    }
  }

  function getSubstitutedString(messageId: string, substitute: string): string {
    return loadTimeData.substituteString(
        testElement.i18n(messageId), substitute);
  }

  test('All sites list populated', async function() {
    setUpAllSites(prefsVarious);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
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
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    // Flush to be sure list container is populated.
    flush();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(3, siteEntries.length);

    testElement.filter = SEARCH_QUERY;
    flush();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    const hiddenSiteEntries = Array.from(
        testElement.shadowRoot!.querySelectorAll('site-entry[hidden]'));
    assertEquals(1, siteEntries.length - hiddenSiteEntries.length);

    for (const entry of siteEntries) {
      if (!hiddenSiteEntries.includes(entry)) {
        assertTrue(entry.siteGroup.origins.some(origin => {
          return origin.origin.includes(SEARCH_QUERY);
        }));
      }
    }
  });

  test('can be sorted by most visited', async function() {
    setUpAllSites(prefsVarious);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);

    await browserProxy.whenCalled('getAllSites');

    // Add additional origins and artificially insert fake engagement scores
    // to sort.
    assertEquals(3, testElement.siteGroupMap.size);
    const fooSiteGroup = testElement.siteGroupMap.get('foo.com')!;
    fooSiteGroup.origins.push(
        createOriginInfo('https://login.foo.com', {engagement: 20}));
    assertEquals(2, fooSiteGroup.origins.length);
    fooSiteGroup.origins[0]!.engagement = 50.4;
    const googleSiteGroup = testElement.siteGroupMap.get('google.com')!;
    assertEquals(1, googleSiteGroup.origins.length);
    googleSiteGroup.origins[0]!.engagement = 55.1261;
    const barSiteGroup = testElement.siteGroupMap.get('bar.com')!;
    assertEquals(1, barSiteGroup.origins.length);
    barSiteGroup.origins[0]!.engagement = 0.5235;

    // 'Most visited' is the default sort method, so sort by a different
    // method first to ensure changing to 'Most visited' works.
    testElement.$.sortMethod.value = 'name';
    testElement.$.sortMethod.dispatchEvent(new CustomEvent('change'));
    flush();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals('bar.com', siteEntries[0]!.$.displayName.innerText.trim());
    assertEquals('foo.com', siteEntries[1]!.$.displayName.innerText.trim());
    assertEquals('google.com', siteEntries[2]!.$.displayName.innerText.trim());

    testElement.$.sortMethod.value = 'most-visited';
    testElement.$.sortMethod.dispatchEvent(new CustomEvent('change'));
    flush();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    // Each site entry is sorted by its maximum engagement, so expect
    // 'foo.com' to come after 'google.com'.
    assertEquals('google.com', siteEntries[0]!.$.displayName.innerText.trim());
    assertEquals('foo.com', siteEntries[1]!.$.displayName.innerText.trim());
    assertEquals('bar.com', siteEntries[2]!.$.displayName.innerText.trim());
  });

  test('dynamic filtered clear data button strings', async function() {
    setUpAllSites(prefsVarious);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');

    const clearAllButton = testElement.$.clearAllButton;
    assertEquals(
        loadTimeData.getString('siteSettingsClearAllStorageLabel'),
        clearAllButton.innerText.trim());

    // Setting a filter, text should change.
    testElement.filter = 'foo';
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsClearDisplayedStorageLabel'),
        clearAllButton.innerText.trim());

    // Removing the filter.
    testElement.filter = '';
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsClearAllStorageLabel'),
        clearAllButton.innerText.trim());
  });

  test('dynamic filtered total usage strings', async function() {
    setUpAllSites(prefsVarious);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    // Removing extra unwanted site entries.
    testElement.siteGroupMap.delete('google.com');
    testElement.forceListUpdateForTesting();
    await flushTasks();

    const clearLabel = testElement.$.clearLabel;
    assertEquals(
        getSubstitutedString('siteSettingsClearAllStorageDescription', '100 B'),
        clearLabel.innerText.trim());

    // Setting a filter, 'foo.com' exists but doesn't have storage.
    testElement.filter = 'foo';
    await flushTasks();
    assertEquals(
        getSubstitutedString(
            'siteSettingsClearDisplayedStorageDescription', '0 B'),
        clearLabel.innerText.trim());

    // Changing the filter, 100 B given to 'bar.com; in the test proxy.
    testElement.filter = 'bar';
    await flushTasks();
    assertEquals(
        getSubstitutedString(
            'siteSettingsClearDisplayedStorageDescription', '100 B'),
        clearLabel.innerText.trim());

    // Remove the filter, without changing the total amount of storage.
    testElement.filter = '';
    await flushTasks();
    assertEquals(
        getSubstitutedString('siteSettingsClearAllStorageDescription', '100 B'),
        clearLabel.innerText.trim());
  });

  test('dynamic filtered clear data confirmation strings', async function() {
    interface DialogState {
      filter: boolean;
      appInstalled: boolean;
      storage: boolean;
      title: string;
      description: string;
      signout: string;
    }

    const dialogStates: DialogState[] = [
      {
        filter: false,
        appInstalled: false,
        storage: true,
        title: 'siteSettingsClearAllStorageDialogTitle',
        description: 'siteSettingsClearAllStorageConfirmation',
        signout: 'siteSettingsClearAllStorageSignOut',
      },
      {
        filter: false,
        appInstalled: true,
        storage: true,
        title: 'siteSettingsClearAllStorageDialogTitle',
        description: 'siteSettingsClearAllStorageConfirmationInstalled',
        signout: 'siteSettingsClearAllStorageSignOut',
      },
      {
        filter: true,
        appInstalled: false,
        storage: true,
        title: 'siteSettingsClearDisplayedStorageDialogTitle',
        description: 'siteSettingsClearDisplayedStorageConfirmation',
        signout: 'siteSettingsClearDisplayedStorageSignOut',
      },
      {
        filter: true,
        appInstalled: false,
        storage: false,
        title: 'siteSettingsClearDisplayedStorageDialogTitle',
        description: 'siteSettingsClearDisplayedStorageConfirmation',
        signout: 'siteSettingsClearDisplayedStorageSignOut',
      },
      {
        filter: true,
        appInstalled: true,
        storage: false,
        title: 'siteSettingsClearDisplayedStorageDialogTitle',
        description: 'siteSettingsClearDisplayedStorageConfirmationInstalled',
        signout: 'siteSettingsClearDisplayedStorageSignOut',
      },
      {
        filter: true,
        appInstalled: true,
        storage: true,
        title: 'siteSettingsClearDisplayedStorageDialogTitle',
        description: 'siteSettingsClearDisplayedStorageConfirmationInstalled',
        signout: 'siteSettingsClearDisplayedStorageSignOut',
      },
    ];

    setUpAllSites(prefsVarious);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    // Removing extra unwanted site entries.
    testElement.siteGroupMap.delete('google.com');

    const clearAllButton =
        testElement.$.clearAllButton.querySelector('cr-button')!;
    const confirmClearAllData = testElement.$.confirmClearAllData.get();

    // Open the clear all data dialog.
    assertFalse(confirmClearAllData.open, 'closed dialog');
    clearAllButton.click();
    assertTrue(confirmClearAllData.open, 'open dialog');

    for (const state of dialogStates) {
      assertTrue(state.filter || state.storage, 'valid state');

      if (state.storage) {  // 'bar' has storage of 100 B
        testElement.filter = state.filter ? 'bar' : '';
      } else {  // 'foo' has storage of 0 B
        testElement.filter = state.filter ? 'foo' : '';
      }

      testElement.siteGroupMap.get('foo.com')!.hasInstalledPWA =
          state.appInstalled;
      testElement.siteGroupMap.get('bar.com')!.hasInstalledPWA =
          state.appInstalled;

      testElement.forceListUpdateForTesting();
      await flushTasks();

      const confirmationTitle =
          confirmClearAllData.querySelector<HTMLElement>(
                                 '[slot=title]')!.innerText.trim();
      const confirmationDescription =
          confirmClearAllData
              .querySelector<HTMLElement>(
                  '#clearAllStorageDialogDescription')!.innerText.trim();
      const confirmationSignOutLabel =
          confirmClearAllData
              .querySelector<HTMLElement>(
                  '#clearAllStorageDialogSignOutLabel')!.innerText.trim();

      assertEquals(loadTimeData.getString(state.title), confirmationTitle);
      assertEquals(
          getSubstitutedString(
              state.description, state.storage ? '100 B' : '0 B'),
          confirmationDescription);
      assertEquals(
          loadTimeData.getString(state.signout), confirmationSignOutLabel);
    }
  });

  test('can be sorted by storage', async function() {
    setUpAllSites(prefsVarious);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    flush();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    // Add additional origins to SiteGroups with cookies to simulate their
    // being grouped entries, plus add local storage.
    siteEntries[0]!.siteGroup.origins[0]!.usage = 900;
    siteEntries[1]!.siteGroup.origins.push(createOriginInfo('http://bar.com'));
    siteEntries[1]!.siteGroup.origins[0]!.usage = 500;
    siteEntries[1]!.siteGroup.origins[1]!.usage = 500;
    siteEntries[2]!.siteGroup.origins.push(
        createOriginInfo('http://google.com'));

    testElement.$.sortMethod.dispatchEvent(new CustomEvent('change'));
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    // Verify all sites is not sorted by storage.
    assertEquals(3, siteEntries.length);
    assertEquals('foo.com', siteEntries[0]!.$.displayName.innerText.trim());
    assertEquals('bar.com', siteEntries[1]!.$.displayName.innerText.trim());
    assertEquals('google.com', siteEntries[2]!.$.displayName.innerText.trim());

    // Change the sort method, then verify all sites is now sorted by
    // name.
    testElement.$.sortMethod.value = 'data-stored';
    testElement.$.sortMethod.dispatchEvent(new CustomEvent('change'));

    flush();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(
        'bar.com',
        siteEntries[0]!.shadowRoot!
            .querySelector<HTMLElement>(
                '#displayName .url-directionality')!.innerText.trim());
    assertEquals(
        'foo.com',
        siteEntries[1]!.shadowRoot!
            .querySelector<HTMLElement>(
                '#displayName .url-directionality')!.innerText.trim());
    assertEquals(
        'google.com',
        siteEntries[2]!.shadowRoot!
            .querySelector<HTMLElement>(
                '#displayName .url-directionality')!.innerText.trim());
  });

  test('can be sorted by storage by passing URL param', async function() {
    // The default sorting (most visited) will have the ascending storage
    // values. With the URL param, we expect the sites to be sorted by usage in
    // descending order.
    document.body.innerHTML = '';
    setUpAllSites(prefsVarious, SortMethod.STORAGE);
    testElement = document.createElement('all-sites');
    document.body.appendChild(testElement);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');

    assertEquals(
        'google.com',
        siteEntries[0]!.shadowRoot!
            .querySelector<HTMLElement>(
                '#displayName .url-directionality')!.innerText.trim());
    assertEquals(
        'bar.com',
        siteEntries[1]!.shadowRoot!
            .querySelector<HTMLElement>(
                '#displayName .url-directionality')!.innerText.trim());
    assertEquals(
        'foo.com',
        siteEntries[2]!.shadowRoot!
            .querySelector<HTMLElement>(
                '#displayName .url-directionality')!.innerText.trim());
  });

  test('can be sorted by name', async function() {
    setUpAllSites(prefsVarious);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    flush();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');

    // Verify all sites is not sorted by name.
    assertEquals(3, siteEntries.length);
    assertEquals('foo.com', siteEntries[0]!.$.displayName.innerText.trim());
    assertEquals('bar.com', siteEntries[1]!.$.displayName.innerText.trim());
    assertEquals('google.com', siteEntries[2]!.$.displayName.innerText.trim());

    // Change the sort method, then verify all sites is now sorted by name.
    testElement.$.sortMethod.value = 'name';
    testElement.$.sortMethod.dispatchEvent(new CustomEvent('change'));
    flush();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals('bar.com', siteEntries[0]!.$.displayName.innerText.trim());
    assertEquals('foo.com', siteEntries[1]!.$.displayName.innerText.trim());
    assertEquals('google.com', siteEntries[2]!.$.displayName.innerText.trim());
  });

  test('can sort by name by passing URL param', async function() {
    document.body.innerHTML = '';
    setUpAllSites(prefsVarious, SortMethod.NAME);
    testElement = document.createElement('all-sites');
    document.body.appendChild(testElement);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');

    assertEquals('bar.com', siteEntries[0]!.$.displayName.innerText.trim());
    assertEquals('foo.com', siteEntries[1]!.$.displayName.innerText.trim());
    assertEquals('google.com', siteEntries[2]!.$.displayName.innerText.trim());
  });

  test('merging additional SiteGroup lists works', async function() {
    setUpAllSites(prefsVarious);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
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
    const STORAGE_SITE_GROUP_LIST: SiteGroup[] = [
      {
        // Test merging an existing site works, with overlapping origin lists.
        etldPlus1: fooEtldPlus1,
        origins: [
          createOriginInfo(fooOrigin),
          createOriginInfo('https://foo.com'),
        ],
        hasInstalledPWA: false,
        numCookies: 0,
      },
      {
        // Test adding a new site entry works.
        etldPlus1: addEtldPlus1,
        origins: [createOriginInfo(addOrigin)],
        hasInstalledPWA: false,
        numCookies: 0,
      },
    ];
    testElement.onStorageListFetched(STORAGE_SITE_GROUP_LIST);

    flush();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(4, siteEntries.length);

    assertEquals(fooEtldPlus1, siteEntries[0]!.siteGroup.etldPlus1);
    assertEquals(2, siteEntries[0]!.siteGroup.origins.length);
    assertEquals(fooOrigin, siteEntries[0]!.siteGroup.origins[0]!.origin);
    assertEquals(
        'https://foo.com', siteEntries[0]!.siteGroup.origins[1]!.origin);

    assertEquals(addEtldPlus1, siteEntries[3]!.siteGroup.etldPlus1);
    assertEquals(1, siteEntries[3]!.siteGroup.origins.length);
    assertEquals(addOrigin, siteEntries[3]!.siteGroup.origins[0]!.origin);
  });

  function resetSettingsViaOverflowMenu(buttonType: string) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);
    const overflowMenuButton =
        siteEntries[0]!.$$<HTMLElement>('#overflowMenuButton')!;
    assertFalse(
        overflowMenuButton.closest<HTMLElement>('.row-aligned')!.hidden);

    // Open the reset settings dialog.
    // Test clicking on the overflow menu button opens the menu.
    const overflowMenu = testElement.$.menu.get();
    assertFalse(overflowMenu.open);
    overflowMenuButton.click();
    assertTrue(overflowMenu.open);
    flush();
    const menuItems =
        overflowMenu.querySelectorAll<HTMLElement>('.dropdown-item');
    // Open the reset settings dialog and tap the |buttonType| button.
    assertFalse(testElement.$.confirmResetSettings.get().open);
    menuItems[0]!.click();
    assertTrue(testElement.$.confirmResetSettings.get().open);
    const actionButtonList =
        testElement.$.confirmResetSettings.get().querySelectorAll<HTMLElement>(
            `.${buttonType}`);
    assertEquals(1, actionButtonList.length);
    actionButtonList[0]!.click();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(testElement.$.confirmResetSettings.get().open);
    assertFalse(overflowMenu.open);
  }

  test('cancelling the confirm dialog on resetting settings works', function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdateForTesting();
    resetSettingsViaOverflowMenu('cancel-button');
  });

  test('reset settings via overflow menu (no data or cookies)', function() {
    // Test when entire siteGroup has no data or cookies.
    // Clone this object to avoid propagating changes made in this test.
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdateForTesting();
    resetSettingsViaOverflowMenu('action-button');
    // Ensure a call was made to setOriginPermissions for each origin.
    assertEquals(
        TEST_MULTIPLE_SITE_GROUP.origins.length,
        browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(testElement.$.allSitesList.items!.length, 0);
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
        testElement.forceListUpdateForTesting();
        resetSettingsViaOverflowMenu('action-button');
        assertEquals(testElement.$.allSitesList.items!.length, 1);
        assertEquals(1, testElement.$.allSitesList.items![0].origins.length);
        assertFalse(testElement.$.allSitesList.items![0]
                        .origins[0]
                        .hasPermissionSettings);
        assertEquals(
            testElement.$.allSitesList.items![0].origins[0].usage, 100);
        assertEquals(
            testElement.$.allSitesList.items![0].origins[0].numCookies, 2);
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
    testElement.forceListUpdateForTesting();
    resetSettingsViaOverflowMenu('action-button');
    assertEquals(testElement.$.allSitesList.items!.length, 1);
    assertEquals(1, testElement.$.allSitesList.items![0].origins.length);
    assertFalse(
        testElement.$.allSitesList.items![0].origins[0].hasPermissionSettings);
    assertEquals(testElement.$.allSitesList.items![0].origins[0].usage, 0);
    assertEquals(testElement.$.allSitesList.items![0].origins[0].numCookies, 5);
  });

  function clearDataViaOverflowMenu(buttonType: string) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);
    const overflowMenuButton =
        siteEntries[0]!.$$<HTMLElement>('#overflowMenuButton')!;
    assertFalse(
        overflowMenuButton.closest<HTMLElement>('.row-aligned')!.hidden);

    // Open the clear data dialog.
    const overflowMenu = testElement.$.menu.get();
    // Test clicking on the overflow menu button opens the menu.
    assertFalse(overflowMenu.open);
    overflowMenuButton.click();
    assertTrue(overflowMenu.open);
    flush();
    const menuItems =
        overflowMenu.querySelectorAll<HTMLElement>('.dropdown-item');

    // Open the clear data dialog and tap the |buttonType| button.
    assertFalse(testElement.$.confirmClearData.get().open);
    menuItems[1]!.click();
    assertTrue(testElement.$.confirmClearData.get().open);
    const actionButtonList =
        testElement.$.confirmClearData.get().querySelectorAll<HTMLElement>(
            `.${buttonType}`);
    assertEquals(1, actionButtonList.length);
    actionButtonList[0]!.click();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(testElement.$.confirmClearData.get().open);
    assertFalse(overflowMenu.open);
  }

  test('cancelling the confirm dialog on clear data works', function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdateForTesting();
    clearDataViaOverflowMenu('cancel-button');
  });

  test('clear data via overflow menu (no permission and no data)', function() {
    // Test when all origins has no permission settings and no data.
    // Clone this object to avoid propagating changes made in this test.
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdateForTesting();
    clearDataViaOverflowMenu('action-button');
    // Ensure a call was made to clearEtldPlus1DataAndCookies.
    assertEquals(1, browserProxy.getCallCount('clearEtldPlus1DataAndCookies'));
    assertEquals(testElement.$.allSitesList.items!.length, 0);
  });

  test('clear data via overflow menu (one origin has permission)', function() {
    // Test when there is one origin has permissions settings.
    // Clone this object to avoid propagating changes made in this test.
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    siteGroup.origins[0].hasPermissionSettings = true;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    clearDataViaOverflowMenu('action-button');
    assertEquals(testElement.$.allSitesList.items!.length, 1);
    assertEquals(testElement.$.allSitesList.items![0].origins.length, 1);
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
        testElement.forceListUpdateForTesting();
        clearDataViaOverflowMenu('action-button');
        assertEquals(testElement.$.allSitesList.items!.length, 1);
        assertEquals(testElement.$.allSitesList.items![0].origins.length, 1);
        assertTrue(testElement.$.allSitesList.items![0]
                       .origins[0]
                       .hasPermissionSettings);
        assertEquals(testElement.$.allSitesList.items![0].origins[0].usage, 0);
        assertEquals(
            testElement.$.allSitesList.items![0].origins[0].numCookies, 0);
      });

  function clearDataViaClearAllButton(buttonType: string) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertTrue(siteEntries.length >= 1);
    const clearAllButton =
        testElement.$.clearAllButton.querySelector('cr-button')!;
    const confirmClearAllData = testElement.$.confirmClearAllData.get();

    // Open the clear all data dialog.
    // Test clicking on the clear all button opens the clear all dialog.
    assertFalse(confirmClearAllData.open);
    clearAllButton.click();
    assertTrue(confirmClearAllData.open);

    // Open the clear data dialog and tap the |buttonType| button.
    const actionButtonList =
        testElement.$.confirmClearAllData.get().querySelectorAll<HTMLElement>(
            `.${buttonType}`);
    assertEquals(1, actionButtonList.length);
    actionButtonList[0]!.click();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(confirmClearAllData.open);
  }

  test('cancelling the confirm dialog on clear all data works', function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdateForTesting();
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
    testElement.forceListUpdateForTesting();
    clearDataViaClearAllButton('action-button');
    // Ensure a call was made to clearEtldPlus1DataAndCookies.
    assertEquals(2, browserProxy.getCallCount('clearEtldPlus1DataAndCookies'));
    assertEquals(testElement.$.allSitesList.items!.length, 0);
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
        testElement.forceListUpdateForTesting();
        assertEquals(testElement.$.allSitesList.items!.length, 2);
        assertEquals(
            testElement.$.allSitesList.items![0].origins.length,
            siteGroup.origins.length);
        clearDataViaClearAllButton('action-button');
        assertEquals(testElement.$.allSitesList.items!.length, 1);
        assertEquals(testElement.$.allSitesList.items![0].origins.length, 1);
      });

  /**
   * Opens the overflow menu for a specific origin within a SiteEntry, clicks
   * on the clear data option, and then clicks on either the cancel or clear
   * data button.
   * @param buttonType The button to click on the clear data dialog
   * @param siteGroup The SiteGroup for which the origin to clear
   *     belongs to.
   * @param originIndex The index of the origin to clear in the
   *     SiteGroup.origins array.
   */
  function clearOriginDataViaOverflowMenu(
      buttonType: string, siteGroup: SiteGroup, originIndex: number) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);

    const expandButton = siteEntries[0]!.$.expandIcon;
    // Open the overflow menu.
    const overflowMenu = testElement.$.menu.get();
    assertFalse(overflowMenu.open);
    testElement.dispatchEvent(new CustomEvent('open-menu', {
      bubbles: true,
      composed: true,
      detail: {
        target: expandButton,
        index: 0,
        item: siteGroup,
        origin: siteGroup.origins[originIndex]!.origin,
        actionScope: 'origin',
      },
    }));
    assertTrue(overflowMenu.open);
    flush();
    const menuItems =
        overflowMenu.querySelectorAll<HTMLElement>('.dropdown-item');

    // Open the clear data dialog and tap the |buttonType| button.
    assertFalse(testElement.$.confirmClearData.get().open);
    menuItems[1]!.click();
    assertTrue(testElement.$.confirmClearData.get().open);
    const actionButtonList =
        testElement.$.confirmClearData.get().querySelectorAll<HTMLElement>(
            `.${buttonType}`);
    assertEquals(1, actionButtonList.length);
    actionButtonList[0]!.click();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(testElement.$.confirmClearData.get().open);
    assertFalse(overflowMenu.open);
  }

  test('cancelling the confirm dialog on clear data works', function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testElement.siteGroupMap.set(siteGroup.etldPlus1, siteGroup);
    testElement.forceListUpdateForTesting();
    assertEquals(1, testElement.$.allSitesList.items!.length);
    assertEquals(3, testElement.$.allSitesList.items![0].origins.length);
    clearOriginDataViaOverflowMenu('cancel-button', siteGroup, 0);
    assertEquals(1, testElement.$.allSitesList.items!.length);
    assertEquals(3, testElement.$.allSitesList.items![0].origins.length);
  });

  test('clear single origin data via overflow menu', function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    siteGroup.origins[0].hasPermissionSettings = false;
    siteGroup.origins[0].usage = 100;
    siteGroup.origins[0].numCookies = 3;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    clearOriginDataViaOverflowMenu('action-button', siteGroup, 0);
    assertEquals(1, testElement.$.allSitesList.items!.length);
    assertEquals(2, testElement.$.allSitesList.items![0].origins.length);
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
        testElement.forceListUpdateForTesting();
        clearOriginDataViaOverflowMenu('action-button', siteGroup, 0);
        assertEquals(1, testElement.$.allSitesList.items!.length);
        assertEquals(3, testElement.$.allSitesList.items![0].origins.length);

        const updatedOrigin = testElement.$.allSitesList.items![0].origins[0];
        assertTrue(updatedOrigin.hasPermissionSettings);
        assertEquals(0, updatedOrigin.usage);
        assertEquals(0, updatedOrigin.numCookies);
      });

  /**
   * Clicks on the overflow menu for a specific origin, hits the reset
   * permissions button on the overflow menu, and takes the specified action on
   * the confirmation dialog.
   * @param buttonType The button to click on the confirmation dialog.
   * @param siteGroup The SiteGroup to which the origin to reset
   *     belongs to.
   * @param originIndex The index in the SiteGroup.origins array of the
   *     origin to reset permissions for.
   */
  function resetOriginSettingsViaOverflowMenu(
      buttonType: string, siteGroup: SiteGroup, originIndex: number) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);

    const expandButton = siteEntries[0]!.$.expandIcon;
    // Open the overflow menu.
    const overflowMenu = testElement.$.menu.get();
    assertFalse(overflowMenu.open);
    testElement.dispatchEvent(new CustomEvent('open-menu', {
      bubbles: true,
      composed: true,
      detail: {
        target: expandButton,
        index: 0,
        item: siteGroup,
        origin: siteGroup.origins[originIndex]!.origin,
        actionScope: 'origin',
      },
    }));
    assertTrue(overflowMenu.open);
    flush();
    const menuItems =
        overflowMenu.querySelectorAll<HTMLElement>('.dropdown-item');

    // Open the clear data dialog and tap the |buttonType| button.
    assertFalse(testElement.$.confirmResetSettings.get().open);
    menuItems[0]!.click();
    assertTrue(testElement.$.confirmResetSettings.get().open);
    const actionButtonList =
        testElement.$.confirmResetSettings.get().querySelectorAll<HTMLElement>(
            `.${buttonType}`);
    assertEquals(1, actionButtonList.length);
    actionButtonList[0]!.click();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(testElement.$.confirmResetSettings.get().open);
    assertFalse(overflowMenu.open);
  }

  test('cancelling the confirm dialog on resetting settings works', function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testElement.siteGroupMap.set(siteGroup.etldPlus1, siteGroup);
    testElement.forceListUpdateForTesting();
    assertEquals(1, testElement.$.allSitesList.items!.length);
    assertEquals(3, testElement.$.allSitesList.items![0].origins.length);
    resetOriginSettingsViaOverflowMenu('cancel-button', siteGroup, 0);
    assertEquals(1, testElement.$.allSitesList.items!.length);
    assertEquals(3, testElement.$.allSitesList.items![0].origins.length);
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
        testElement.forceListUpdateForTesting();
        resetOriginSettingsViaOverflowMenu('action-button', siteGroup, 0);
        assertEquals(1, testElement.$.allSitesList.items!.length);
        assertEquals(2, testElement.$.allSitesList.items![0].origins.length);
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
        testElement.forceListUpdateForTesting();
        resetOriginSettingsViaOverflowMenu('action-button', siteGroup, 0);
        assertEquals(1, testElement.$.allSitesList.items!.length);
        assertEquals(3, testElement.$.allSitesList.items![0].origins.length);
      });
});

suite('AllSites_EnabledConsolidatedControls', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   */
  const TEST_MULTIPLE_SITE_GROUP = createSiteGroup('example.com', [
    'http://subdomain.example.com/',
    'https://www.example.com/',
    'https://login.example.com/',
  ]);

  /**
   * An example eTLD+1 Object with a single origin grouped under it.
   */
  const TEST_SINGLE_SITE_GROUP = createSiteGroup('example.com', [
    'https://single.example.com',
  ]);

  let testElement: AllSitesElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  /**
   * The mock local data proxy object to use during test.
   */
  let localDataBrowserProxy: TestLocalDataBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      consolidatedSiteStorageControlsEnabled: true,
    });
  });

  // Initialize a site-list before each test.
  setup(async function() {
    document.body.innerHTML = '';

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    localDataBrowserProxy = new TestLocalDataBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    LocalDataBrowserProxyImpl.setInstance(localDataBrowserProxy);
    testElement = document.createElement('all-sites');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);
  });

  function removeFirstOrigin() {
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);
    const originList = siteEntries[0]!.$.originList.get();
    flush();
    const originEntries = originList.querySelectorAll('.hr');
    assertEquals(3, originEntries.length);
    originEntries[0]!.querySelector<HTMLElement>(
                         '#removeOriginButton')!.click();
  }

  function removeFirstSiteGroup() {
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);
    siteEntries[0]!.$$<HTMLElement>('#removeSiteButton')!.click();
  }

  function confirmDialog() {
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    testElement.$.confirmRemoveSite.get()
        .querySelector<HTMLElement>('.action-button')!.click();
  }

  function cancelDialog() {
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    testElement.$.confirmRemoveSite.get()
        .querySelector<HTMLElement>('.cancel-button')!.click();
  }

  function getString(messageId: string): string {
    return testElement.i18n(messageId);
  }

  function getSubstitutedString(messageId: string, substitute: string): string {
    return loadTimeData.substituteString(
        testElement.i18n(messageId), substitute);
  }

  test('remove site group', function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstSiteGroup();
    confirmDialog();

    assertEquals(
        TEST_MULTIPLE_SITE_GROUP.origins.length,
        browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(0, testElement.$.allSitesList.items!.length);
    assertEquals(1, browserProxy.getCallCount('clearEtldPlus1DataAndCookies'));
  });

  test('remove origin', async function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    siteGroup.origins[0].numCookies = 1;
    siteGroup.origins[1].numCookies = 2;
    siteGroup.origins[2].numCookies = 3;
    siteGroup.numCookies = 6;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstOrigin();
    confirmDialog();

    assertEquals(
        siteGroup.origins[0].origin,
        await browserProxy.whenCalled(
            'clearUnpartitionedOriginDataAndCookies'));

    const [origin, types, setting] =
        await browserProxy.whenCalled('setOriginPermissions');
    assertEquals(origin, siteGroup.origins[0].origin);
    assertEquals(types, null);  // Null affects all content types.
    assertEquals(setting, ContentSetting.DEFAULT);

    assertEquals(
        1, browserProxy.getCallCount('clearUnpartitionedOriginDataAndCookies'));
    assertEquals(1, browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(5, testElement.$.allSitesList.items![0].numCookies);
  });

  test('remove partitioned origin', async function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    siteGroup.origins[0].isPartitioned = true;
    siteGroup.origins[0].numCookies = 1;
    siteGroup.origins[1].numCookies = 2;
    siteGroup.origins[2].numCookies = 3;
    siteGroup.numCookies = 6;

    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    flush();

    // Remove the the partitioned entry, which will have been ordered to the
    // bottom of the displayed origins.
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    const originList = siteEntries[0]!.$.originList.get();
    flush();
    const originEntries = originList.querySelectorAll('.hr');
    assertEquals(3, originEntries.length);
    originEntries[2]!.querySelector<HTMLElement>(
                         '#removeOriginButton')!.click();
    confirmDialog();

    const [origin, etldPlus1] =
        await browserProxy.whenCalled('clearPartitionedOriginDataAndCookies');

    assertEquals(siteGroup.origins[0].origin, origin);
    assertEquals(siteGroup.etldPlus1, etldPlus1);
    assertEquals(
        1, browserProxy.getCallCount('clearPartitionedOriginDataAndCookies'));
    assertEquals(0, browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(5, testElement.$.allSitesList.items![0].numCookies);
  });

  test('cancel remove site group', function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstSiteGroup();
    cancelDialog();

    assertEquals(0, browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(1, testElement.$.allSitesList.items!.length);
    assertEquals(0, browserProxy.getCallCount('clearEtldPlus1DataAndCookies'));
  });

  test('cancel remove origin', function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    siteGroup.origins[0].numCookies = 1;
    siteGroup.origins[1].numCookies = 2;
    siteGroup.origins[2].numCookies = 3;
    siteGroup.numCookies = 6;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstOrigin();
    cancelDialog();

    assertEquals(
        0, browserProxy.getCallCount('clearUnpartitionedOriginDataAndCookies'));
    assertEquals(0, browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(6, testElement.$.allSitesList.items![0].numCookies);
  });

  test('permissions bullet point visbility', function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    siteGroup.origins[0].hasPermissionSettings = true;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstOrigin();
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    assertTrue(isChildVisible(testElement, '#permissionsBulletPoint'));
    cancelDialog();

    removeFirstSiteGroup();
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    assertTrue(isChildVisible(testElement, '#permissionsBulletPoint'));
    cancelDialog();

    siteGroup.origins[0].hasPermissionSettings = false;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstOrigin();
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    assertFalse(isChildVisible(testElement, '#permissionsBulletPoint'));
    cancelDialog();

    removeFirstSiteGroup();
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    assertFalse(isChildVisible(testElement, '#permissionsBulletPoint'));
    cancelDialog();
  });

  test('dynamic strings', async function() {
    // Single origin, no apps.
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstOrigin();
    assertEquals(
        getSubstitutedString(
            'siteSettingsRemoveSiteOriginDialogTitle', 'subdomain.example.com'),
        testElement.shadowRoot!.querySelector<HTMLElement>(
                                   '#removeSiteTitle')!.innerText);
    assertEquals(
        getString('siteSettingsRemoveSiteOriginLogout'),
        testElement.shadowRoot!
            .querySelector<HTMLElement>('#logoutBulletPoint')!.innerText);
    cancelDialog();

    // Site group, multiple origins, no apps.
    removeFirstSiteGroup();
    assertEquals(
        getSubstitutedString(
            'siteSettingsRemoveSiteGroupDialogTitle', 'example.com'),
        testElement.shadowRoot!.querySelector<HTMLElement>(
                                   '#removeSiteTitle')!.innerText);
    assertEquals(
        getString('siteSettingsRemoveSiteGroupLogout'),
        testElement.shadowRoot!
            .querySelector<HTMLElement>('#logoutBulletPoint')!.innerText);
    cancelDialog();

    // Single origin with app.
    siteGroup.origins[0].isInstalled = true;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstOrigin();
    assertEquals(
        getSubstitutedString(
            'siteSettingsRemoveSiteOriginAppDialogTitle',
            'subdomain.example.com'),
        testElement.shadowRoot!.querySelector<HTMLElement>(
                                   '#removeSiteTitle')!.innerText);
    assertEquals(
        getString('siteSettingsRemoveSiteOriginLogout'),
        testElement.shadowRoot!
            .querySelector<HTMLElement>('#logoutBulletPoint')!.innerText);
    cancelDialog();

    // Site group, multiple origins, with single app.
    removeFirstSiteGroup();
    assertEquals(
        getSubstitutedString(
            'siteSettingsRemoveSiteGroupAppDialogTitle', 'example.com'),
        testElement.shadowRoot!.querySelector<HTMLElement>(
                                   '#removeSiteTitle')!.innerText);
    assertEquals(
        getString('siteSettingsRemoveSiteGroupLogout'),
        testElement.shadowRoot!
            .querySelector<HTMLElement>('#logoutBulletPoint')!.innerText);
    cancelDialog();

    // Site group, multiple sites, multiple apps.
    siteGroup.origins[1].isInstalled = true;
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstSiteGroup();
    assertEquals(
        getSubstitutedString(
            'siteSettingsRemoveSiteGroupAppPluralDialogTitle', 'example.com'),
        testElement.shadowRoot!.querySelector<HTMLElement>(
                                   '#removeSiteTitle')!.innerText);
    assertEquals(
        getString('siteSettingsRemoveSiteGroupLogout'),
        testElement.shadowRoot!
            .querySelector<HTMLElement>('#logoutBulletPoint')!.innerText);
    cancelDialog();

    // Site group, single origin, no app.
    const singleOriginSiteGroup =
        JSON.parse(JSON.stringify(TEST_SINGLE_SITE_GROUP));
    testElement.siteGroupMap.set(
        singleOriginSiteGroup.etldPlus1,
        JSON.parse(JSON.stringify(singleOriginSiteGroup)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstSiteGroup();
    assertEquals(
        getSubstitutedString(
            'siteSettingsRemoveSiteOriginDialogTitle', 'single.example.com'),
        testElement.shadowRoot!.querySelector<HTMLElement>(
                                   '#removeSiteTitle')!.innerText);
    assertEquals(
        getString('siteSettingsRemoveSiteOriginLogout'),
        testElement.shadowRoot!
            .querySelector<HTMLElement>('#logoutBulletPoint')!.innerText);
    cancelDialog();

    // Site group, single origin, one app.
    singleOriginSiteGroup.origins[0].isInstalled = true;
    testElement.siteGroupMap.set(
        singleOriginSiteGroup.etldPlus1,
        JSON.parse(JSON.stringify(singleOriginSiteGroup)));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstSiteGroup();
    assertEquals(
        getSubstitutedString(
            'siteSettingsRemoveSiteOriginAppDialogTitle', 'single.example.com'),
        testElement.shadowRoot!.querySelector<HTMLElement>(
                                   '#removeSiteTitle')!.innerText);
    assertEquals(
        getString('siteSettingsRemoveSiteOriginLogout'),
        testElement.shadowRoot!
            .querySelector<HTMLElement>('#logoutBulletPoint')!.innerText);
  });
});

suite('AllSites_EnableFirstPartySets', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   */
  const TEST_MULTIPLE_SITE_GROUP = createSiteGroup('example.com', [
    'http://example.com',
    'https://www.example.com',
    'https://login.example.com',
  ]);

  /**
   * Example site groups with one owned SiteGroup.
   */
  const TEST_SITE_GROUPS: SiteGroup[] = [
    {
      etldPlus1: 'foo.com',
      origins: [createOriginInfo('https://foo.com')],
      numCookies: 0,
      fpsOwner: 'foo.com',
      hasInstalledPWA: false,
    },
    {
      etldPlus1: 'bar.com',
      origins: [createOriginInfo('https://bar.com')],
      numCookies: 0,
      hasInstalledPWA: false,
    },
    {
      etldPlus1: 'example.com',
      origins: [createOriginInfo('https://example.com')],
      numCookies: 0,
      hasInstalledPWA: false,
    },
  ];

  /**
   * Example first party set site groups.
   */
  const TEST_FPS_SITE_GROUPS: SiteGroup[] = [
    {
      etldPlus1: 'google.com',
      origins: [createOriginInfo('https://google.com')],
      numCookies: 0,
      fpsOwner: 'google.com',
      fpsNumMembers: 2,
      hasInstalledPWA: false,
    },
    {
      etldPlus1: 'youtube.com',
      origins: [createOriginInfo('https://youtube.com')],
      numCookies: 0,
      fpsOwner: 'google.com',
      fpsNumMembers: 2,
      hasInstalledPWA: false,
    },
  ];

  let testElement: AllSitesElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  /**
   * The mock local data proxy object to use during test.
   */
  let localDataBrowserProxy: TestLocalDataBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      consolidatedSiteStorageControlsEnabled: true,
      firstPartySetsUIEnabled: true,
    });
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });


  // Initialize a site-list before each test.
  setup(async function() {
    document.body.innerHTML = '';

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    localDataBrowserProxy = new TestLocalDataBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    LocalDataBrowserProxyImpl.setInstance(localDataBrowserProxy);
    testElement = document.createElement('all-sites');
    assertTrue(!!testElement);
    document.body.appendChild(testElement);
  });

  teardown(function() {
    // The code being tested changes the Route. Reset so that state is not
    // leaked across tests.
    Router.getInstance().resetRouteForTesting();
  });

  function removeSiteViaOverflowMenu(buttonType: string) {
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    flush();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertTrue(siteEntries.length >= 1);
    const overflowMenuButton =
        siteEntries[0]!.shadowRoot!.querySelector<HTMLElement>(
            '#fpsOverflowMenuButton')!;
    assertFalse(
        overflowMenuButton.closest<HTMLElement>('.row-aligned')!.hidden);

    // Test clicking on the overflow menu button opens the menu.
    const overflowMenu = testElement.$.menu.get();
    assertFalse(overflowMenu.open);
    overflowMenuButton.click();
    assertTrue(overflowMenu.open);
    flush();
    const menuItems =
        overflowMenu.querySelectorAll<HTMLElement>('.dropdown-item');
    assertFalse(testElement.$.confirmRemoveSite.get().open);
    menuItems[1]!.click();
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    const actionButtonList =
        testElement.$.confirmRemoveSite.get().querySelectorAll<HTMLElement>(
            `.${buttonType}`);
    assertEquals(1, actionButtonList.length);
    actionButtonList[0]!.click();
    // Check the dialog and overflow menu are now both closed.
    assertFalse(testElement.$.confirmRemoveSite.get().open);
    assertFalse(overflowMenu.open);
  }

  function removeFirstSiteGroup() {
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);
    siteEntries[0]!.shadowRoot!.querySelector<HTMLElement>(
                                   '#removeSiteButton')!.click();
  }

  function confirmDialog() {
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    testElement.$.confirmRemoveSite.get()
        .querySelector<HTMLElement>('.action-button')!.click();
  }

  function cancelDialog() {
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    testElement.$.confirmRemoveSite.get()
        .querySelector<HTMLElement>('.cancel-button')!.click();
  }

  test('remove site via overflow menu', async function() {
    const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
    siteGroup.fpsOwner = 'google.com';
    testElement.siteGroupMap.set(
        siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    testElement.forceListUpdateForTesting();
    assertEquals(testElement.$.allSitesList.items!.length, 1);
    removeSiteViaOverflowMenu('action-button');
    assertEquals(testElement.$.allSitesList.items!.length, 0);
  });

  test(
      'cancelling the confirm dialog on removing site works', async function() {
        const siteGroup = JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP));
        siteGroup.fpsOwner = 'google.com';
        testElement.siteGroupMap.set(
            siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
        testElement.forceListUpdateForTesting();
        removeSiteViaOverflowMenu('cancel-button');
      });

  test('click and remove site entry with remove button', async function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.etldPlus1,
        JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
    testElement.forceListUpdateForTesting();
    flush();
    removeFirstSiteGroup();
    confirmDialog();
  });

  test(
      'click and cancel dialog site entry with remove button',
      async function() {
        testElement.siteGroupMap.set(
            TEST_MULTIPLE_SITE_GROUP.etldPlus1,
            JSON.parse(JSON.stringify(TEST_MULTIPLE_SITE_GROUP)));
        testElement.forceListUpdateForTesting();
        flush();
        removeFirstSiteGroup();
        cancelDialog();
      });

  test('filter sites by first party set owner', async function() {
    TEST_SITE_GROUPS.forEach(siteGroup => {
      testElement.siteGroupMap.set(
          siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
    });
    testElement.forceListUpdateForTesting();
    flush();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(3, siteEntries.length);
    const overflowMenuButton =
        siteEntries[0]!.shadowRoot!.querySelector<HTMLElement>(
            '#fpsOverflowMenuButton')!;
    assertFalse(
        overflowMenuButton.closest<HTMLElement>('.row-aligned')!.hidden);

    // Test clicking on the overflow menu button opens the menu.
    const overflowMenu = testElement.$.menu.get();
    assertFalse(overflowMenu.open);
    overflowMenuButton.click();
    assertTrue(overflowMenu.open);
    flush();
    const menuItems =
        overflowMenu.querySelectorAll<HTMLElement>('.dropdown-item');
    assertEquals('', testElement.filter);
    // Click show related sites.
    menuItems[0]!.click();
    // Check the overflow menu is now closed.
    assertFalse(overflowMenu.open);
    // Verify filter is applied in search query.
    assertEquals(
        'related:foo.com',
        Router.getInstance().getQueryParameters().get('searchSubpage'));
    // Filter needs to be set manually here as rerouting to all-sites with a
    // search query doesn't change it in this test.
    testElement.filter = 'related:foo.com';
    flush();

    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    let hiddenSiteEntries = Array.from(
        testElement.shadowRoot!.querySelectorAll('site-entry[hidden]'));
    assertEquals(1, siteEntries.length - hiddenSiteEntries.length);
    assertEquals('foo.com', siteEntries[0]!.siteGroup.fpsOwner);

    // Clear filter and assert the list is back to 3 elements.
    testElement.filter = '';
    flush();

    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    hiddenSiteEntries = Array.from(
        testElement.shadowRoot!.querySelectorAll('site-entry[hidden]'));
    assertEquals(3, siteEntries.length - hiddenSiteEntries.length);
  });

  test(
      'site entry first party set information updated on site deletion',
      async function() {
        TEST_FPS_SITE_GROUPS.forEach(siteGroup => {
          testElement.siteGroupMap.set(
              siteGroup.etldPlus1, JSON.parse(JSON.stringify(siteGroup)));
        });
        testElement.forceListUpdateForTesting();
        flush();
        let siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(testElement.$.allSitesList.items!.length, 2);
        await localDataBrowserProxy.whenCalled('getFpsMembershipLabel');
        assertEquals(
            '· Allowed for 2 google.com sites',
            siteEntries[1]!.$.fpsMembership.innerText.trim());

        // Remove first site group.
        removeSiteViaOverflowMenu('action-button');
        siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(testElement.$.allSitesList.items!.length, 1);
        await localDataBrowserProxy.whenCalled('getFpsMembershipLabel');
        assertEquals(
            '· Allowed for 1 google.com site',
            siteEntries[1]!.$.fpsMembership.innerText.trim());
      });
});
