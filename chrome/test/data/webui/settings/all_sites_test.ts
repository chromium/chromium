// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {AllSitesElement, SiteGroup} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsPrefsBrowserProxyImpl, SortMethod} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, DeleteBrowsingDataAction, MetricsBrowserProxyImpl, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsPrefsBrowserProxy} from './test_site_settings_prefs_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createOriginInfo, createRawSiteException, createSiteGroup, createSiteSettingsPrefs, groupingKey} from './test_util.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';


// clang-format on

suite('DisableRelatedWebsiteSets', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   */
  const TEST_MULTIPLE_SITE_GROUP =
      createSiteGroup('example.com', 'example.com', [
        'http://subdomain.example.com/',
        'https://www.example.com/',
        'https://login.example.com/',
      ]);

  /**
   * An example eTLD+1 Object with a single origin grouped under it.
   */
  const TEST_SINGLE_SITE_GROUP = createSiteGroup('example.com', 'example.com', [
    'https://single.example.com/',
  ]);

  /**
   * An example pref with multiple categories and multiple allow/block
   * state.
   */
  let prefsVarious: SiteSettingsPref;

  let testElement: AllSitesElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;


  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      firstPartySetsUIEnabled: false,
    });
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  // Initialize a site-list before each test.
  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

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
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
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

    browserProxy.resetResolver('getAllSites');
    const searchParams = new URLSearchParams(
        'searchSubpage=' + encodeURIComponent(SEARCH_QUERY));
    const currentRoute = Router.getInstance().getCurrentRoute();
    Router.getInstance().navigateTo(currentRoute, searchParams);
    testElement.filter = SEARCH_QUERY;

    flush();
    // Changing filter shouldn't trigger additional getAllSites calls.
    assertEquals(0, browserProxy.getCallCount('getAllSites'));
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
    const fooSiteGroup = testElement.siteGroupMap.get(groupingKey('foo.com'))!;
    fooSiteGroup.origins.push(
        createOriginInfo('https://login.foo.com', {engagement: 20}));
    assertEquals(2, fooSiteGroup.origins.length);
    fooSiteGroup.origins[0]!.engagement = 50.4;
    const googleSiteGroup =
        testElement.siteGroupMap.get(groupingKey('google.com'))!;
    assertEquals(1, googleSiteGroup.origins.length);
    googleSiteGroup.origins[0]!.engagement = 55.1261;
    const barSiteGroup = testElement.siteGroupMap.get(groupingKey('bar.com'))!;
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
        loadTimeData.getString('siteSettingsDeleteAllStorageLabel'),
        clearAllButton.innerText.trim());

    // Setting a filter, text should change.
    testElement.filter = 'foo';
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsDeleteDisplayedStorageLabel'),
        clearAllButton.innerText.trim());

    // Removing the filter.
    testElement.filter = '';
    await flushTasks();
    assertEquals(
        loadTimeData.getString('siteSettingsDeleteAllStorageLabel'),
        clearAllButton.innerText.trim());
  });

  test('dynamic filtered total usage strings', async function() {
    setUpAllSites(prefsVarious);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    // Removing extra unwanted site entries.
    testElement.siteGroupMap.delete(groupingKey('google.com'));
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
        title: 'siteSettingsDeleteAllStorageDialogTitle',
        description: 'siteSettingsDeleteAllStorageConfirmation',
        signout: 'siteSettingsClearAllStorageSignOut',
      },
      {
        filter: false,
        appInstalled: true,
        storage: true,
        title: 'siteSettingsDeleteAllStorageDialogTitle',
        description: 'siteSettingsDeleteAllStorageConfirmationInstalled',
        signout: 'siteSettingsClearAllStorageSignOut',
      },
      {
        filter: true,
        appInstalled: false,
        storage: true,
        title: 'siteSettingsDeleteDisplayedStorageDialogTitle',
        description: 'siteSettingsDeleteDisplayedStorageConfirmation',
        signout: 'siteSettingsClearDisplayedStorageSignOut',
      },
      {
        filter: true,
        appInstalled: false,
        storage: false,
        title: 'siteSettingsDeleteDisplayedStorageDialogTitle',
        description: 'siteSettingsDeleteDisplayedStorageConfirmation',
        signout: 'siteSettingsClearDisplayedStorageSignOut',
      },
      {
        filter: true,
        appInstalled: true,
        storage: false,
        title: 'siteSettingsDeleteDisplayedStorageDialogTitle',
        description: 'siteSettingsDeleteDisplayedStorageConfirmationInstalled',
        signout: 'siteSettingsClearDisplayedStorageSignOut',
      },
      {
        filter: true,
        appInstalled: true,
        storage: true,
        title: 'siteSettingsDeleteDisplayedStorageDialogTitle',
        description: 'siteSettingsDeleteDisplayedStorageConfirmationInstalled',
        signout: 'siteSettingsClearDisplayedStorageSignOut',
      },
    ];

    setUpAllSites(prefsVarious);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    // Removing extra unwanted site entries.
    testElement.siteGroupMap.delete(groupingKey('google.com'));

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

      testElement.siteGroupMap.get(groupingKey('foo.com'))!.hasInstalledPWA =
          state.appInstalled;
      testElement.siteGroupMap.get(groupingKey('bar.com'))!.hasInstalledPWA =
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

  test('clear data "no sites" string', async function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.groupingKey,
        structuredClone(TEST_MULTIPLE_SITE_GROUP));
    const googleSiteGroup = createSiteGroup('google.com', 'google.com', [
      'https://www.google.com',
      'https://docs.google.com',
      'https://mail.google.com',
    ]);
    testElement.siteGroupMap.set(googleSiteGroup.groupingKey, googleSiteGroup);
    testElement.filter = 'google';
    testElement.forceListUpdateForTesting();
    await flushTasks();

    assertFalse(isChildVisible(testElement, '#noSitesFoundText'));

    clearDataViaClearAllButton('action-button');
    await flushTasks();

    assertTrue(isChildVisible(testElement, '#noSitesFoundText'));
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
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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
        groupingKey: groupingKey(fooEtldPlus1),
        displayName: fooEtldPlus1,
        origins: [
          createOriginInfo(fooOrigin),
          createOriginInfo('https://foo.com'),
        ],
        hasInstalledPWA: false,
        numCookies: 0,
      },
      {
        // Test adding a new site entry works.
        groupingKey: groupingKey(addEtldPlus1),
        displayName: addEtldPlus1,
        origins: [createOriginInfo(addOrigin)],
        hasInstalledPWA: false,
        numCookies: 0,
      },
    ];
    testElement.onStorageListFetched(STORAGE_SITE_GROUP_LIST);

    flush();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(4, siteEntries.length);

    assertEquals(fooEtldPlus1, siteEntries[0]!.siteGroup.displayName);
    assertEquals(2, siteEntries[0]!.siteGroup.origins.length);
    assertEquals(fooOrigin, siteEntries[0]!.siteGroup.origins[0]!.origin);
    assertEquals(
        'https://foo.com', siteEntries[0]!.siteGroup.origins[1]!.origin);

    assertEquals(addEtldPlus1, siteEntries[3]!.siteGroup.displayName);
    assertEquals(1, siteEntries[3]!.siteGroup.origins.length);
    assertEquals(addOrigin, siteEntries[3]!.siteGroup.origins[0]!.origin);
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
        TEST_MULTIPLE_SITE_GROUP.groupingKey,
        structuredClone(TEST_MULTIPLE_SITE_GROUP));
    testElement.forceListUpdateForTesting();
    clearDataViaClearAllButton('cancel-button');
  });

  test('clearing data via clear all dialog', async function() {
    // Test when all origins has no permission settings and no data.
    // Clone this object to avoid propagating changes made in this test.
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.groupingKey,
        structuredClone(TEST_MULTIPLE_SITE_GROUP));
    const googleSiteGroup = createSiteGroup('google.com', 'google.com', [
      'https://www.google.com',
      'https://docs.google.com',
      'https://mail.google.com',
    ]);
    testElement.siteGroupMap.set(googleSiteGroup.groupingKey, googleSiteGroup);
    testElement.forceListUpdateForTesting();
    clearDataViaClearAllButton('action-button');
    // Ensure a call was made to clearSiteGroupDataAndCookies.
    assertEquals(2, browserProxy.getCallCount('clearSiteGroupDataAndCookies'));
    assertEquals(testElement.$.allSitesList.items!.length, 0);

    assertEquals(
        DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
        await metricsBrowserProxy.whenCalled('recordDeleteBrowsingDataAction'));
  });

  test(
      'clear data via clear all button (one origin has permission)',
      async function() {
        // Test when there is one origin has permissions settings.
        // Clone this object to avoid propagating changes made in this test.
        const siteGroup = structuredClone(TEST_MULTIPLE_SITE_GROUP);
        siteGroup.origins[0]!.hasPermissionSettings = true;
        testElement.siteGroupMap.set(
            siteGroup.groupingKey, structuredClone(siteGroup));
        const googleSiteGroup = createSiteGroup('google.com', 'google.com', [
          'https://www.google.com',
          'https://docs.google.com',
          'https://mail.google.com',
        ]);
        testElement.siteGroupMap.set(
            googleSiteGroup.groupingKey, googleSiteGroup);
        testElement.forceListUpdateForTesting();
        assertEquals(testElement.$.allSitesList.items!.length, 2);
        assertEquals(
            testElement.$.allSitesList.items![0].origins.length,
            siteGroup.origins.length);
        clearDataViaClearAllButton('action-button');
        assertEquals(testElement.$.allSitesList.items!.length, 1);
        assertEquals(testElement.$.allSitesList.items![0].origins.length, 1);

        assertEquals(
            DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
            await metricsBrowserProxy.whenCalled(
                'recordDeleteBrowsingDataAction'));
      });

  test('clear all button is hidden when the list is empty', async function() {
    // Start with an empty list should hide clear all button.
    assertEquals(testElement.$.allSitesList.items!.length, 0);
    const clearAllButton = testElement.$.clearAllButton;
    assertFalse(isVisible(clearAllButton));

    // Add an entry to site group map.
    const siteGroup = structuredClone(TEST_MULTIPLE_SITE_GROUP);
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await flushTasks();

    // Ensure list is populated and clear all button is shown.
    assertEquals(testElement.$.allSitesList.items!.length, 1);
    assertTrue(isVisible(clearAllButton));

    // Clearing all data should re-hide the button.
    clearDataViaClearAllButton('action-button');
    assertEquals(testElement.$.allSitesList.items!.length, 0);
    assertFalse(isVisible(clearAllButton));
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

  test('remove site group', function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.groupingKey,
        structuredClone(TEST_MULTIPLE_SITE_GROUP));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstSiteGroup();
    confirmDialog();

    assertEquals(
        TEST_MULTIPLE_SITE_GROUP.origins.length,
        browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(0, testElement.$.allSitesList.items!.length);
    assertEquals(1, browserProxy.getCallCount('clearSiteGroupDataAndCookies'));
  });

  test('remove origin', async function() {
    const siteGroup = structuredClone(TEST_MULTIPLE_SITE_GROUP);
    siteGroup.origins[0]!.numCookies = 1;
    siteGroup.origins[1]!.numCookies = 2;
    siteGroup.origins[2]!.numCookies = 3;
    siteGroup.numCookies = 6;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstOrigin();
    confirmDialog();

    assertEquals(
        siteGroup.origins[0]!.origin,
        await browserProxy.whenCalled(
            'clearUnpartitionedOriginDataAndCookies'));

    const [origin, types, setting] =
        await browserProxy.whenCalled('setOriginPermissions');
    assertEquals(origin, siteGroup.origins[0]!.origin);
    assertEquals(types, null);  // Null affects all content types.
    assertEquals(setting, ContentSetting.DEFAULT);

    assertEquals(
        1, browserProxy.getCallCount('clearUnpartitionedOriginDataAndCookies'));
    assertEquals(1, browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(5, testElement.$.allSitesList.items![0].numCookies);
  });

  test('remove partitioned origin', async function() {
    const siteGroup = structuredClone(TEST_MULTIPLE_SITE_GROUP);
    siteGroup.origins[0]!.isPartitioned = true;
    siteGroup.origins[0]!.numCookies = 1;
    siteGroup.origins[1]!.numCookies = 2;
    siteGroup.origins[2]!.numCookies = 3;
    siteGroup.numCookies = 6;

    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
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

    const [origin, groupingKey] =
        await browserProxy.whenCalled('clearPartitionedOriginDataAndCookies');

    assertEquals(siteGroup.origins[0]!.origin, origin);
    assertEquals(siteGroup.groupingKey, groupingKey);
    assertEquals(
        1, browserProxy.getCallCount('clearPartitionedOriginDataAndCookies'));
    assertEquals(0, browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(5, testElement.$.allSitesList.items![0].numCookies);
  });

  test('cancel remove site group', function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.groupingKey,
        structuredClone(TEST_MULTIPLE_SITE_GROUP));
    testElement.forceListUpdateForTesting();
    flush();

    removeFirstSiteGroup();
    cancelDialog();

    assertEquals(0, browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(1, testElement.$.allSitesList.items!.length);
    assertEquals(0, browserProxy.getCallCount('clearSiteGroupDataAndCookies'));
  });

  test('cancel remove origin', function() {
    const siteGroup = structuredClone(TEST_MULTIPLE_SITE_GROUP);
    siteGroup.origins[0]!.numCookies = 1;
    siteGroup.origins[1]!.numCookies = 2;
    siteGroup.origins[2]!.numCookies = 3;
    siteGroup.numCookies = 6;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
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
    const siteGroup = structuredClone(TEST_MULTIPLE_SITE_GROUP);
    siteGroup.origins[0]!.hasPermissionSettings = true;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
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

    siteGroup.origins[0]!.hasPermissionSettings = false;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
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
    const siteGroup = structuredClone(TEST_MULTIPLE_SITE_GROUP);
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
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
    siteGroup.origins[0]!.isInstalled = true;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
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
    siteGroup.origins[1]!.isInstalled = true;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
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
    const singleOriginSiteGroup = structuredClone(TEST_SINGLE_SITE_GROUP);
    testElement.siteGroupMap.set(
        singleOriginSiteGroup.groupingKey,
        structuredClone(singleOriginSiteGroup));
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
    singleOriginSiteGroup.origins[0]!.isInstalled = true;
    testElement.siteGroupMap.set(
        singleOriginSiteGroup.groupingKey,
        structuredClone(singleOriginSiteGroup));
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

suite('EnableRelatedWebsiteSets', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   */
  const TEST_MULTIPLE_SITE_GROUP =
      createSiteGroup('example.com', 'example.com', [
        'http://example.com',
        'https://www.example.com',
        'https://login.example.com',
      ]);

  /**
   * Example site groups with one owned SiteGroup.
   */
  const TEST_SITE_GROUPS: SiteGroup[] = [
    {
      groupingKey: groupingKey('foo.com'),
      etldPlus1: 'foo.com',
      displayName: 'foo.com',
      origins: [createOriginInfo('https://foo.com')],
      numCookies: 0,
      rwsOwner: 'foo.com',
      hasInstalledPWA: false,
    },
    {
      groupingKey: groupingKey('bar.com'),
      etldPlus1: 'bar.com',
      displayName: 'bar.com',
      origins: [createOriginInfo('https://bar.com')],
      numCookies: 0,
      hasInstalledPWA: false,
    },
    {
      groupingKey: groupingKey('example.com'),
      etldPlus1: 'example.com',
      displayName: 'example.com',
      origins: [createOriginInfo('https://example.com')],
      numCookies: 0,
      hasInstalledPWA: false,
    },
  ];

  /**
   * Example related website set site groups.
   */
  const TEST_RWS_SITE_GROUPS: SiteGroup[] = [
    {
      groupingKey: groupingKey('google.com'),
      etldPlus1: 'google.com',
      displayName: 'google.com',
      origins: [
        createOriginInfo('https://google.com'),
        createOriginInfo('https://translate.google.com'),
      ],
      numCookies: 4,
      rwsOwner: 'google.com',
      rwsNumMembers: 2,
      hasInstalledPWA: false,
    },
    {
      groupingKey: groupingKey('youtube.com'),
      etldPlus1: 'youtube.com',
      displayName: 'youtube.com',
      origins: [createOriginInfo('https://youtube.com')],
      numCookies: 0,
      rwsOwner: 'google.com',
      rwsNumMembers: 2,
      hasInstalledPWA: false,
    },
  ];

  let testElement: AllSitesElement;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsPrefsBrowserProxy;

  let metricsBrowserProxy: TestMetricsBrowserProxy;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      firstPartySetsUIEnabled: true,
    });
  });

  suiteTeardown(function() {
    CrSettingsPrefs.resetForTesting();
  });


  // Initialize a site-list before each test.
  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    SiteSettingsPrefsBrowserProxyImpl.setInstance(browserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
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
            '#rwsOverflowMenuButton')!;
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
    const siteGroup = structuredClone(TEST_MULTIPLE_SITE_GROUP);
    siteGroup.rwsOwner = 'google.com';
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    assertEquals(testElement.$.allSitesList.items!.length, 1);
    removeSiteViaOverflowMenu('action-button');
    assertEquals(testElement.$.allSitesList.items!.length, 0);

    assertEquals(
        DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
        await metricsBrowserProxy.whenCalled('recordDeleteBrowsingDataAction'));
  });

  test(
      'cancelling the confirm dialog on removing site works', async function() {
        const siteGroup = structuredClone(TEST_MULTIPLE_SITE_GROUP);
        siteGroup.rwsOwner = 'google.com';
        testElement.siteGroupMap.set(
            siteGroup.groupingKey, structuredClone(siteGroup));
        testElement.forceListUpdateForTesting();
        removeSiteViaOverflowMenu('cancel-button');
      });

  test('click and remove site entry with remove button', async function() {
    testElement.siteGroupMap.set(
        TEST_MULTIPLE_SITE_GROUP.groupingKey,
        structuredClone(TEST_MULTIPLE_SITE_GROUP));
    testElement.forceListUpdateForTesting();
    flush();
    removeFirstSiteGroup();
    confirmDialog();

    assertEquals(
        DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
        await metricsBrowserProxy.whenCalled('recordDeleteBrowsingDataAction'));
  });

  test(
      'click and cancel dialog site entry with remove button',
      async function() {
        testElement.siteGroupMap.set(
            TEST_MULTIPLE_SITE_GROUP.groupingKey,
            structuredClone(TEST_MULTIPLE_SITE_GROUP));
        testElement.forceListUpdateForTesting();
        flush();
        removeFirstSiteGroup();
        cancelDialog();
      });

  test('filter sites by related website set owner', async function() {
    TEST_SITE_GROUPS.forEach(siteGroup => {
      testElement.siteGroupMap.set(
          siteGroup.groupingKey, structuredClone(siteGroup));
    });
    testElement.forceListUpdateForTesting();
    flush();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(3, siteEntries.length);
    const overflowMenuButton =
        siteEntries[0]!.shadowRoot!.querySelector<HTMLElement>(
            '#rwsOverflowMenuButton')!;
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
    assertEquals('foo.com', siteEntries[0]!.siteGroup.rwsOwner);

    // Clear filter and assert the list is back to 3 elements.
    testElement.filter = '';
    flush();

    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    hiddenSiteEntries = Array.from(
        testElement.shadowRoot!.querySelectorAll('site-entry[hidden]'));
    assertEquals(3, siteEntries.length - hiddenSiteEntries.length);
  });

  test(
      'site entry related website set information updated on site deletion',
      async function() {
        TEST_RWS_SITE_GROUPS.forEach(siteGroup => {
          testElement.siteGroupMap.set(
              siteGroup.groupingKey, structuredClone(siteGroup));
        });
        testElement.forceListUpdateForTesting();
        flush();
        let siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(testElement.$.allSitesList.items!.length, 2);
        await browserProxy.whenCalled('getRwsMembershipLabel');
        assertEquals(
            '· 2 sites in google.com\'s group',
            siteEntries[1]!.$.rwsMembership.innerText.trim());

        // Remove first site group.
        removeSiteViaOverflowMenu('action-button');
        siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(testElement.$.allSitesList.items!.length, 1);
        await browserProxy.whenCalled('getRwsMembershipLabel');
        assertEquals(
            '· 1 site in google.com\'s group',
            siteEntries[1]!.$.rwsMembership.innerText.trim());
      });

  test(
      'site entry related website set constant member count on origin deletion',
      async function() {
        TEST_RWS_SITE_GROUPS.forEach(siteGroup => {
          testElement.siteGroupMap.set(
              siteGroup.groupingKey, structuredClone(siteGroup));
        });
        testElement.forceListUpdateForTesting();
        flush();

        let siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(testElement.$.allSitesList.items!.length, 2);
        await browserProxy.whenCalled('getRwsMembershipLabel');
        assertEquals(
            '· 2 sites in google.com\'s group',
            siteEntries[1]!.$.rwsMembership.innerText.trim());

        let originList = siteEntries[0]!.$.originList.get();
        flush();
        // Ensure there are 2 origin entries.
        let originEntries = originList.querySelectorAll('.hr');
        assertEquals(2, originEntries.length);

        // Remove the first origin.
        originEntries[0]!.querySelector<HTMLElement>(
                             '#removeOriginButton')!.click();
        assertTrue(testElement.$.confirmRemoveSite.get().open);
        testElement.$.confirmRemoveSite.get()
            .querySelector<HTMLElement>('.action-button')!.click();

        // Validate that only 1 origin entry remaining.
        siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        originList = siteEntries[0]!.$.originList.get();
        flush();
        originEntries = originList.querySelectorAll('.hr');
        assertEquals(1, originEntries.length);

        // Ensure that related website set info is unaffected by origin removal.
        await browserProxy.whenCalled('getRwsMembershipLabel');
        assertEquals(
            '· 2 sites in google.com\'s group',
            siteEntries[1]!.$.rwsMembership.innerText.trim());

        // Remove the last origin.
        siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        originList = siteEntries[0]!.$.originList.get();
        flush();
        originEntries[0]!.querySelector<HTMLElement>(
                             '#removeOriginButton')!.click();
        assertTrue(testElement.$.confirmRemoveSite.get().open);
        testElement.$.confirmRemoveSite.get()
            .querySelector<HTMLElement>('.action-button')!.click();

        // Ensure that the site entry remains in the list as there are cookies
        // set at the eTLD+1 level so it converts to an ungrouped site entry and
        // related website set information remain unchanged.
        assertEquals(testElement.$.allSitesList.items!.length, 2);
        await browserProxy.whenCalled('getRwsMembershipLabel');
        assertEquals(
            '· 2 sites in google.com\'s group',
            siteEntries[1]!.$.rwsMembership.innerText.trim());
      });

  test(
      'show learn more about related website sets link when filtering by rws owner',
      function() {
        TEST_SITE_GROUPS.forEach(siteGroup => {
          testElement.siteGroupMap.set(
              siteGroup.groupingKey, structuredClone(siteGroup));
        });
        testElement.forceListUpdateForTesting();
        flush();
        let relatedWebsiteSetsLearnMore =
            testElement.shadowRoot!.querySelector<HTMLElement>('#relatedWebsiteSetsLearnMore');
        // When no filter is applied (as the test starts) the learn more link
        // should be hidden.
        assertTrue(relatedWebsiteSetsLearnMore!.hidden);

        testElement.filter = 'related:foo.com';
        flush();

        relatedWebsiteSetsLearnMore =
            testElement.shadowRoot!.querySelector<HTMLElement>('#relatedWebsiteSetsLearnMore');
        assertFalse(relatedWebsiteSetsLearnMore!.hidden);
        assertEquals(
            [
              loadTimeData.getStringF(
                  'siteSettingsRelatedWebsiteSetsLearnMore', 'foo.com'),
              loadTimeData.getString('learnMore'),
            ].join(' '),
            relatedWebsiteSetsLearnMore!.innerText.trim());

        testElement.filter = 'related:bar.com';
        flush();

        relatedWebsiteSetsLearnMore =
            testElement.shadowRoot!.querySelector<HTMLElement>('#relatedWebsiteSetsLearnMore');
        assertTrue(relatedWebsiteSetsLearnMore!.hidden);
      });
});
