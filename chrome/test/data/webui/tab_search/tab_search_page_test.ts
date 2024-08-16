// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetricsReporterImpl} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';
import type {ProfileData, RecentlyClosedTab, Tab, TabSearchItemElement, TabSearchPageElement} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TabGroupColor, TabSearchApiProxyImpl} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {MockedMetricsReporter} from 'chrome://webui-test/mocked_metrics_reporter.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createProfileData, createTab, generateSampleDataFromSiteNames, generateSampleRecentlyClosedTabs, generateSampleRecentlyClosedTabsFromSiteNames, generateSampleTabsFromSiteNames, SAMPLE_RECENTLY_CLOSED_DATA, SAMPLE_WINDOW_HEIGHT, sampleToken} from './tab_search_test_data.js';
import {initLoadTimeDataWithDefaults} from './tab_search_test_helper.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabSearchAppTest', () => {
  let tabSearchPage: TabSearchPageElement;
  let testProxy: TestTabSearchApiProxy;

  // http://crbug.com/1481787: Replace this function with
  // tabSearchPage.setValue() to be able to reproduce the bug.
  function setSearchText(text: string) {
    tabSearchPage.getSearchInput().value = text;
    tabSearchPage.onSearchTermInput();
  }

  function verifyTabIds(rows: NodeListOf<HTMLElement>, ids: number[]) {
    assertEquals(ids.length, rows.length);
    rows.forEach((row, index) => {
      assertEquals(ids[index]!.toString(), row.getAttribute('id'));
    });
  }

  function queryRows(): NodeListOf<HTMLElement> {
    return tabSearchPage.$.tabsList.querySelectorAll(
        'tab-search-item, tab-search-group-item');
  }

  function queryListTitle(): NodeListOf<HTMLElement> {
    return tabSearchPage.$.tabsList.querySelectorAll('.list-section-title');
  }

  /**
   * @param sampleData A mock data object containing relevant profile data for
   *     the test.
   */
  async function setupTest(
      sampleData: ProfileData,
      loadTimeOverriddenData?: {[key: string]: number|string|boolean}) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    initLoadTimeDataWithDefaults(loadTimeOverriddenData);

    MetricsReporterImpl.setInstanceForTest(new MockedMetricsReporter());

    testProxy = new TestTabSearchApiProxy();
    testProxy.setProfileData(sampleData);
    TabSearchApiProxyImpl.setInstance(testProxy);

    tabSearchPage = document.createElement('tab-search-page');

    document.body.appendChild(tabSearchPage);
    await eventToPromise('viewport-filled', tabSearchPage.$.tabsList);
    await microtasksFinished();
  }

  test('return all tabs', async () => {
    await setupTest(createProfileData());
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
              'Sample Tab', sampleTabCount, sampleToken(0n, 1n)),
          tabGroups: [],
          recentlyClosedTabGroups: [{
            sessionId: sampleSessionId,
            id: sampleToken(0n, 1n),
            color: 1,
            title: 'Reading List',
            tabCount: sampleTabCount,
            lastActiveTime: {internalValue: BigInt(sampleTabCount + 1)},
            lastActiveElapsedText: '',
          }],
          recentlyClosedSectionExpanded: true,
        },
        {
          recentlyClosedDefaultItemDisplayCount: 5,
        });

    await tabSearchPage.$.tabsList.ensureAllDomItemsAvailable();

    // Assert the recently closed tab group is included in the recently closed
    // items section and that the recently closed tabs belonging to it are
    // filtered from the recently closed items section by default.
    assertEquals(2, queryRows().length);
  });

  test('return all open and recently closed tabs', async () => {
    await setupTest(createProfileData({
      recentlyClosedTabs: SAMPLE_RECENTLY_CLOSED_DATA,
      recentlyClosedSectionExpanded: true,
    }));
    await tabSearchPage.$.tabsList.ensureAllDomItemsAvailable();

    assertEquals(8, queryRows().length);
  });

  test('Limit recently closed tabs to the default display count', async () => {
    await setupTest(
        createProfileData({
          windows: [{
            active: true,
            height: SAMPLE_WINDOW_HEIGHT,
            tabs: generateSampleTabsFromSiteNames(['OpenTab1'], true),
          }],
          recentlyClosedTabs: generateSampleRecentlyClosedTabsFromSiteNames(
              ['RecentlyClosedTab1', 'RecentlyClosedTab2']),
          recentlyClosedSectionExpanded: true,
        }),
        {
          recentlyClosedDefaultItemDisplayCount: 1,
        });

    const rows = queryRows();
    assertEquals(2, rows.length);
  });

  test('Default tab selection when data is present', async () => {
    await setupTest(createProfileData());
    assertNotEquals(
        -1, tabSearchPage.getSelectedTabIndex(),
        'No default selection in the presence of data');
  });

  test('Search text changes tab items', async () => {
    await setupTest(createProfileData({
      recentlyClosedTabs: SAMPLE_RECENTLY_CLOSED_DATA,
      recentlyClosedSectionExpanded: true,
    }));
    setSearchText('bing');
    await microtasksFinished();
    verifyTabIds(queryRows(), [2]);
    assertEquals(0, tabSearchPage.getSelectedTabIndex());

    setSearchText('paypal');
    await microtasksFinished();
    verifyTabIds(queryRows(), [100]);
    assertEquals(0, tabSearchPage.getSelectedTabIndex());
  });

  test('Search text changes recently closed tab items', async () => {
    const sampleSessionId = 101;
    const sampleTabCount = 5;
    await setupTest(
        createProfileData({
          windows: [{
            active: true,
            height: SAMPLE_WINDOW_HEIGHT,
            tabs: generateSampleTabsFromSiteNames(['Open sample tab'], true),
          }],
          recentlyClosedTabs: generateSampleRecentlyClosedTabs(
              'Sample Tab', sampleTabCount, sampleToken(0n, 1n)),
          recentlyClosedTabGroups: [({
            sessionId: sampleSessionId,
            id: sampleToken(0n, 1n),
            color: 1,
            title: 'Reading List',
            tabCount: sampleTabCount,
            lastActiveTime: {internalValue: BigInt(sampleTabCount + 1)},
            lastActiveElapsedText: '',
          })],
          recentlyClosedSectionExpanded: true,
        }),
        {
          recentlyClosedDefaultItemDisplayCount: 5,
        });

    setSearchText('sample');
    await microtasksFinished();

    // Assert that the recently closed items associated to a recently closed
    // group as well as the open tabs are rendered when applying a search
    // criteria matching their titles.
    assertEquals(6, queryRows().length);
  });

  test('No tab selected when there are no search matches', async () => {
    await setupTest(createProfileData());
    setSearchText('Twitter');
    await microtasksFinished();
    assertEquals(0, queryRows().length);
    assertEquals(-1, tabSearchPage.getSelectedTabIndex());
  });

  test('Click on tab item triggers actions', async () => {
    const tabData = createTab({
      title: 'Google',
      url: {url: 'https://www.google.com'},
      lastActiveTimeTicks: {internalValue: BigInt(4)},
    });
    await setupTest(createProfileData({
      windows: [{active: true, height: SAMPLE_WINDOW_HEIGHT, tabs: [tabData]}],
    }));

    const tabSearchItem =
        tabSearchPage.$.tabsList.querySelector('tab-search-item')!;
    tabSearchItem.click();
    const [tabInfo] = await testProxy.whenCalled('switchToTab');
    assertEquals(tabData.tabId, tabInfo.tabId);

    const tabSearchItemCloseButton =
        tabSearchItem.shadowRoot!.querySelector('cr-icon-button')!;
    tabSearchItemCloseButton.click();
    const [tabId] = await testProxy.whenCalled('closeTab');
    assertEquals(tabData.tabId, tabId);
  });

  test('Click on recently closed tab item triggers action', async () => {
    const tabData: RecentlyClosedTab = {
      tabId: 100,
      title: 'PayPal',
      url: {url: 'https://www.paypal.com'},
      lastActiveElapsedText: '',
      lastActiveTime: {internalValue: BigInt(11)},
      groupId: null,
    };

    await setupTest(createProfileData({
      windows: [{
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: [createTab({
          title: 'Google',
          url: {url: 'https://www.google.com'},
          lastActiveTimeTicks: {internalValue: BigInt(4)},
        })],
      }],
      recentlyClosedTabs: [tabData],
      recentlyClosedSectionExpanded: true,
    }));

    const tabSearchItem = tabSearchPage.$.tabsList.querySelector<HTMLElement>(
        'tab-search-item[id="100"]')!;
    tabSearchItem.click();
    const [tabId, withSearch, isTab, index] =
        await testProxy.whenCalled('openRecentlyClosedEntry');
    assertEquals(tabData.tabId, tabId);
    assertFalse(withSearch);
    assertTrue(isTab);
    assertEquals(0, index);
  });

  test('Click on recently closed tab group item triggers action', async () => {
    const tabGroupData = {
      sessionId: 101,
      id: sampleToken(0n, 1n),
      title: 'My Favorites',
      color: TabGroupColor.kBlue,
      tabCount: 1,
      lastActiveTime: {internalValue: BigInt(11)},
      lastActiveElapsedText: '',
    };

    await setupTest(createProfileData({
      windows: [{
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: [createTab({
          title: 'Google',
          url: {url: 'https://www.google.com'},
          lastActiveTimeTicks: {internalValue: BigInt(4)},
        })],
      }],
      recentlyClosedTabGroups: [tabGroupData],
      recentlyClosedSectionExpanded: true,
    }));

    const tabSearchItem =
        tabSearchPage.$.tabsList.querySelector('tab-search-group-item')!;
    tabSearchItem.click();
    const [id, withSearch, isTab, index] =
        await testProxy.whenCalled('openRecentlyClosedEntry');
    assertEquals(tabGroupData.sessionId, id);
    assertFalse(withSearch);
    assertFalse(isTab);
    assertEquals(0, index);
  });

  test('Keyboard navigation on an empty list', async () => {
    await setupTest(createProfileData({
      windows: [{active: true, height: SAMPLE_WINDOW_HEIGHT, tabs: []}],
    }));

    const searchField = tabSearchPage.$.searchField;

    keyDownOn(searchField, 0, [], 'ArrowUp');
    await microtasksFinished();
    assertEquals(-1, tabSearchPage.getSelectedTabIndex());

    keyDownOn(searchField, 0, [], 'ArrowDown');
    await microtasksFinished();
    assertEquals(-1, tabSearchPage.getSelectedTabIndex());

    keyDownOn(searchField, 0, [], 'Home');
    await microtasksFinished();
    assertEquals(-1, tabSearchPage.getSelectedTabIndex());

    keyDownOn(searchField, 0, [], 'End');
    await microtasksFinished();
    assertEquals(-1, tabSearchPage.getSelectedTabIndex());
  });

  test('Keyboard navigation abides by item list range boundaries', async () => {
    const testData = createProfileData();
    await setupTest(testData);

    const numTabs =
        testData.windows.reduce((total, w) => total + w.tabs.length, 0);
    const searchField = tabSearchPage.$.searchField;

    keyDownOn(searchField, 0, [], 'ArrowUp');
    await microtasksFinished();
    assertEquals(numTabs - 1, tabSearchPage.getSelectedTabIndex());

    keyDownOn(searchField, 0, [], 'ArrowDown');
    await microtasksFinished();
    assertEquals(0, tabSearchPage.getSelectedTabIndex());

    keyDownOn(searchField, 0, [], 'ArrowDown');
    await microtasksFinished();
    assertEquals(1, tabSearchPage.getSelectedTabIndex());

    keyDownOn(searchField, 0, [], 'ArrowUp');
    await microtasksFinished();
    assertEquals(0, tabSearchPage.getSelectedTabIndex());

    keyDownOn(searchField, 0, [], 'End');
    await microtasksFinished();
    assertEquals(numTabs - 1, tabSearchPage.getSelectedTabIndex());

    keyDownOn(searchField, 0, [], 'Home');
    await microtasksFinished();
    assertEquals(0, tabSearchPage.getSelectedTabIndex());
  });

  test(
      'Verify all list items are present when Shift+Tab navigating from the search field to the last item',
      async () => {
        const siteNames = Array.from({length: 20}, (_, i) => 'site' + (i + 1));
        const testData = generateSampleDataFromSiteNames(siteNames);
        await setupTest(testData);

        const searchField = tabSearchPage.$.searchField;

        keyDownOn(searchField, 0, ['shift'], 'Tab');
        await microtasksFinished();

        // Since default actions are not triggered via simulated events we rely
        // on asserting the expected DOM item count necessary to focus the last
        // item is present.
        assertEquals(siteNames.length, queryRows().length);
      });

  test('Key with modifiers should not affect selected item', async () => {
    await setupTest(createProfileData());

    const searchField = tabSearchPage.$.searchField;

    for (const key of ['ArrowUp', 'ArrowDown', 'Home', 'End']) {
      keyDownOn(searchField, 0, ['shift'], key);
      await microtasksFinished();
      assertEquals(0, tabSearchPage.getSelectedTabIndex());
    }
  });

  test('refresh on tabs changed', async () => {
    await setupTest(createProfileData());
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    testProxy.getCallbackRouterRemote().tabsChanged(
        createProfileData({windows: []}));
    await microtasksFinished();
    verifyTabIds(queryRows(), []);
    assertEquals(-1, tabSearchPage.getSelectedTabIndex());
  });

  test('On tabs changed, tab item selection preserved or updated', async () => {
    const testData = createProfileData();
    await setupTest(testData);

    const searchField = tabSearchPage.$.searchField;
    keyDownOn(searchField, 0, [], 'ArrowDown');
    assertEquals(1, tabSearchPage.getSelectedTabIndex());

    testProxy.getCallbackRouterRemote().tabsChanged(createProfileData({
      windows: [testData.windows[0]!],
    }));
    await microtasksFinished();
    assertEquals(1, tabSearchPage.getSelectedTabIndex());

    testProxy.getCallbackRouterRemote().tabsChanged(createProfileData({
      windows: [{
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: [testData.windows[0]!.tabs[0]!],
      }],
    }));
    await microtasksFinished();
    assertEquals(0, tabSearchPage.getSelectedTabIndex());
  });

  test('refresh on tab updated', async () => {
    await setupTest(createProfileData());
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    let tabSearchItem =
        tabSearchPage.$.tabsList.querySelector<TabSearchItemElement>(
            'tab-search-item[id="1"]')!;
    assertEquals('Google', tabSearchItem.data.tab.title);
    assertEquals('https://www.google.com', tabSearchItem.data.tab.url.url);
    const updatedTab: Tab = createTab({
      lastActiveTimeTicks: {internalValue: BigInt(5)},
    });
    const tabUpdateInfo = {
      inActiveWindow: true,
      tab: updatedTab,
    };
    testProxy.getCallbackRouterRemote().tabUpdated(tabUpdateInfo);
    await microtasksFinished();
    // tabIds are not changed after tab updated.
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    tabSearchItem =
        tabSearchPage.$.tabsList.querySelector('tab-search-item[id="1"]')!;
    assertEquals(updatedTab.title, tabSearchItem.data.tab.title);
    assertEquals(updatedTab.url.url, tabSearchItem.data.tab.url.url);
    assertEquals('www.example.com', tabSearchItem.data.hostname);
  });

  test('tab update for tab not in profile data adds tab to list', async () => {
    await setupTest(createProfileData({
      windows: [{
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: generateSampleTabsFromSiteNames(['OpenTab1'], true),
      }],
    }));
    verifyTabIds(queryRows(), [1]);

    const updatedTab: Tab = createTab({
      tabId: 2,
      lastActiveTimeTicks: {internalValue: BigInt(5)},
    });
    const tabUpdateInfo = {
      inActiveWindow: true,
      tab: updatedTab,
    };
    testProxy.getCallbackRouterRemote().tabUpdated(tabUpdateInfo);
    await microtasksFinished();
    verifyTabIds(queryRows(), [2, 1]);
  });

  test('refresh on tabs removed, no restore service', async () => {
    // Simulates a scenario where there is no tab restore service available for
    // the current profile and thus, on removing a tab, there are no associated
    // recently closed tabs created, such as in a OTR case.
    await setupTest(createProfileData());
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);

    testProxy.getCallbackRouterRemote().tabsRemoved({
      tabIds: [1, 2],
      recentlyClosedTabs: [],
    });
    await microtasksFinished();
    verifyTabIds(queryRows(), [5, 6, 3, 4]);

    // Assert that on removing all items, we display the no-results div.
    testProxy.getCallbackRouterRemote().tabsRemoved(
        {tabIds: [3, 4, 5, 6], recentlyClosedTabs: []});
    await microtasksFinished();
    assertNotEquals(
        null, tabSearchPage.shadowRoot!.querySelector('#no-results'));
  });

  test('Closed tab appears in recently closed section', async () => {
    await setupTest(createProfileData({
      windows: [{
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs:
            generateSampleTabsFromSiteNames(['SampleTab', 'SampleTab2'], true),
      }],
      recentlyClosedSectionExpanded: true,
    }));
    verifyTabIds(queryRows(), [1, 2]);

    testProxy.getCallbackRouterRemote().tabsRemoved({
      tabIds: [1],
      recentlyClosedTabs: [{
        groupId: null,
        tabId: 3,
        title: `SampleTab`,
        url: {url: 'https://www.sampletab.com'},
        lastActiveTime: {internalValue: BigInt(3)},
        lastActiveElapsedText: '',
      }],
    });
    await microtasksFinished();
    verifyTabIds(queryRows(), [2, 3]);
  });

  test('Verify visibilitychange triggers data fetch', async () => {
    await setupTest(createProfileData());
    assertEquals(1, testProxy.getCallCount('getProfileData'));

    // When hidden visibilitychange should not trigger the data callback.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    assertEquals(1, testProxy.getCallCount('getProfileData'));

    // When visible visibilitychange should trigger the data callback.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    assertEquals(2, testProxy.getCallCount('getProfileData'));
  });

  test('Verify hiding document resets selection and search text', async () => {
    await setupTest(createProfileData());
    assertEquals(1, testProxy.getCallCount('getProfileData'));

    const searchField = tabSearchPage.$.searchField;

    setSearchText('Apple');
    await microtasksFinished();
    verifyTabIds(queryRows(), [6, 4]);
    assertEquals(0, tabSearchPage.getSelectedTabIndex());
    keyDownOn(searchField, 0, [], 'ArrowDown');
    assertEquals('Apple', tabSearchPage.getSearchTextForTesting());
    assertEquals(1, tabSearchPage.getSelectedTabIndex());

    // When hidden visibilitychange should reset selection and search text.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    assertEquals('', tabSearchPage.getSearchTextForTesting());
    assertEquals(0, tabSearchPage.getSelectedTabIndex());

    // State should match that of the hidden state when visible again.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);
    assertEquals('', tabSearchPage.getSearchTextForTesting());
    assertEquals(0, tabSearchPage.getSelectedTabIndex());
  });

  test('Verify tab switch is called correctly', async () => {
    await setupTest(createProfileData());
    // Make sure that tab data has been recieved.
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);

    // Click the first element with tabId 1.
    let tabSearchItem = tabSearchPage.$.tabsList.querySelector<HTMLElement>(
        'tab-search-item[id="1"]')!;
    tabSearchItem.click();

    // Assert switchToTab() was called appropriately for an unfiltered tab list.
    await testProxy.whenCalled('switchToTab').then(([tabInfo]) => {
      assertEquals(1, tabInfo.tabId);
    });

    testProxy.reset();
    // Click the first element with tabId 6.
    tabSearchItem = tabSearchPage.$.tabsList.querySelector<HTMLElement>(
        'tab-search-item[id="6"]')!;
    tabSearchItem.click();

    // Assert switchToTab() was called appropriately for an unfiltered tab list.
    await testProxy.whenCalled('switchToTab').then(([tabInfo]) => {
      assertEquals(6, tabInfo.tabId);
    });

    // Force a change to filtered tab data that would result in a
    // re-render.
    setSearchText('bing');
    await microtasksFinished();
    verifyTabIds(queryRows(), [2]);

    testProxy.reset();
    // Click the only remaining element with tabId 2.
    tabSearchItem = tabSearchPage.$.tabsList.querySelector<HTMLElement>(
        'tab-search-item[id="2"]')!;
    tabSearchItem.click();

    // Assert switchToTab() was called appropriately for a tab list fitlered by
    // the search query.
    await testProxy.whenCalled('switchToTab').then(([tabInfo]) => {
      assertEquals(2, tabInfo.tabId);
    });
  });

  test('Verify notifySearchUiReadyToShow() is called correctly', async () => {
    await setupTest(createProfileData());

    // Make sure that tab data has been received.
    verifyTabIds(queryRows(), [1, 5, 6, 2, 3, 4]);

    // Ensure that notifySearchUiReadyToShow() has been called after the
    // initial data has been rendered.
    await testProxy.whenCalled('notifySearchUiReadyToShow');

    // Force a change to filtered tab data that would result in a
    // re-render.
    setSearchText('bing');
    await microtasksFinished();
    verifyTabIds(queryRows(), [2]);

    // |notifySearchUiReadyToShow()| should still have only been called once.
    assertEquals(1, testProxy.getCallCount('notifySearchUiReadyToShow'));
  });

  test('Sort by most recent active tabs', async () => {
    const tabs = [
      createTab({
        index: 0,
        tabId: 1,
        title: 'Google',
        url: {url: 'https://www.google.com'},
        lastActiveTimeTicks: {internalValue: BigInt(2)},
      }),
      createTab({
        index: 1,
        tabId: 2,
        title: 'Bing',
        url: {url: 'https://www.bing.com'},
        lastActiveTimeTicks: {internalValue: BigInt(4)},
        active: true,
      }),
      createTab({
        index: 2,
        tabId: 3,
        title: 'Yahoo',
        url: {url: 'https://www.yahoo.com'},
        lastActiveTimeTicks: {internalValue: BigInt(3)},
      }),
    ];

    // Move active tab to the bottom of the list.
    await setupTest(createProfileData({
      windows: [{active: true, height: SAMPLE_WINDOW_HEIGHT, tabs}],
    }));
    verifyTabIds(queryRows(), [3, 1, 2]);
  });

  test('Tab associated with TabGroup data', async () => {
    const token = sampleToken(1n, 1n);
    const tabs = [
      createTab({
        groupId: token,
        title: 'Google',
        url: {url: 'https://www.google.com'},
        lastActiveTimeTicks: {internalValue: BigInt(2)},
      }),
    ];
    const tabGroup = {
      id: token,
      color: TabGroupColor.kBlue,
      title: 'Search Engines',
    };

    await setupTest(createProfileData({
      windows: [{active: true, height: SAMPLE_WINDOW_HEIGHT, tabs}],
      tabGroups: [tabGroup],
    }));

    const tabSearchItem =
        tabSearchPage.$.tabsList.querySelector<TabSearchItemElement>(
            'tab-search-item[id="1"]')!;
    assertEquals('Google', tabSearchItem.data.tab.title);
    assertEquals('Search Engines', tabSearchItem.data.tabGroup!.title);
  });

  test('Recently closed section collapse and expand', async () => {
    await setupTest(createProfileData({
      windows: [{
        active: true,
        height: SAMPLE_WINDOW_HEIGHT,
        tabs: generateSampleTabsFromSiteNames(['SampleOpenTab'], true),
      }],
      recentlyClosedTabs: SAMPLE_RECENTLY_CLOSED_DATA,
      recentlyClosedSectionExpanded: true,
    }));
    assertEquals(3, queryRows().length);

    const recentlyClosedTitleItem = queryListTitle()[1];
    assertTrue(!!recentlyClosedTitleItem);

    const recentlyClosedTitleExpandButton =
        recentlyClosedTitleItem!.querySelector('cr-expand-button');
    assertTrue(!!recentlyClosedTitleExpandButton);

    // Collapse the `Recently Closed` section and assert item count.
    recentlyClosedTitleExpandButton.click();
    const [expanded] =
        await testProxy.whenCalled('saveRecentlyClosedExpandedPref');
    assertFalse(expanded);
    await microtasksFinished();
    assertEquals(1, queryRows().length);

    // Expand the `Recently Closed` section and assert item count.
    recentlyClosedTitleExpandButton.click();

    await testProxy.whenCalled('saveRecentlyClosedExpandedPref');
    assertEquals(2, testProxy.getCallCount('saveRecentlyClosedExpandedPref'));
    await microtasksFinished();
    assertEquals(3, queryRows().length);
  });

  [true, false].forEach((windowActive) => {
    test(
        `Available height set correctly when the window's active state is ${
            windowActive}`,
        async () => {
          await setupTest(
              createProfileData({
                windows: [{
                  active: windowActive,
                  height: SAMPLE_WINDOW_HEIGHT,
                  tabs: generateSampleTabsFromSiteNames(['OpenTab1'], true),
                }],
                recentlyClosedTabs:
                    generateSampleRecentlyClosedTabsFromSiteNames(
                        ['RecentlyClosedTab1', 'RecentlyClosedTab2']),
                recentlyClosedSectionExpanded: true,
              }),
              {
                recentlyClosedDefaultItemDisplayCount: 1,
              });

          assertEquals(
              SAMPLE_WINDOW_HEIGHT,
              tabSearchPage.getAvailableHeightForTesting());
        });
  });

  test('Changing active does not render extra tabs', async () => {
    const siteNames = Array.from({length: 20}, (_, i) => 'site' + (i + 1));
    const testData = generateSampleDataFromSiteNames(siteNames);
    await setupTest(testData);
    const numRows = queryRows().length + queryListTitle().length;
    const numItems = tabSearchPage.$.tabsList.items.length;
    assertGT(numItems, numRows);

    function whenVisibilityChanged(): Promise<void> {
      return new Promise(resolve => {
        const pageObserver = new IntersectionObserver((_entries, observer) => {
          resolve();
          observer.unobserve(tabSearchPage);
        }, {root: document.documentElement});
        pageObserver.observe(tabSearchPage);
      });
    }

    // Simulate switching to another tab. This line imitates the CSS in
    // cr-page-selector.
    const displayStyle =
        (tabSearchPage.computedStyleMap().get('display') as CSSKeywordValue)
            .value;
    tabSearchPage.style.display = 'none';
    await whenVisibilityChanged();
    await microtasksFinished();
    assertEquals(numRows, queryListTitle().length + queryRows().length);

    // Re-activating the tabs list should not increase the number of items.
    tabSearchPage.style.display = displayStyle;
    await whenVisibilityChanged();
    await microtasksFinished();
    assertEquals(numRows, queryListTitle().length + queryRows().length);
  });

  test('KeyPressOnSearchFieldTriggersActions', async () => {
    await setupTest(createProfileData());

    // Ensure there is a selected item.
    assertEquals(0, tabSearchPage.getSelectedTabIndex());

    keyDownOn(tabSearchPage.getSearchInput(), 0, [], 'Enter');
    // Assert switchToTab() was called appropriately for an unfiltered tab list.
    const [tabInfo] = await testProxy.whenCalled('switchToTab');
    assertEquals(1, tabInfo.tabId);
  });

  test('KeyPressOnItemTriggersActions', async () => {
    await setupTest(createProfileData());

    // Ensure there is a selected item.
    assertEquals(0, tabSearchPage.getSelectedTabIndex());
    const tabSearchItem =
        tabSearchPage.$.tabsList.querySelector('tab-search-item')!;

    keyDownOn(tabSearchItem, 0, [], 'Enter');
    // Assert switchToTab() was called appropriately for an unfiltered tab list.
    const [tabInfo] = await testProxy.whenCalled('switchToTab');
    assertEquals(1, tabInfo.tabId);
  });
});
