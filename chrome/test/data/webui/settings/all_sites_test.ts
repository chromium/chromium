// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrInfiniteListElement} from 'chrome://resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import type {AllSitesElement, SiteEntryElement, SiteGroup} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsBrowserProxyImpl, SortMethod} from 'chrome://settings/lazy_load.js';
import {DeleteBrowsingDataAction, loadTimeData, MetricsBrowserProxyImpl, resetGlobalScrollTargetForTesting, Router, routes, setGlobalScrollTarget} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createOriginInfo, createRawSiteException, createSiteGroup, createSiteSettingsPrefs, groupingKey} from './test_util.js';

/**
 * An example pref with multiple categories and multiple allow/block
 * state.
 */
function createPrefsVarious(): SiteSettingsPref {
  return createSiteSettingsPrefs([], [
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
}

suite('AllSites', function() {
  /**
   * An example eTLD+1 Object with multiple origins grouped under it.
   */
  function createMultipleSiteGroup(): SiteGroup {
    return createSiteGroup('example.com', 'example.com', [
      'http://subdomain.example.com/',
      'https://www.example.com/',
      'https://login.example.com/',
    ]);
  }

  /**
   * An example eTLD+1 Object with a single origin grouped under it.
   */
  function createSingleSiteGroup(): SiteGroup {
    return createSiteGroup('example.com', 'example.com', [
      'https://single.example.com/',
    ]);
  }

  let testElement: AllSitesElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  /**
   * The mock proxy object to use during test.
   */
  let browserProxy: TestSiteSettingsBrowserProxy;

  // Initialize a site-list before each test.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
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
    resetGlobalScrollTargetForTesting();
  });

  /**
   * Configures the test element.
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

  function getDisplayName(siteEntry: SiteEntryElement): string {
    const urlDirectionality =
        siteEntry.$.displayName.querySelector<HTMLElement>(
            '.url-directionality');
    assertTrue(!!urlDirectionality);
    return urlDirectionality.textContent.trim();
  }

  function getAllSitesList(): CrInfiniteListElement<SiteGroup> {
    const list =
        testElement.shadowRoot.querySelector<CrInfiniteListElement<SiteGroup>>(
            'cr-infinite-list');
    assertTrue(!!list);
    return list;
  }

  test('All sites list populated', async function() {
    setUpAllSites(createPrefsVarious());
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    await microtasksFinished();
    assertEquals(3, testElement.siteGroupMap.size);

    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(3, siteEntries.length);
    assertEquals(SortMethod.MOST_VISITED, testElement.$.sortMethod.value);
  });

  test('search query filters list', async function() {
    const SEARCH_QUERY = 'foo';
    setUpAllSites(createPrefsVarious());
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    await microtasksFinished();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(3, siteEntries.length);

    browserProxy.resetResolver('getAllSites');
    const searchParams = new URLSearchParams(
        'searchSubpage=' + encodeURIComponent(SEARCH_QUERY));
    const currentRoute = Router.getInstance().getCurrentRoute();
    Router.getInstance().navigateTo(currentRoute, searchParams);
    testElement.filter = SEARCH_QUERY;

    await microtasksFinished();
    // Changing filter shouldn't trigger additional getAllSites calls.
    assertEquals(0, browserProxy.getCallCount('getAllSites'));
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);

    for (const entry of siteEntries) {
      assertTrue(!!entry.siteGroup);
      assertTrue(entry.siteGroup.origins.some(origin => {
        return origin.origin.includes(SEARCH_QUERY);
      }));
    }
  });

  test('can be sorted by most visited', async function() {
    setUpAllSites(createPrefsVarious());
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);

    await browserProxy.whenCalled('getAllSites');

    // Add additional origins and artificially insert fake engagement scores
    // to sort.
    assertEquals(3, testElement.siteGroupMap.size);
    const fooSiteGroup = testElement.siteGroupMap.get(groupingKey('foo.com'));
    assertTrue(!!fooSiteGroup);
    fooSiteGroup.origins.push(
        createOriginInfo('https://login.foo.com', {engagement: 20}));
    assertEquals(2, fooSiteGroup.origins.length);
    fooSiteGroup.origins[0]!.engagement = 50.4;
    const googleSiteGroup =
        testElement.siteGroupMap.get(groupingKey('google.com'));
    assertTrue(!!googleSiteGroup);
    assertEquals(1, googleSiteGroup.origins.length);
    googleSiteGroup.origins[0]!.engagement = 55.1261;
    const barSiteGroup = testElement.siteGroupMap.get(groupingKey('bar.com'));
    assertTrue(!!barSiteGroup);
    assertEquals(1, barSiteGroup.origins.length);
    barSiteGroup.origins[0]!.engagement = 0.5235;

    // 'Most visited' is the default sort method, so sort by a different
    // method first to ensure changing to 'Most visited' works.
    testElement.$.sortMethod.value = 'name';
    testElement.$.sortMethod.dispatchEvent(new CustomEvent('change'));
    await microtasksFinished();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals('bar.com', getDisplayName(siteEntries[0]!));
    assertEquals('foo.com', getDisplayName(siteEntries[1]!));
    assertEquals('google.com', getDisplayName(siteEntries[2]!));

    testElement.$.sortMethod.value = 'most-visited';
    testElement.$.sortMethod.dispatchEvent(new CustomEvent('change'));
    await microtasksFinished();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    // Each site entry is sorted by its maximum engagement, so expect
    // 'foo.com' to come after 'google.com'.
    assertEquals('google.com', getDisplayName(siteEntries[0]!));
    assertEquals('foo.com', getDisplayName(siteEntries[1]!));
    assertEquals('bar.com', getDisplayName(siteEntries[2]!));
  });

  test('dynamic filtered clear data button strings', async function() {
    setUpAllSites(createPrefsVarious());
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');

    const clearAllButton = testElement.$.clearAllButton;
    assertEquals(
        loadTimeData.getString('siteSettingsDeleteAllStorageLabel'),
        clearAllButton.textContent.trim());

    // Setting a filter, text should change.
    testElement.filter = 'foo';
    await microtasksFinished();
    assertEquals(
        loadTimeData.getString('siteSettingsDeleteDisplayedStorageLabel'),
        clearAllButton.textContent.trim());

    // Removing the filter.
    testElement.filter = '';
    await microtasksFinished();
    assertEquals(
        loadTimeData.getString('siteSettingsDeleteAllStorageLabel'),
        clearAllButton.textContent.trim());
  });

  test('dynamic filtered total usage strings', async function() {
    setUpAllSites(createPrefsVarious());
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    // Removing extra unwanted site entries.
    testElement.siteGroupMap.delete(groupingKey('google.com'));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    const clearLabel = testElement.$.clearLabel;
    assertEquals(
        getSubstitutedString('siteSettingsClearAllStorageDescription', '100 B'),
        clearLabel.textContent.trim());

    // Setting a filter, 'foo.com' exists but doesn't have storage.
    testElement.filter = 'foo';
    await microtasksFinished();
    assertEquals(
        getSubstitutedString(
            'siteSettingsClearDisplayedStorageDescription', '0 B'),
        clearLabel.textContent.trim());

    // Changing the filter, 100 B given to 'bar.com; in the test proxy.
    testElement.filter = 'bar';
    await microtasksFinished();
    assertEquals(
        getSubstitutedString(
            'siteSettingsClearDisplayedStorageDescription', '100 B'),
        clearLabel.textContent.trim());

    // Remove the filter, without changing the total amount of storage.
    testElement.filter = '';
    await microtasksFinished();
    assertEquals(
        getSubstitutedString('siteSettingsClearAllStorageDescription', '100 B'),
        clearLabel.textContent.trim());
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

    setUpAllSites(createPrefsVarious());
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    // Removing extra unwanted site entries.
    testElement.siteGroupMap.delete(groupingKey('google.com'));

    const clearAllButton =
        testElement.$.clearAllButton.querySelector('cr-button');
    assertTrue(!!clearAllButton);
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

      const fooGroup = testElement.siteGroupMap.get(groupingKey('foo.com'));
      assertTrue(!!fooGroup);
      fooGroup.hasInstalledPWA = state.appInstalled;
      const barGroup = testElement.siteGroupMap.get(groupingKey('bar.com'));
      assertTrue(!!barGroup);
      barGroup.hasInstalledPWA = state.appInstalled;

      testElement.forceListUpdateForTesting();
      await microtasksFinished();

      const titleEl =
          confirmClearAllData.querySelector<HTMLElement>('[slot=title]');
      assertTrue(!!titleEl);
      const descriptionEl = confirmClearAllData.querySelector<HTMLElement>(
          '#clearAllStorageDialogDescription');
      assertTrue(!!descriptionEl);
      const signOutEl = confirmClearAllData.querySelector<HTMLElement>(
          '#clearAllStorageDialogSignOutLabel');
      assertTrue(!!signOutEl);

      assertEquals(
          loadTimeData.getString(state.title), titleEl.textContent.trim());
      assertEquals(
          getSubstitutedString(
              state.description, state.storage ? '100 B' : '0 B'),
          descriptionEl.textContent.trim());
      assertEquals(
          loadTimeData.getString(state.signout), signOutEl.textContent.trim());
    }
  });

  test('clear data "no sites" string', async function() {
    const multipleSiteGroup = createMultipleSiteGroup();
    testElement.siteGroupMap.set(
        multipleSiteGroup.groupingKey, multipleSiteGroup);
    const googleSiteGroup = createSiteGroup('google.com', 'google.com', [
      'https://www.google.com',
      'https://docs.google.com',
      'https://mail.google.com',
    ]);
    testElement.siteGroupMap.set(googleSiteGroup.groupingKey, googleSiteGroup);
    testElement.filter = 'google';
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    assertFalse(isChildVisible(testElement, '#noSitesFoundText'));

    await clearDataViaClearAllButton('action-button');

    assertTrue(isChildVisible(testElement, '#noSitesFoundText'));
  });

  test('can be sorted by storage', async function() {
    setUpAllSites(createPrefsVarious());
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    await microtasksFinished();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(3, siteEntries.length);
    // Add additional origins to SiteGroups with cookies to simulate their
    // being grouped entries, plus add local storage.
    siteEntries[0]!.siteGroup!.origins[0]!.usage = 900;
    siteEntries[1]!.siteGroup!.origins.push(createOriginInfo('http://bar.com'));
    siteEntries[1]!.siteGroup!.origins[0]!.usage = 500;
    siteEntries[1]!.siteGroup!.origins[1]!.usage = 500;
    siteEntries[2]!.siteGroup!.origins.push(
        createOriginInfo('http://google.com'));

    testElement.$.sortMethod.dispatchEvent(new CustomEvent('change'));
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    // Verify all sites is not sorted by storage.
    assertEquals(3, siteEntries.length);
    assertEquals('foo.com', getDisplayName(siteEntries[0]!));
    assertEquals('bar.com', getDisplayName(siteEntries[1]!));
    assertEquals('google.com', getDisplayName(siteEntries[2]!));

    // Change the sort method, then verify all sites is now sorted by
    // name.
    testElement.$.sortMethod.value = 'data-stored';
    testElement.$.sortMethod.dispatchEvent(new CustomEvent('change'));

    await microtasksFinished();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals('bar.com', getDisplayName(siteEntries[0]!));
    assertEquals('foo.com', getDisplayName(siteEntries[1]!));
    assertEquals('google.com', getDisplayName(siteEntries[2]!));
  });

  test('can be sorted by storage by passing URL param', async function() {
    // The default sorting (most visited) will have the ascending storage
    // values. With the URL param, we expect the sites to be sorted by usage in
    // descending order.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setUpAllSites(createPrefsVarious(), SortMethod.STORAGE);
    testElement = document.createElement('all-sites');
    document.body.appendChild(testElement);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    await microtasksFinished();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(3, siteEntries.length);
    assertEquals(SortMethod.STORAGE, testElement.$.sortMethod.value);

    assertEquals('google.com', getDisplayName(siteEntries[0]!));
    assertEquals('bar.com', getDisplayName(siteEntries[1]!));
    assertEquals('foo.com', getDisplayName(siteEntries[2]!));
  });

  test('can be sorted by name', async function() {
    setUpAllSites(createPrefsVarious());
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    await microtasksFinished();
    let siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');

    // Verify all sites is not sorted by name.
    assertEquals(3, siteEntries.length);
    assertEquals('foo.com', getDisplayName(siteEntries[0]!));
    assertEquals('bar.com', getDisplayName(siteEntries[1]!));
    assertEquals('google.com', getDisplayName(siteEntries[2]!));

    // Change the sort method, then verify all sites is now sorted by name.
    testElement.$.sortMethod.value = 'name';
    testElement.$.sortMethod.dispatchEvent(new CustomEvent('change'));
    await microtasksFinished();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals('bar.com', getDisplayName(siteEntries[0]!));
    assertEquals('foo.com', getDisplayName(siteEntries[1]!));
    assertEquals('google.com', getDisplayName(siteEntries[2]!));
  });

  test('can sort by name by passing URL param', async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setUpAllSites(createPrefsVarious(), SortMethod.NAME);
    testElement = document.createElement('all-sites');
    document.body.appendChild(testElement);
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    await microtasksFinished();
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');

    assertEquals('bar.com', getDisplayName(siteEntries[0]!));
    assertEquals('foo.com', getDisplayName(siteEntries[1]!));
    assertEquals('google.com', getDisplayName(siteEntries[2]!));
  });

  test('merging additional SiteGroup lists works', async function() {
    setUpAllSites(createPrefsVarious());
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    await microtasksFinished();
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

    await microtasksFinished();
    siteEntries = testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(4, siteEntries.length);

    assertEquals(fooEtldPlus1, siteEntries[0]!.siteGroup!.displayName);
    assertEquals(2, siteEntries[0]!.siteGroup!.origins.length);
    assertEquals(fooOrigin, siteEntries[0]!.siteGroup!.origins[0]!.origin);
    assertEquals(
        'https://foo.com', siteEntries[0]!.siteGroup!.origins[1]!.origin);

    assertEquals(addEtldPlus1, siteEntries[3]!.siteGroup!.displayName);
    assertEquals(1, siteEntries[3]!.siteGroup!.origins.length);
    assertEquals(addOrigin, siteEntries[3]!.siteGroup!.origins[0]!.origin);
  });

  async function clearDataViaClearAllButton(buttonType: string): Promise<void> {
    await microtasksFinished();
    assertTrue(
        buttonType === 'cancel-button' || buttonType === 'action-button');
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertTrue(siteEntries.length >= 1);
    const clearAllButton =
        testElement.$.clearAllButton.querySelector('cr-button');
    assertTrue(!!clearAllButton);
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
    await microtasksFinished();

    // Check the dialog and overflow menu are now both closed.
    assertFalse(confirmClearAllData.open);
  }

  test(
      'cancelling the confirm dialog on clear all data works',
      async function() {
        const siteGroup = createMultipleSiteGroup();
        testElement.siteGroupMap.set(siteGroup.groupingKey, siteGroup);
        testElement.forceListUpdateForTesting();
        await clearDataViaClearAllButton('cancel-button');
      });

  test('clearing data via clear all dialog', async function() {
    // Test when all origins has no permission settings and no data.
    const siteGroup = createMultipleSiteGroup();
    testElement.siteGroupMap.set(siteGroup.groupingKey, siteGroup);
    const googleSiteGroup = createSiteGroup('google.com', 'google.com', [
      'https://www.google.com',
      'https://docs.google.com',
      'https://mail.google.com',
    ]);
    testElement.siteGroupMap.set(googleSiteGroup.groupingKey, googleSiteGroup);
    testElement.forceListUpdateForTesting();
    await clearDataViaClearAllButton('action-button');
    // Ensure a call was made to clearSiteGroupDataAndCookies.
    assertEquals(2, browserProxy.getCallCount('clearSiteGroupDataAndCookies'));
    assertEquals(0, getAllSitesList().items.length);

    assertEquals(
        DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
        await metricsBrowserProxy.whenCalled('recordDeleteBrowsingDataAction'));
  });

  test(
      'clear data via clear all button (one origin has permission)',
      async function() {
        // Test when there is one origin has permissions settings.
        const siteGroup = createMultipleSiteGroup();
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
        await microtasksFinished();
        assertEquals(2, getAllSitesList().items.length);
        assertEquals(
            siteGroup.origins.length,
            getAllSitesList().items[0]!.origins.length);
        await clearDataViaClearAllButton('action-button');
        assertEquals(1, getAllSitesList().items.length);
        assertEquals(1, getAllSitesList().items[0]!.origins.length);

        assertEquals(
            DeleteBrowsingDataAction.SITES_SETTINGS_PAGE,
            await metricsBrowserProxy.whenCalled(
                'recordDeleteBrowsingDataAction'));
      });

  test('clear all button is hidden when the list is empty', async function() {
    // Start with an empty list should hide clear all button.
    assertEquals(0, getAllSitesList().items.length);
    const clearAllButton = testElement.$.clearAllButton;
    assertFalse(isVisible(clearAllButton));

    // Add an entry to site group map.
    const siteGroup = createMultipleSiteGroup();
    testElement.siteGroupMap.set(siteGroup.groupingKey, siteGroup);
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    // Ensure list is populated and clear all button is shown.
    assertEquals(1, getAllSitesList().items.length);
    assertTrue(isVisible(clearAllButton));

    // Clearing all data should re-hide the button.
    await clearDataViaClearAllButton('action-button');
    assertEquals(0, getAllSitesList().items.length);
    assertFalse(isVisible(clearAllButton));
  });

  async function removeFirstOrigin(): Promise<void> {
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);
    const originList = siteEntries[0]!.$.originList.get();
    const originEntries = originList.querySelectorAll('.hr');
    assertEquals(3, originEntries.length);
    const removeOriginButton =
        originEntries[0]!.querySelector<HTMLElement>('#removeOriginButton');
    assertTrue(!!removeOriginButton);
    removeOriginButton.click();
    await microtasksFinished();
  }

  async function removeFirstSiteGroup(): Promise<void> {
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);
    const removeSiteButton =
        siteEntries[0]!.shadowRoot.querySelector<HTMLElement>(
            '#removeSiteButton');
    assertTrue(!!removeSiteButton);
    removeSiteButton.click();
    await microtasksFinished();
  }

  async function confirmDialog(): Promise<void> {
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    const actionButton =
        testElement.$.confirmRemoveSite.get().querySelector<HTMLElement>(
            '.action-button');
    assertTrue(!!actionButton);
    actionButton.click();
    await microtasksFinished();
  }

  async function cancelDialog(): Promise<void> {
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    const cancelButton =
        testElement.$.confirmRemoveSite.get().querySelector<HTMLElement>(
            '.cancel-button');
    assertTrue(!!cancelButton);
    cancelButton.click();
    await microtasksFinished();
  }

  function getString(messageId: string): string {
    return testElement.i18n(messageId);
  }

  test('remove site group', async function() {
    const siteGroup = createMultipleSiteGroup();
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstSiteGroup();
    await confirmDialog();

    assertEquals(
        siteGroup.origins.length,
        browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(0, getAllSitesList().items.length);
    assertEquals(1, browserProxy.getCallCount('clearSiteGroupDataAndCookies'));
  });

  test('remove origin', async function() {
    const siteGroup = createMultipleSiteGroup();
    siteGroup.origins[0]!.numCookies = 1;
    siteGroup.origins[1]!.numCookies = 2;
    siteGroup.origins[2]!.numCookies = 3;
    siteGroup.numCookies = 6;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstOrigin();
    await confirmDialog();

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
    assertEquals(5, getAllSitesList().items[0]!.numCookies);
  });

  test('remove partitioned origin', async function() {
    const siteGroup = createMultipleSiteGroup();
    siteGroup.origins[0]!.isPartitioned = true;
    siteGroup.origins[0]!.numCookies = 1;
    siteGroup.origins[1]!.numCookies = 2;
    siteGroup.origins[2]!.numCookies = 3;
    siteGroup.numCookies = 6;

    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    // Remove the the partitioned entry, which will have been ordered to the
    // bottom of the displayed origins.
    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    const originList = siteEntries[0]!.$.originList.get();
    const originEntries = originList.querySelectorAll('.hr');
    assertEquals(3, originEntries.length);
    const removeOriginButton =
        originEntries[2]!.querySelector<HTMLElement>('#removeOriginButton');
    assertTrue(!!removeOriginButton);
    removeOriginButton.click();
    await confirmDialog();

    const [origin, groupingKey] =
        await browserProxy.whenCalled('clearPartitionedOriginDataAndCookies');

    assertEquals(siteGroup.origins[0]!.origin, origin);
    assertEquals(siteGroup.groupingKey, groupingKey);
    assertEquals(
        1, browserProxy.getCallCount('clearPartitionedOriginDataAndCookies'));
    assertEquals(0, browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(5, getAllSitesList().items[0]!.numCookies);
  });

  test('remove last origin keeps eTLD+1 cookies placeholder', async function() {
    // A group with a single origin and additional cookies scoped to the
    // eTLD+1 itself.
    const siteGroup = createSiteGroup('foo.com', 'foo.com', [
      'https://www.foo.com/',
    ]);
    siteGroup.origins[0]!.numCookies = 1;
    siteGroup.numCookies = 3;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    const siteEntries =
        testElement.$.listContainer.querySelectorAll('site-entry');
    assertEquals(1, siteEntries.length);
    const originList = siteEntries[0]!.$.originList.get();
    const originEntries = originList.querySelectorAll('.hr');
    assertEquals(1, originEntries.length);
    const removeOriginButton =
        originEntries[0]!.querySelector<HTMLElement>('#removeOriginButton');
    assertTrue(!!removeOriginButton);
    removeOriginButton.click();
    await confirmDialog();

    // The remaining cookies are held by a placeholder origin for the eTLD+1,
    // matching what the backend sends, not by the removed origin.
    const updatedGroup = getAllSitesList().items[0]!;
    assertEquals('foo.com', updatedGroup.etldPlus1);
    assertEquals(2, updatedGroup.numCookies);
    assertEquals(1, updatedGroup.origins.length);
    assertEquals('http://foo.com/', updatedGroup.origins[0]!.origin);
    assertEquals(2, updatedGroup.origins[0]!.numCookies);
  });

  test('cancel remove site group', async function() {
    const siteGroup = createMultipleSiteGroup();
    testElement.siteGroupMap.set(siteGroup.groupingKey, siteGroup);
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstSiteGroup();
    await cancelDialog();

    assertEquals(0, browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(1, getAllSitesList().items.length);
    assertEquals(0, browserProxy.getCallCount('clearSiteGroupDataAndCookies'));
  });

  test('cancel remove origin', async function() {
    const siteGroup = createMultipleSiteGroup();
    siteGroup.origins[0]!.numCookies = 1;
    siteGroup.origins[1]!.numCookies = 2;
    siteGroup.origins[2]!.numCookies = 3;
    siteGroup.numCookies = 6;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstOrigin();
    await cancelDialog();

    assertEquals(
        0, browserProxy.getCallCount('clearUnpartitionedOriginDataAndCookies'));
    assertEquals(0, browserProxy.getCallCount('setOriginPermissions'));
    assertEquals(6, getAllSitesList().items[0]!.numCookies);
  });

  test('permissions bullet point visbility', async function() {
    const siteGroup = createMultipleSiteGroup();
    siteGroup.origins[0]!.hasPermissionSettings = true;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstOrigin();
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    assertTrue(isChildVisible(testElement, '#permissionsBulletPoint'));
    await cancelDialog();

    await removeFirstSiteGroup();
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    assertTrue(isChildVisible(testElement, '#permissionsBulletPoint'));
    await cancelDialog();

    siteGroup.origins[0]!.hasPermissionSettings = false;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstOrigin();
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    assertFalse(isChildVisible(testElement, '#permissionsBulletPoint'));
    await cancelDialog();

    await removeFirstSiteGroup();
    assertTrue(testElement.$.confirmRemoveSite.get().open);
    assertFalse(isChildVisible(testElement, '#permissionsBulletPoint'));
    await cancelDialog();
  });

  test('dynamic strings', async function() {
    function assertRemoveSiteDialogText(
        expectedTitle: string, expectedLogout: string) {
      const removeSiteTitle =
          testElement.shadowRoot.querySelector<HTMLElement>('#removeSiteTitle');
      assertTrue(!!removeSiteTitle);
      assertEquals(expectedTitle, removeSiteTitle.textContent.trim());
      const logoutBulletPoint =
          testElement.shadowRoot.querySelector<HTMLElement>(
              '#logoutBulletPoint');
      assertTrue(!!logoutBulletPoint);
      assertEquals(expectedLogout, logoutBulletPoint.textContent.trim());
    }

    // Single origin, no apps.
    const siteGroup = createMultipleSiteGroup();
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstOrigin();
    assertRemoveSiteDialogText(
        getSubstitutedString(
            'siteSettingsRemoveSiteOriginDialogTitle', 'subdomain.example.com'),
        getString('siteSettingsRemoveSiteOriginLogout'));
    await cancelDialog();

    // Site group, multiple origins, no apps.
    await removeFirstSiteGroup();
    assertRemoveSiteDialogText(
        getSubstitutedString(
            'siteSettingsRemoveSiteGroupDialogTitle', 'example.com'),
        getString('siteSettingsRemoveSiteGroupLogout'));
    await cancelDialog();

    // Single origin with app.
    siteGroup.origins[0]!.isInstalled = true;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstOrigin();
    assertRemoveSiteDialogText(
        getSubstitutedString(
            'siteSettingsRemoveSiteOriginAppDialogTitle',
            'subdomain.example.com'),
        getString('siteSettingsRemoveSiteOriginLogout'));
    await cancelDialog();

    // Site group, multiple origins, with single app.
    await removeFirstSiteGroup();
    assertRemoveSiteDialogText(
        getSubstitutedString(
            'siteSettingsRemoveSiteGroupAppDialogTitle', 'example.com'),
        getString('siteSettingsRemoveSiteGroupLogout'));
    await cancelDialog();

    // Site group, multiple sites, multiple apps.
    siteGroup.origins[1]!.isInstalled = true;
    testElement.siteGroupMap.set(
        siteGroup.groupingKey, structuredClone(siteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstSiteGroup();
    assertRemoveSiteDialogText(
        getSubstitutedString(
            'siteSettingsRemoveSiteGroupAppPluralDialogTitle', 'example.com'),
        getString('siteSettingsRemoveSiteGroupLogout'));
    await cancelDialog();

    // Site group, single origin, no app.
    const singleOriginSiteGroup = createSingleSiteGroup();
    testElement.siteGroupMap.set(
        singleOriginSiteGroup.groupingKey,
        structuredClone(singleOriginSiteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstSiteGroup();
    assertRemoveSiteDialogText(
        getSubstitutedString(
            'siteSettingsRemoveSiteOriginDialogTitle', 'single.example.com'),
        getString('siteSettingsRemoveSiteOriginLogout'));
    await cancelDialog();

    // Site group, single origin, one app.
    singleOriginSiteGroup.origins[0]!.isInstalled = true;
    testElement.siteGroupMap.set(
        singleOriginSiteGroup.groupingKey,
        structuredClone(singleOriginSiteGroup));
    testElement.forceListUpdateForTesting();
    await microtasksFinished();

    await removeFirstSiteGroup();
    assertRemoveSiteDialogText(
        getSubstitutedString(
            'siteSettingsRemoveSiteOriginAppDialogTitle', 'single.example.com'),
        getString('siteSettingsRemoveSiteOriginLogout'));
  });

  test('subpageScrollTarget deactivates on route change', async function() {
    const scrollTarget = document.createElement('div');
    setGlobalScrollTarget(scrollTarget);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('all-sites');
    document.body.appendChild(testElement);
    setUpAllSites(createPrefsVarious());
    await microtasksFinished();
    assertEquals(scrollTarget, testElement.scrollTarget);
    assertEquals(scrollTarget, testElement.subpageScrollTarget);
    assertEquals(scrollTarget, getAllSitesList().scrollTarget);

    Router.getInstance().navigateTo(routes.BASIC);
    // Deactivation is deferred by a task.
    await microtasksFinished();
    assertNull(testElement.subpageScrollTarget);
    assertNull(testElement.shadowRoot.querySelector('cr-infinite-list'));

    Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
    await microtasksFinished();
    assertEquals(scrollTarget, testElement.subpageScrollTarget);
    assertEquals(scrollTarget, getAllSitesList().scrollTarget);
  });

  test('reloading route removes stale deleted sites', async function() {
    setUpAllSites(createPrefsVarious());
    testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    testElement.onStorageListFetched([
      createSiteGroup(
          'storage-only.com', 'storage-only.com',
          ['https://storage-only.com'], 200),
    ]);
    await microtasksFinished();
    assertEquals(4, testElement.siteGroupMap.size);
    assertEquals(4, getAllSitesList().items.length);

    // Simulate clearing bar.com, google.com, and storage-only.com on another
    // subpage (such as Site Details) and navigating back to All Sites.
    browserProxy.resetResolver('getAllSites');
    browserProxy.setPrefs(createSiteSettingsPrefs([], [
      createContentSettingTypeToValuePair(
          ContentSettingsTypes.GEOLOCATION,
          [createRawSiteException('https://foo.com')]),
    ]));
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_SITE_DETAILS);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
    await browserProxy.whenCalled('getAllSites');
    testElement.onStorageListFetched([]);
    await microtasksFinished();

    assertEquals(1, testElement.siteGroupMap.size);
    assertTrue(testElement.siteGroupMap.has(groupingKey('foo.com')));
    assertFalse(testElement.siteGroupMap.has(groupingKey('bar.com')));
    assertFalse(testElement.siteGroupMap.has(groupingKey('storage-only.com')));
    assertEquals(1, getAllSitesList().items.length);
  });
});

suite('AllSitesFocus', function() {
  let testElement: AllSitesElement;
  let browserProxy: TestSiteSettingsBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    MetricsBrowserProxyImpl.setInstance(new TestMetricsBrowserProxy());
    testElement = document.createElement('all-sites');
    document.body.appendChild(testElement);
  });

  teardown(function() {
    Router.getInstance().resetRouteForTesting();
  });

  function setUpAllSites(prefs: SiteSettingsPref) {
    browserProxy.setPrefs(prefs);
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
  }

  test(
      'focus restored to remove button when focused row is removed',
      async function() {
        setUpAllSites(createPrefsVarious());
        testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
        await browserProxy.whenCalled('getAllSites');
        await microtasksFinished();

        let siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(3, siteEntries.length);

        const lastRemoveButton =
            siteEntries[2]!.shadowRoot.querySelector<HTMLElement>(
                '#removeSiteButton');
        assertTrue(!!lastRemoveButton);
        lastRemoveButton.focus();
        await microtasksFinished();
        assertEquals(
            lastRemoveButton, siteEntries[2]!.shadowRoot.activeElement);

        const newMap = new Map(testElement.siteGroupMap);
        newMap.delete(siteEntries[2]!.siteGroup!.groupingKey);
        testElement.siteGroupMap = newMap;
        testElement.forceListUpdateForTesting();
        const allSitesList =
            testElement.shadowRoot.querySelector('cr-infinite-list');
        assertTrue(!!allSitesList);
        await eventToPromise('viewport-filled', allSitesList);
        await microtasksFinished();

        siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(2, siteEntries.length);
        assertEquals(
            siteEntries[1]!.shadowRoot.querySelector('#removeSiteButton'),
            siteEntries[1]!.shadowRoot.activeElement);
      });

  test(
      'focusOnLastSelectedEntry clamps to filteredList length',
      async function() {
        setUpAllSites(createPrefsVarious());
        testElement.currentRouteChanged(routes.SITE_SETTINGS_ALL);
        await browserProxy.whenCalled('getAllSites');
        testElement.filter = 'foo';
        await microtasksFinished();

        const siteEntries =
            testElement.$.listContainer.querySelectorAll('site-entry');
        assertEquals(1, siteEntries.length);

        testElement.fire(
            'site-entry-selected', {item: siteEntries[0]!.siteGroup, index: 2});
        testElement.onStorageListFetched([]);
        await microtasksFinished();

        assertEquals(
            siteEntries[0]!.shadowRoot.querySelector('.subpage-arrow'),
            siteEntries[0]!.shadowRoot.activeElement);
      });
});
