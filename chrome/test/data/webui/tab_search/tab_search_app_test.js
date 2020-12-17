// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {ProfileTabs, Tab, TabSearchApiProxyImpl, TabSearchAppElement, TabSearchSearchField} from 'chrome://tab-search/tab_search.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';

import {generateSampleDataFromSiteNames, sampleData} from './tab_search_test_data.js';
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
   * @param {ProfileTabs} sampleData
   * @param {Object=} loadTimeOverriddenData
   */
  async function setupTest(sampleData, loadTimeOverriddenData) {
    testProxy = new TestTabSearchApiProxy();
    testProxy.setProfileTabs(sampleData);
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

  test('Default tab selection when data is present', async () => {
    await setupTest(sampleData());
    assertNotEquals(-1, tabSearchApp.getSelectedIndex(),
        'No default selection in the presence of data');
  });

  test('Search text changes tab items', async () => {
    await setupTest(sampleData());
    const searchField = /** @type {!TabSearchSearchField} */
      (tabSearchApp.shadowRoot.querySelector("#searchField"));
    searchField.setValue('bing');
    await flushTasks();
    verifyTabIds(queryRows(), [2]);
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
    await setupTest({windows: [{active: true, tabs: [tabData]}]});

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

  test('Keyboard navigation on an empty list', async () => {
    await setupTest({windows: [{active: true, tabs: []}]});

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
    testProxy.setProfileTabs({windows: []});
    testProxy.getCallbackRouterRemote().tabsChanged();
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

    testProxy.setProfileTabs({windows: [testData.windows[0]]});
    testProxy.getCallbackRouterRemote().tabsChanged();
    await flushTasks();
    assertEquals(1, tabSearchApp.getSelectedIndex());

    testProxy.setProfileTabs(
        {windows: [{active: true, tabs: [testData.windows[0].tabs[0]]}]});
    testProxy.getCallbackRouterRemote().tabsChanged();
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
      lastActiveTimeTicks: {internalValue: BigInt(1)},
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

  test('Verify initial tab render time is logged correctly', async () => {
    // |metricNames| tracks thow many times recordTime() has been called for
    // a metric.
    const metricNames = {};
    chrome.metricsPrivate.recordTime = (...args) => {
      if ( args[0] in metricNames ) {
        metricNames[args[0]] += 1;
      } else {
        metricNames[args[0]] = 1;
      }
    };

    await setupTest(sampleData());
    await testProxy.whenCalled('showUI');
    await waitAfterNextRender(tabSearchApp);

    // Make sure that tab data has been received.
    verifyTabIds(queryRows(), [ 1, 5, 6, 2, 3, 4 ]);

    // Ensure that |chrome.metricsPrivate.recordTime()| has been called
    // once for InitialTabsRenderTime after initial tab data has been
    // recieved.
    assertEquals(1, metricNames['Tabs.TabSearch.WebUI.InitialTabsRenderTime']);

    // Force a change to filtered tab data that would result in a
    // re-render.
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    searchField.setValue('bing');
    await flushTasks();
    await waitAfterNextRender(tabSearchApp);
    verifyTabIds(queryRows(), [ 2 ]);

    // |chrome.metricsPrivate.recordTime()| should still have only been
    // called once for InitialTabsRenderTime.
    assertEquals(1, metricNames['Tabs.TabSearch.WebUI.InitialTabsRenderTime']);
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

  test('Submit feeedback footer disabled by default', async () => {
    await setupTest(sampleData());
    assertTrue(
        tabSearchApp.shadowRoot.querySelector('#feedback-footer') === null);
  });

  test('Click on Sumit Feedback footer triggers action', async () => {
    await setupTest(sampleData(), {'submitFeedbackEnabled': true});

    const feedbackButton = /** @type {!HTMLButtonElement} */
        (tabSearchApp.shadowRoot.querySelector('#feedback-footer'));
    feedbackButton.click();
    await testProxy.whenCalled('showFeedbackPage');
  });

  test('Sort by most recent active tabs', async () => {
    const tabs = [
      {
        index: 0,
        tabId: 1,
        title: 'Google',
        url: 'https://www.google.com',
        lastActiveTimeTicks: {internalValue: BigInt(2)},
      },
      {
        index: 1,
        tabId: 2,
        title: 'Bing',
        url: 'https://www.bing.com',
        lastActiveTimeTicks: {internalValue: BigInt(4)},
        active: true,
      },
      {
        index: 2,
        tabId: 3,
        title: 'Yahoo',
        url: 'https://www.yahoo.com',
        lastActiveTimeTicks: {internalValue: BigInt(3)},
      }
    ];

    // Move active tab to the bottom of the list.
    await setupTest({windows: [{active: true, tabs}]});
    verifyTabIds(queryRows(), [3, 1, 2]);

    await setupTest(
        {windows: [{active: true, tabs}]}, {'moveActiveTabToBottom': false});
    verifyTabIds(queryRows(), [2, 3, 1]);
  });

  test('Escape key triggers close UI API', async () => {
    await setupTest(sampleData(), {'submitFeedbackEnabled': true});

    const elements = [
      tabSearchApp.shadowRoot.querySelector('#searchField'),
      tabSearchApp.shadowRoot.querySelector('#tabsList'),
      tabSearchApp.shadowRoot.querySelector('#tabsList')
          .querySelector('tab-search-item'),
      tabSearchApp.shadowRoot.querySelector('#feedback-footer'),
    ];

    for (const element of elements) {
      keyDownOn(element, 0, [], 'Escape');
    }

    assertEquals(4, testProxy.getCallCount('closeUI'));
  });
});
