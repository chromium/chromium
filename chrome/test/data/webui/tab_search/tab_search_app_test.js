// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {ProfileData, Tab, TabSearchApiProxyImpl, TabSearchAppElement, TabSearchSearchField} from 'chrome://tab-search.top-chrome/tab_search.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';

import {generateSampleDataFromSiteNames, generateSampleTabsFromSiteNames, SAMPLE_RECENTLY_CLOSED_DATA, SAMPLE_WINDOW_DATA, SAMPLE_WINDOW_HEIGHT, sampleData} from './tab_search_test_data.js';
import {initLoadTimeDataWithDefaults} from './tab_search_test_helper.js';
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
        .querySelectorAll('tab-search-item');
  }

  /**
   * @param {ProfileData} sampleData
   * @param {Object=} loadTimeOverriddenData
   */
  async function setupTest(sampleData, loadTimeOverriddenData) {
    testProxy = new TestTabSearchApiProxy();
    testProxy.setProfileData(sampleData);
    TabSearchApiProxyImpl.instance_ = testProxy;

    initLoadTimeDataWithDefaults(loadTimeOverriddenData);

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

  test('return all open and recently closed tabs', async () => {
    await setupTest({
      windows: SAMPLE_WINDOW_DATA,
      recentlyClosedTabs: SAMPLE_RECENTLY_CLOSED_DATA
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
        },
        {recentlyClosedDefaultItemDisplayCount: 1});

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
      recentlyClosedTabs: SAMPLE_RECENTLY_CLOSED_DATA
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
      url: 'https://www.google.com',
    };
    await setupTest(
        {windows: [{active: true, tabs: [tabData]}], recentlyClosedTabs: []});

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
      url: 'https://www.paypal.com',
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
          url: 'https://www.google.com',
        }]
      }],
      recentlyClosedTabs: [tabData]
    });

    let tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item[id="100"]'));
    tabSearchItem.click();
    const tabId = await testProxy.whenCalled('openRecentlyClosedTab');
    assertEquals(tabData.tabId, tabId);
  });

  test('Keyboard navigation on an empty list', async () => {
    await setupTest(
        {windows: [{active: true, tabs: []}], recentlyClosedTabs: []});

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
    testProxy.getCallbackRouterRemote().tabsChanged(
        {windows: [], recentlyClosedTabs: []});
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

    testProxy.getCallbackRouterRemote().tabsChanged(
        {windows: [testData.windows[0]], recentlyClosedTabs: []});
    await flushTasks();
    assertEquals(1, tabSearchApp.getSelectedIndex());

    testProxy.getCallbackRouterRemote().tabsChanged({
      windows: [{active: true, tabs: [testData.windows[0].tabs[0]]}],
      recentlyClosedTabs: []
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
    assertEquals('https://www.google.com', tabSearchItem.data.tab.url);
    const updatedTab = /** @type {!Tab} */ ({
      index: 0,
      tabId: 1,
      title: 'Example',
      url: 'https://example.com',
      lastActiveTimeTicks: {internalValue: BigInt(5)},
      lastActiveElapsedText: '',
    });
    testProxy.getCallbackRouterRemote().tabUpdated(updatedTab);
    await flushTasks();
    // tabIds are not changed after tab updated.
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item[id="1"]'));
    assertEquals(updatedTab.title, tabSearchItem.data.tab.title);
    assertEquals(updatedTab.url, tabSearchItem.data.tab.url);
    assertEquals('example.com', tabSearchItem.data.hostname);
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
        url: 'https://www.google.com',
        lastActiveTimeTicks: {internalValue: BigInt(2)},
        lastActiveElapsedText: '',
      },
      {
        index: 1,
        tabId: 2,
        title: 'Bing',
        url: 'https://www.bing.com',
        lastActiveTimeTicks: {internalValue: BigInt(4)},
        lastActiveElapsedText: '',
        active: true,
      },
      {
        index: 2,
        tabId: 3,
        title: 'Yahoo',
        url: 'https://www.yahoo.com',
        lastActiveTimeTicks: {internalValue: BigInt(3)},
        lastActiveElapsedText: '',
      }
    ];

    // Move active tab to the bottom of the list.
    await setupTest({
      windows: [{active: true, height: SAMPLE_WINDOW_HEIGHT, tabs}],
      recentlyClosedTabs: []
    });
    verifyTabIds(queryRows(), [3, 1, 2]);

    await setupTest(
        {
          windows: [{active: true, height: SAMPLE_WINDOW_HEIGHT, tabs}],
          recentlyClosedTabs: []
        },
        {'moveActiveTabToBottom': false});
    verifyTabIds(queryRows(), [2, 3, 1]);
  });

  test('Escape key triggers close UI API', async () => {
    await setupTest(sampleData());

    const elements = [
      tabSearchApp.shadowRoot.querySelector('#searchField'),
      tabSearchApp.shadowRoot.querySelector('#tabsList'),
      tabSearchApp.shadowRoot.querySelector('#tabsList')
          .querySelector('tab-search-item'),
    ];

    for (const element of elements) {
      keyDownOn(element, 0, [], 'Escape');
    }

    assertEquals(3, testProxy.getCallCount('closeUI'));
  });
});
