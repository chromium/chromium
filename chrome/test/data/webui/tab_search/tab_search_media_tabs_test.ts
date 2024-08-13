// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MetricsReporterImpl} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';
import type {ProfileData, Tab, TabSearchPageElement} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TabAlertState, TabSearchApiProxyImpl} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {MockedMetricsReporter} from 'chrome://webui-test/mocked_metrics_reporter.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createProfileData, createTab, SAMPLE_WINDOW_DATA, SAMPLE_WINDOW_DATA_WITH_MEDIA_TAB, SAMPLE_WINDOW_HEIGHT} from './tab_search_test_data.js';
import {initLoadTimeDataWithDefaults} from './tab_search_test_helper.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabSearchMediaTabsTest', () => {
  let tabSearchPage: TabSearchPageElement;
  let testProxy: TestTabSearchApiProxy;

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
    initLoadTimeDataWithDefaults(loadTimeOverriddenData);

    MetricsReporterImpl.setInstanceForTest(new MockedMetricsReporter());

    testProxy = new TestTabSearchApiProxy();
    testProxy.setProfileData(sampleData);
    TabSearchApiProxyImpl.setInstance(testProxy);

    tabSearchPage = document.createElement('tab-search-page');

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(tabSearchPage);
    await eventToPromise('selected-change', tabSearchPage.$.tabsList);
    await microtasksFinished();
  }

  test('Verify initially selected tab is most recently used tab', async () => {
    await setupTest(
        createProfileData({
          windows: SAMPLE_WINDOW_DATA_WITH_MEDIA_TAB,
        }),
        {mediaTabsEnabled: true});
    assertEquals(1, tabSearchPage.getSelectedTabIndex());
    const tabSearchItems = queryRows();
    keyDownOn(tabSearchItems[1]!, 0, [], 'ArrowUp');
    await eventToPromise('selected-change', tabSearchPage.$.tabsList);
    await microtasksFinished();
    assertEquals(0, tabSearchPage.getSelectedTabIndex());

    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    // Note that unlike the 'Verify hiding document resets selection and
    // search text' test case, if no search query was originally provided
    // onSearchChanged will not be called when hidden and the index is not
    // reset until the state is visible again.
    assertEquals(-1, tabSearchPage.getSelectedTabIndex());

    // The selected tab should again be the most recently used tab.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    assertEquals(1, tabSearchPage.getSelectedTabIndex());

    // During search there should be no Audio & Video section and the selected
    // index should be 0.
    tabSearchPage.setValue('Google');
    await microtasksFinished();
    verifyTabIds(queryRows(), [2, 1]);
    assertEquals(0, tabSearchPage.getSelectedTabIndex());

    // When the search query is reset the initially selected index should also
    // be reset.
    tabSearchPage.setValue('');
    await microtasksFinished();
    assertEquals(1, tabSearchPage.getSelectedTabIndex());
  });

  test('Verify initially selected tab is not the active tab', async () => {
    const tabs = [
      createTab({
        active: false,
        alertStates: [TabAlertState.kMediaRecording],
        index: 0,
        tabId: 1,
        title: 'Meet',
        url: {url: 'https://meet.google.com/'},
        lastActiveTimeTicks: {internalValue: BigInt(4)},
      }),
      createTab({
        active: false,
        alertStates: [TabAlertState.kAudioPlaying],
        index: 1,
        tabId: 2,
        title: 'Youtube',
        url: {url: 'https://youtube.com/'},
        lastActiveTimeTicks: {internalValue: BigInt(3)},
      }),
      createTab({
        active: true,
        index: 2,
        tabId: 3,
        title: 'Google',
        url: {url: 'https://www.google.com'},
        lastActiveTimeTicks: {internalValue: BigInt(5)},
      }),
      createTab({
        active: false,
        index: 3,
        tabId: 4,
        title: 'Example',
        url: {url: 'https://www.example.com'},
        lastActiveTimeTicks: {internalValue: BigInt(2)},
      }),
    ];

    await setupTest(
        createProfileData({
          windows: [{active: true, height: SAMPLE_WINDOW_HEIGHT, tabs}],
        }),
        {mediaTabsEnabled: true});

    // MRU is the tab with Id 3 but since it is the active tab the selected
    // index should be the next MRU tab.
    assertEquals(0, tabSearchPage.getSelectedTabIndex());
  });

  test('Show media tab in Audio & Video section', async () => {
    await setupTest(
        createProfileData({windows: SAMPLE_WINDOW_DATA_WITH_MEDIA_TAB}));
    // One media tab and two non-media tabs.
    assertEquals(3, queryRows().length);
    // "Audio and Video" and "Open Tabs" section should both exist.
    assertEquals(2, queryListTitle().length);
  });


  test('Tab is no longer media tab', async () => {
    await setupTest(
        createProfileData({windows: SAMPLE_WINDOW_DATA_WITH_MEDIA_TAB}));
    const updatedTab: Tab = createTab({
      alertStates: [],
      tabId: 1,
      lastActiveTimeTicks: {internalValue: BigInt(5)},
    });

    const tabUpdateInfo = {
      inActiveWindow: true,
      tab: updatedTab,
    };
    testProxy.getCallbackRouterRemote().tabUpdated(tabUpdateInfo);
    await microtasksFinished();
    // Three non-media tabs.
    assertEquals(3, queryRows().length);
    // Only "Open Tabs" section should exist.
    assertEquals(1, queryListTitle().length);
  });


  test('Non-media tab becomes media tab', async () => {
    await setupTest(
        createProfileData({windows: SAMPLE_WINDOW_DATA}),
    );
    assertEquals(1, queryListTitle().length);

    const updatedTab: Tab = createTab({
      alertStates: [TabAlertState.kAudioPlaying],
      tabId: 5,
      lastActiveTimeTicks: {internalValue: BigInt(1)},
    });

    const tabUpdateInfo = {
      inActiveWindow: true,
      tab: updatedTab,
    };
    testProxy.getCallbackRouterRemote().tabUpdated(tabUpdateInfo);
    await microtasksFinished();
    // One media tab, the rest are non-media tabs.
    assertEquals(6, queryRows().length);
    // "Audio and Video" and "Open Tabs" section should both exist.
    assertEquals(2, queryListTitle().length);
  });


  test('Search for media tab', async () => {
    await setupTest(
        createProfileData({windows: SAMPLE_WINDOW_DATA_WITH_MEDIA_TAB}));
    tabSearchPage.setValue('google');
    await microtasksFinished();
    // No media tabs section when there is a search query.
    assertEquals(1, queryListTitle().length);
    assertEquals(2, queryRows().length);
  });
});
