// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {MetricsReporterImpl} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {ProfileData, Tab, TabAlertState, TabSearchApiProxyImpl, TabSearchAppElement} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {MockedMetricsReporter} from 'chrome://webui-test/metrics_reporter/mocked_metrics_reporter.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createProfileData, createTab, SAMPLE_WINDOW_DATA, SAMPLE_WINDOW_DATA_WITH_MEDIA_TAB, SAMPLE_WINDOW_HEIGHT} from './tab_search_test_data.js';
import {initLoadTimeDataWithDefaults} from './tab_search_test_helper.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabSearchMediaTabsTest', () => {
  let tabSearchApp: TabSearchAppElement;
  let testProxy: TestTabSearchApiProxy;

  function verifyTabIds(rows: NodeListOf<HTMLElement>, ids: number[]) {
    assertEquals(ids.length, rows.length);
    rows.forEach((row, index) => {
      assertEquals(ids[index]!.toString(), row.getAttribute('id'));
    });
  }

  function queryRows(): NodeListOf<HTMLElement> {
    return tabSearchApp.$.tabsList.querySelectorAll(
        'tab-search-item, tab-search-group-item');
  }

  function queryListTitle(): NodeListOf<HTMLElement> {
    return tabSearchApp.$.tabsList.querySelectorAll('.list-section-title');
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

    tabSearchApp = document.createElement('tab-search-app');

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(tabSearchApp);
    await flushTasks();
  }

  test('Verify initially selected tab is most recently used tab', async () => {
    await setupTest(
        createProfileData({
          windows: SAMPLE_WINDOW_DATA_WITH_MEDIA_TAB,
        }),
        {mediaTabsEnabled: true});
    assertEquals(1, tabSearchApp.getSelectedIndex());
    const tabSearchItems = queryRows();
    keyDownOn(tabSearchItems[1]!, 0, [], 'ArrowUp');
    assertEquals(0, tabSearchApp.getSelectedIndex());

    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await flushTasks();
    // Note that unlike the 'Verify hiding document resets selection and
    // search text' test case, if no search query was originally provided
    // onSearchChanged will not be called when hidden and the index is not
    // reset until the state is visible again.
    assertEquals(-1, tabSearchApp.getSelectedIndex());

    // The selected tab should again be the most recently used tab.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await flushTasks();
    assertEquals(1, tabSearchApp.getSelectedIndex());

    // During search there should be no Audio & Video section and the selected
    // index should be 0.
    const searchField = tabSearchApp.$.searchField;
    searchField.setValue('Google');
    await flushTasks();
    verifyTabIds(queryRows(), [2, 1]);
    assertEquals(0, tabSearchApp.getSelectedIndex());

    // When the search query is reset the initially selected index should also
    // be reset.
    searchField.setValue('');
    await flushTasks();
    assertEquals(1, tabSearchApp.getSelectedIndex());
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
    assertEquals(0, tabSearchApp.getSelectedIndex());
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
    await flushTasks();
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
    await flushTasks();
    // One media tab, the rest are non-media tabs.
    assertEquals(6, queryRows().length);
    // "Audio and Video" and "Open Tabs" section should both exist.
    assertEquals(2, queryListTitle().length);
  });


  test('Search for media tab', async () => {
    await setupTest(
        createProfileData({windows: SAMPLE_WINDOW_DATA_WITH_MEDIA_TAB}));
    const searchField = tabSearchApp.$.searchField;
    searchField.setValue('google');
    await flushTasks();
    // No media tabs section when there is a search query.
    assertEquals(1, queryListTitle().length);
    assertEquals(2, queryRows().length);
  });
});
