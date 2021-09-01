// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {ProfileData, Tab, TabGroup, TabGroupColor, TabSearchApiProxyImpl, TabSearchAppElement, TabSearchSearchField, TabUpdateInfo} from 'chrome://tab-search.top-chrome/tab_search.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.js';

import {generateSampleDataFromSiteNames, generateSampleRecentlyClosedTabs, generateSampleTabsFromSiteNames, SAMPLE_RECENTLY_CLOSED_DATA, SAMPLE_WINDOW_DATA, SAMPLE_WINDOW_HEIGHT, sampleData, sampleToken} from './tab_search_test_data.js';
import {initLoadTimeDataWithDefaults, initProfileDataWithDefaults} from './tab_search_test_helper.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabSearchAppTest', () => {
  /** @type {!TabSearchAppElement} */
  let tabSearchApp;
  /** @type {!TestTabSearchApiProxy} */
  let testProxy;

  /**
   * @param {!NodeList<!Element>} rows
   * @param {!Array<number>} ids
   */
  function verifyTabIds(rows, ids) {
    assertEquals(ids.length, rows.length);
    rows.forEach((row, index) => {
      assertEquals(ids[index].toString(), row.getAttribute('id'));
    });
  }

  /**
   * @return {!NodeList<!Element>}
   */
  function queryRows() {
    return tabSearchApp.shadowRoot.querySelector('#tabsList')
        .querySelectorAll('tab-search-item, tab-search-group-item');
  }

  /**
   * @param {!Object} sampleData A mock data object containing relevant profile
   *     data for the test.
   * @param {Object=} loadTimeOverriddenData
   */
  async function setupTest(sampleData, loadTimeOverriddenData) {
    initProfileDataWithDefaults(/** @type {ProfileData} */ (sampleData));
    initLoadTimeDataWithDefaults(loadTimeOverriddenData);

    testProxy = new TestTabSearchApiProxy();
    testProxy.setProfileData(/** @type {ProfileData} */ (sampleData));
    TabSearchApiProxyImpl.instance_ = testProxy;

    tabSearchApp = /** @type {!TabSearchAppElement} */
        (document.createElement('tab-search-app'));

    document.body.innerHTML = '';
    document.body.appendChild(tabSearchApp);
    await flushTasks();
  }

  test('return all tabs', async () => {
    await setupTest(sampleData());
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
  });

  test('recently closed tab groups and tabs', async () => {
    const sampleSessionId = 101;
    const sampleTabCount = 5;
    await setupTest(
        {
          windows: [{
            active: true,
            height: SAMPLE_WINDOW_HEIGHT,
            tabs: generateSampleTabsFromSiteNames(['OpenTab1'], true),
          }],
          recentlyClosedTabs: generateSampleRecentlyClosedTabs(
              'Sample Tab', sampleTabCount, sampleToken(0, 1)),
          recentlyClosedTabGroups: [{
            sessionId: sampleSessionId,
            id: sampleToken(0, 1),
            color: 1,
            title: 'Reading List',
            tabCount: sampleTabCount,
            lastActiveTime: {internalValue: BigInt(sampleTabCount + 1)},
            lastActiveElapsedText: ''
          }],
          recentlyClosedSectionExpanded: true,
        },
        {
          recentlyClosedDefaultItemDisplayCount: 5,
        });

    tabSearchApp.shadowRoot.querySelector('#tabsList')
        .ensureAllDomItemsAvailable();

    // Assert the recently closed tab group is included in the recently closed
    // items section and that the recently closed tabs belonging to it are
    // filtered from the recently closed items section by default.
    assertEquals(2, queryRows().length);
  });

  test('return all open and recently closed tabs', async () => {
    await setupTest({
      windows: SAMPLE_WINDOW_DATA,
      recentlyClosedTabs: SAMPLE_RECENTLY_CLOSED_DATA,
      recentlyClosedSectionExpanded: true,
    });
    tabSearchApp.shadowRoot.querySelector('#tabsList')
        .ensureAllDomItemsAvailable();

    assertEquals(8, queryRows().length);
  });

  test('Limit recently closed tabs to the default display count', async () => {
    await setupTest(
        {
          windows: [{
            active: true,
            height: SAMPLE_WINDOW_HEIGHT,
            tabs: generateSampleTabsFromSiteNames(['OpenTab1'], true),
          }],
          recentlyClosedTabs: generateSampleTabsFromSiteNames(
              ['RecentlyClosedTab1', 'RecentlyClosedTab2'], false),
          recentlyClosedSectionExpanded: true
        },
        {
          recentlyClosedDefaultItemDisplayCount: 1,
        });

    assertEquals(2, queryRows().length);
  });

  test('Default tab selection when data is present', async () => {
    await setupTest(sampleData());
    assertNotEquals(-1, tabSearchApp.getSelectedIndex(),
        'No default selection in the presence of data');
  });

  test('Search text changes tab items', async () => {
    await setupTest({
      windows: SAMPLE_WINDOW_DATA,
      recentlyClosedTabs: SAMPLE_RECENTLY_CLOSED_DATA,
      recentlyClosedSectionExpanded: true,
    });
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    searchField.setValue('bing');
    await flushTasks();
    verifyTabIds(queryRows(), [2]);
    assertEquals(0, tabSearchApp.getSelectedIndex());

    searchField.setValue('paypal');
    await flushTasks();
    verifyTabIds(queryRows(), [100]);
    assertEquals(0, tabSearchApp.getSelectedIndex());
  });

  test('Search text changes recently closed tab items', async () => {
    const sampleSessionId = 101;
    const sampleTabCount = 5;
    await setupTest(
        {
          windows: [{
            active: true,
            height: SAMPLE_WINDOW_HEIGHT,
            tabs: generateSampleTabsFromSiteNames(['Open sample tab'], true),
          }],
          recentlyClosedTabs: generateSampleRecentlyClosedTabs(
              'Sample Tab', sampleTabCount, sampleToken(0, 1)),
          recentlyClosedTabGroups: [({
            sessionId: sampleSessionId,
            id: sampleToken(0, 1),
            color: 1,
            title: 'Reading List',
            tabCount: sampleTabCount,
            lastActiveTime: {internalValue: BigInt(sampleTabCount + 1)},
            lastActiveElapsedText: ''
          })],
          recentlyClosedSectionExpanded: true,
        },
        {
          recentlyClosedDefaultItemDisplayCount: 5,
        });

    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    searchField.setValue('sample');
    await flushTasks();

    // Assert that the recently closed items associated to a recently closed
    // group as well as the open tabs are rendered when applying a search
    // criteria matching their titles.
    assertEquals(6, queryRows().length);
  });

  test('No tab selected when there are no search matches', async () => {
    await setupTest(sampleData());
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    searchField.setValue('Twitter');
    await flushTasks();
    assertEquals(0, queryRows().length);
    assertEquals(-1, tabSearchApp.getSelectedIndex());
  });

  test('Click on tab item triggers actions', async () => {
    const tabData = {
      index: 0,
      tabId: 1,
      title: 'Google',
      url: {url: 'https://www.google.com'},
    };
    await setupTest({
      windows: [{active: true, tabs: [tabData]}],
    });

    const tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item'));
    tabSearchItem.click();
    const [tabInfo] = await testProxy.whenCalled('switchToTab');
    assertEquals(tabData.tabId, tabInfo.tabId);

    const tabSearchItemCloseButton = /** @type {!HTMLElement} */ (
        tabSearchItem.shadowRoot.querySelector('cr-icon-button'));
    tabSearchItemCloseButton.click();
    const [tabId, withSearch, closedTabIndex] =
        await testProxy.whenCalled('closeTab');
    assertEquals(tabData.tabId, tabId);
    assertFalse(withSearch);
    assertEquals(0, closedTabIndex);
  });

  test('Click on recently closed tab item triggers action', async () => {
    const tabData = {
      tabId: 100,
      title: 'PayPal',
      url: {url: 'https://www.paypal.com'},
      lastActiveTimeTicks: {internalValue: BigInt(11)},
      lastActiveElapsedText: '',
    };

    await setupTest({
      windows: [{
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: [{
          index: 0,
          tabId: 1,
          title: 'Google',
          url: {url: 'https://www.google.com'},
        }]
      }],
      recentlyClosedTabs: [tabData],
      recentlyClosedSectionExpanded: true
    });

    let tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item[id="100"]'));
    tabSearchItem.click();
    const [tabId, withSearch, isTab] =
        await testProxy.whenCalled('openRecentlyClosedEntry');
    assertEquals(tabData.tabId, tabId);
    assertFalse(withSearch);
    assertTrue(isTab);
  });

  test('Click on recently closed tab group item triggers action', async () => {
    const tabGroupData = {
      sessionId: 101,
      id: sampleToken(0, 1),
      title: 'My Favorites',
      color: TabGroupColor.kBlue,
      tabCount: 1,
      lastActiveTime: {internalValue: BigInt(11)},
      lastActiveElapsedText: '',
    };

    await setupTest({
      windows: [{
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: [{
          index: 0,
          tabId: 1,
          title: 'Google',
          url: {url: 'https://www.google.com'},
        }]
      }],
      recentlyClosedTabGroups: [tabGroupData],
      recentlyClosedSectionExpanded: true
    });

    let tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-group-item'));
    tabSearchItem.click();
    const [id, withSearch, isTab] =
        await testProxy.whenCalled('openRecentlyClosedEntry');
    assertEquals(tabGroupData.sessionId, id);
    assertFalse(withSearch);
    assertFalse(isTab);
  });

  test('Keyboard navigation on an empty list', async () => {
    await setupTest({
      windows: [{active: true, tabs: []}],
    });

    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector("#searchField"));

    keyDownOn(searchField, 0, [], 'ArrowUp');
    assertEquals(-1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'ArrowDown');
    assertEquals(-1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'Home');
    assertEquals(-1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'End');
    assertEquals(-1, tabSearchApp.getSelectedIndex());
  });

  test('Keyboard navigation abides by item list range boundaries', async () => {
    const testData = sampleData();
    await setupTest(testData);

    const numTabs =
        testData.windows.reduce((total, w) => total + w.tabs.length, 0);
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector("#searchField"));

    keyDownOn(searchField, 0, [], 'ArrowUp');
    assertEquals(numTabs - 1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'ArrowDown');
    assertEquals(0, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'ArrowDown');
    assertEquals(1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'ArrowUp');
    assertEquals(0, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'End');
    assertEquals(numTabs - 1, tabSearchApp.getSelectedIndex());

    keyDownOn(searchField, 0, [], 'Home');
    assertEquals(0, tabSearchApp.getSelectedIndex());
  });

  test(
      'Verify all list items are present when Shift+Tab navigating from the search field to the last item',
      async () => {
        const siteNames = Array.from({length: 20}, (_, i) => 'site' + (i + 1));
        const testData = generateSampleDataFromSiteNames(siteNames);
        await setupTest(testData);

        const numTabs =
            testData.windows.reduce((total, w) => total + w.tabs.length, 0);
        const searchField = /** @type {!TabSearchSearchField} */
            (tabSearchApp.shadowRoot.querySelector('#searchField'));

        keyDownOn(searchField, 0, ['shift'], 'Tab');
        await waitAfterNextRender(tabSearchApp);

        // Since default actions are not triggered via simulated events we rely
        // on asserting the expected DOM item count necessary to focus the last
        // item is present.
        assertEquals(siteNames.length, queryRows().length);
      });

  test('Key with modifiers should not affect selected item', async () => {
    await setupTest(sampleData());

    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));

    for (const key of ['ArrowUp', 'ArrowDown', 'Home', 'End']) {
      keyDownOn(searchField, 0, ['shift'], key);
      assertEquals(0, tabSearchApp.getSelectedIndex());
    }
  });

  test('refresh on tabs changed', async () => {
    await setupTest(sampleData());
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    testProxy.getCallbackRouterRemote().tabsChanged({
      windows: [],
      recentlyClosedTabs: [],
      tabGroups: [],
      recentlyClosedTabGroups: [],
    });
    await flushTasks();
    verifyTabIds(queryRows(), []);
    assertEquals(-1, tabSearchApp.getSelectedIndex());
  });

  test('On tabs changed, tab item selection preserved or updated', async () => {
    const testData = sampleData();
    await setupTest(testData);

    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    keyDownOn(searchField, 0, [], 'ArrowDown');
    assertEquals(1, tabSearchApp.getSelectedIndex());

    testProxy.getCallbackRouterRemote().tabsChanged({
      windows: [testData.windows[0]],
      recentlyClosedTabs: [],
      tabGroups: [],
      recentlyClosedTabGroups: [],
    });
    await flushTasks();
    assertEquals(1, tabSearchApp.getSelectedIndex());

    testProxy.getCallbackRouterRemote().tabsChanged({
      windows: [{active: true, tabs: [testData.windows[0].tabs[0]]}],
      recentlyClosedTabs: [],
      tabGroups: [],
      recentlyClosedTabGroups: [],
    });
    await flushTasks();
    assertEquals(0, tabSearchApp.getSelectedIndex());
  });

  test('refresh on tab updated', async () => {
    await setupTest(sampleData());
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    let tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item[id="1"]'));
    assertEquals('Google', tabSearchItem.data.tab.title);
    assertEquals('https://www.google.com', tabSearchItem.data.tab.url.url);
    const updatedTab = /** @type {!Tab} */ ({
      index: 0,
      tabId: 1,
      title: 'Example',
      url: {url: 'https://example.com'},
      lastActiveTimeTicks: {internalValue: BigInt(5)},
      lastActiveElapsedText: '',
    });
    const tabUpdateInfo = /** @type {!TabUpdateInfo} */ ({
      inActiveWindow: true,
      tab: updatedTab,
    });
    testProxy.getCallbackRouterRemote().tabUpdated(tabUpdateInfo);
    await flushTasks();
    // tabIds are not changed after tab updated.
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item[id="1"]'));
    assertEquals(updatedTab.title, tabSearchItem.data.tab.title);
    assertEquals(updatedTab.url.url, tabSearchItem.data.tab.url.url);
    assertEquals('example.com', tabSearchItem.data.hostname);
  });

  test('tab update for tab not in profile data adds tab to list', async () => {
    await setupTest({
      windows: [{
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: generateSampleTabsFromSiteNames(['OpenTab1'], true),
      }],
    });
    verifyTabIds(queryRows(), [1]);

    const updatedTab = /** @type {!Tab} */ ({
      index: 1,
      tabId: 2,
      title: 'Example',
      url: {url: 'https://example.com'},
      lastActiveTimeTicks: {internalValue: BigInt(5)},
      lastActiveElapsedText: '',
    });
    const tabUpdateInfo = /** @type {!TabUpdateInfo} */ ({
      inActiveWindow: true,
      tab: updatedTab,
    });
    testProxy.getCallbackRouterRemote().tabUpdated(tabUpdateInfo);
    await flushTasks();
    verifyTabIds(queryRows(), [2, 1]);
  });

  test('refresh on tabs removed', async () => {
    await setupTest(sampleData());
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    testProxy.getCallbackRouterRemote().tabsRemoved([1, 2]);
    await flushTasks();
    verifyTabIds(queryRows(), [5, 6, 3, 4]);
  });

  test('Verify visibilitychange triggers data fetch', async () => {
    await setupTest(sampleData());
    assertEquals(1, testProxy.getCallCount('getProfileData'));

    // When hidden visibilitychange should not trigger the data callback.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await flushTasks();
    assertEquals(1, testProxy.getCallCount('getProfileData'));

    // When visible visibilitychange should trigger the data callback.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await flushTasks();
    assertEquals(2, testProxy.getCallCount('getProfileData'));
  });

  test('Verify hiding document resets selection and search text', async () => {
    await setupTest(sampleData());
    assertEquals(1, testProxy.getCallCount('getProfileData'));

    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    searchField.setValue('Apple');
    await flushTasks();
    verifyTabIds(queryRows(), [6, 4]);
    keyDownOn(searchField, 0, [], 'ArrowDown');
    assertEquals('Apple', tabSearchApp.getSearchTextForTesting());
    assertEquals(1, tabSearchApp.getSelectedIndex());

    // When hidden visibilitychange should reset selection and search text.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await flushTasks();
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    assertEquals('', tabSearchApp.getSearchTextForTesting());
    assertEquals(0, tabSearchApp.getSelectedIndex());

    // State should match that of the hidden state when visible again.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await flushTasks();
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    assertEquals('', tabSearchApp.getSearchTextForTesting());
    assertEquals(0, tabSearchApp.getSelectedIndex());
  });

  test('Verify tab switch is logged correctly', async () => {
    await setupTest(sampleData());
    // Make sure that tab data has been recieved.
    verifyTabIds(queryRows(), [ 1, 5, 6, 2, 3, 4 ]);

    // Click the first element with tabId 1.
    let tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item[id="1"]'));
    tabSearchItem.click();

    // Assert switchToTab() was called appropriately for an unfiltered tab list.
    await testProxy.whenCalled('switchToTab')
        .then(([tabInfo, withSearch, switchedTabIndex]) => {
          assertEquals(1, tabInfo.tabId);
          assertFalse(withSearch);
          assertEquals(0, switchedTabIndex);
        });

    testProxy.reset();
    // Click the first element with tabId 6.
    tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item[id="6"]'));
    tabSearchItem.click();

    // Assert switchToTab() was called appropriately for an unfiltered tab list.
    await testProxy.whenCalled('switchToTab')
        .then(([tabInfo, withSearch, switchedTabIndex]) => {
          assertEquals(6, tabInfo.tabId);
          assertFalse(withSearch);
          assertEquals(2, switchedTabIndex);
        });

    // Force a change to filtered tab data that would result in a
    // re-render.
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    searchField.setValue('bing');
    await flushTasks();
    verifyTabIds(queryRows(), [ 2 ]);

    testProxy.reset();
    // Click the only remaining element with tabId 2.
    tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item[id="2"]'));
    tabSearchItem.click();

    // Assert switchToTab() was called appropriately for a tab list fitlered by
    // the search query.
    await testProxy.whenCalled('switchToTab')
        .then(([tabInfo, withSearch, switchedTabIndex]) => {
          assertEquals(2, tabInfo.tabId);
          assertTrue(withSearch);
          assertEquals(0, switchedTabIndex);
        });
  });

  test('Verify showUI() is called correctly', async () => {
    await setupTest(sampleData());
    await waitAfterNextRender(tabSearchApp);

    // Make sure that tab data has been received.
    verifyTabIds(queryRows(), [ 1, 5, 6, 2, 3, 4 ]);

    // Ensure that showUI() has been called after the initial data has been
    // rendered.
    await testProxy.whenCalled('showUI');

    // Force a change to filtered tab data that would result in a
    // re-render.
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    searchField.setValue('bing');
    await flushTasks();
    await waitAfterNextRender(tabSearchApp);
    verifyTabIds(queryRows(), [ 2 ]);

    // |showUI()| should still have only been called once.
    assertEquals(1, testProxy.getCallCount('showUI'));
  });

  test('Sort by most recent active tabs', async () => {
    const tabs = [
      {
        index: 0,
        tabId: 1,
        title: 'Google',
        url: {url: 'https://www.google.com'},
        lastActiveTimeTicks: {internalValue: BigInt(2)},
        lastActiveElapsedText: '',
      },
      {
        index: 1,
        tabId: 2,
        title: 'Bing',
        url: {url: 'https://www.bing.com'},
        lastActiveTimeTicks: {internalValue: BigInt(4)},
        lastActiveElapsedText: '',
        active: true,
      },
      {
        index: 2,
        tabId: 3,
        title: 'Yahoo',
        url: {url: 'https://www.yahoo.com'},
        lastActiveTimeTicks: {internalValue: BigInt(3)},
        lastActiveElapsedText: '',
      }
    ];

    // Move active tab to the bottom of the list.
    await setupTest({
      windows: [{active: true, height: SAMPLE_WINDOW_HEIGHT, tabs}],
    });
    verifyTabIds(queryRows(), [3, 1, 2]);

    await setupTest(
        {
          windows: [{active: true, height: SAMPLE_WINDOW_HEIGHT, tabs}],
        },
        {'moveActiveTabToBottom': false});
    verifyTabIds(queryRows(), [2, 3, 1]);
  });

  test('Tab associated with TabGroup data', async () => {
    const token = sampleToken(1, 1);
    const tabs = [
      {
        index: 0,
        tabId: 1,
        groupId: token,
        title: 'Google',
        url: {url: 'https://www.google.com'},
        lastActiveTimeTicks: {internalValue: BigInt(2)},
        lastActiveElapsedText: '',
      },
    ];
    const tabGroup = /** @type {!TabGroup} */ ({
      id: token,
      color: TabGroupColor.kBlue,
      title: 'Search Engines',
    });

    await setupTest({
      windows: [{active: true, height: SAMPLE_WINDOW_HEIGHT, tabs}],
      tabGroups: [tabGroup],
    });

    let tabSearchItem = /** @type {!HTMLElement} */ (
        tabSearchApp.shadowRoot.querySelector('#tabsList')
            .querySelector('tab-search-item[id="1"]'));
    assertEquals('Google', tabSearchItem.data.tab.title);
    assertEquals('Search Engines', tabSearchItem.data.tabGroup.title);
  });

  test('Recently closed section collapse and expand', async () => {
    await setupTest({
      windows: [{
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: generateSampleTabsFromSiteNames(['SampleOpenTab'], true),
      }],
      recentlyClosedTabs: SAMPLE_RECENTLY_CLOSED_DATA,
      recentlyClosedSectionExpanded: true,
    });
    assertEquals(3, queryRows().length);

    const recentlyClosedTitleItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelectorAll('.list-section-title')[1]);
    assertNotEquals(null, recentlyClosedTitleItem);

    const recentlyClosedTitleExpandButton = /** @type {!HTMLElement} */
        (recentlyClosedTitleItem.querySelector('cr-expand-button'));
    assertNotEquals(null, recentlyClosedTitleExpandButton);

    // Collapse the `Recently Closed` section and assert item count.
    recentlyClosedTitleExpandButton.click();
    const [expanded] =
        await testProxy.whenCalled('saveRecentlyClosedExpandedPref');
    assertFalse(expanded);
    assertEquals(1, queryRows().length);

    // Expand the `Recently Closed` section and assert item count.
    recentlyClosedTitleExpandButton.click();
    assertEquals(2, testProxy.getCallCount('saveRecentlyClosedExpandedPref'));
    assertEquals(3, queryRows().length);
  });
});
