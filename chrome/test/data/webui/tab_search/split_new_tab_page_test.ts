// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {SplitNewTabPageAppElement, Tab} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TabAlertState, TabSearchApiProxyImpl} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createProfileData, createTab, SAMPLE_WINDOW_HEIGHT} from './tab_search_test_data.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

const ACTIVE_TAB_ID = 5;

function createWindowData() {
  return [
    {
      active: true,
      height: SAMPLE_WINDOW_HEIGHT,
      tabs: [
        createTab({
          tabId: 3,
          title: 'Google',
          url: {url: 'https://www.google.com'},
          visible: true,
        }),
        createTab({
          active: true,
          index: 1,
          tabId: ACTIVE_TAB_ID,
          title: 'Split View New Tab Page',
          url: {url: 'chrome://tab-search.top-chrome/split_new_tab_page.html'},
          visible: true,
        }),
        createTab({
          alertStates: [TabAlertState.kMediaRecording],
          index: 2,
          lastActiveTimeTicks: {internalValue: BigInt(2)},
          tabId: 6,
          title: 'Facebook',
          url: {url: 'https://www.facebook.com'},
        }),
        createTab({
          index: 4,
          lastActiveTimeTicks: {internalValue: BigInt(7)},
          tabId: 7,
          title: 'Expedia',
          url: {url: 'https://www.expedia.com'},
        }),
        createTab({
          index: 5,
          lastActiveTimeTicks: {internalValue: BigInt(8)},
          tabId: 8,
          title: 'Wikipedia',
          url: {url: 'https://en.wikipedia.org'},
        }),
      ],
    },
    {
      active: false,
      height: SAMPLE_WINDOW_HEIGHT,
      tabs: [
        createTab({
          active: true,
          tabId: 4,
          title: 'Apple',
          url: {url: 'https://www.apple.com/'},
        }),
      ],
    },
  ];
}

suite('SplitNewTabPageTest', () => {
  let splitNewTabPage: SplitNewTabPageAppElement;
  let testApiProxy: TestTabSearchApiProxy;

  async function splitNewTabPageSetup() {
    splitNewTabPage = document.createElement('split-new-tab-page-app');
    document.body.appendChild(splitNewTabPage);

    await eventToPromise('viewport-filled', splitNewTabPage.$.splitTabsList);
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      splitViewEnabled: true,
    });

    testApiProxy = new TestTabSearchApiProxy();
    testApiProxy.setProfileData(
        createProfileData({windows: createWindowData()}));
    testApiProxy.setIsSplit(true);
    TabSearchApiProxyImpl.setInstance(testApiProxy);
  });

  teardown(() => {
    testApiProxy.reset();
    splitNewTabPage.remove();
  });

  test('Shows correct tab count', async () => {
    await splitNewTabPageSetup();
    assertEquals(1, testApiProxy.getCallCount('getProfileData'));
    const tabSearchItems =
        splitNewTabPage.shadowRoot.querySelectorAll('tab-search-item');
    // 3 tabs should be listed, as tabs in non-active windows as well as active
    // or visible tabs in the active window should be excluded.
    assertEquals(3, tabSearchItems.length);
  });

  test('Sorts list', async () => {
    await splitNewTabPageSetup();
    const tabSearchItems =
        splitNewTabPage.shadowRoot.querySelectorAll('tab-search-item');
    assertEquals(3, tabSearchItems.length);
    // Media tabs should appear first in the list, otherwise the list should
    // be ordered from most to least recently active.
    assertEquals('Facebook', tabSearchItems[0]!.data.tab.title);
    assertEquals('Wikipedia', tabSearchItems[1]!.data.tab.title);
    assertEquals('Expedia', tabSearchItems[2]!.data.tab.title);
  });

  test('Updates on tab updated', async () => {
    await splitNewTabPageSetup();
    const initialTabSearchItems =
        splitNewTabPage.shadowRoot.querySelectorAll('tab-search-item');
    assertEquals(3, initialTabSearchItems.length);
    const tab = initialTabSearchItems[0]!.data.tab as Tab;
    assertEquals('Facebook', tab.title);

    tab.title = 'New Title';
    const tabUpdateInfo = {
      inActiveWindow: true,
      tab: tab,
    };
    testApiProxy.getCallbackRouterRemote().tabUpdated(tabUpdateInfo);
    await eventToPromise('viewport-filled', splitNewTabPage.$.splitTabsList);

    const updatedTabSearchItems =
        splitNewTabPage.shadowRoot.querySelectorAll('tab-search-item');
    assertEquals(3, updatedTabSearchItems.length);
    assertEquals('New Title', updatedTabSearchItems[0]!.data.tab.title);
  });

  test('Updates on tab visibility updated', async () => {
    const windowData = createWindowData();
    const tab = windowData[0]!.tabs[1] as Tab;
    tab.visible = false;
    testApiProxy.setProfileData(createProfileData({windows: windowData}));
    await splitNewTabPageSetup();
    const initialTabSearchItems =
        splitNewTabPage.shadowRoot.querySelectorAll('tab-search-item');
    assertEquals(4, initialTabSearchItems.length);

    tab.visible = true;
    const tabUpdateInfo = {
      inActiveWindow: true,
      tab: tab,
    };
    testApiProxy.getCallbackRouterRemote().tabUpdated(tabUpdateInfo);
    await eventToPromise('viewport-filled', splitNewTabPage.$.splitTabsList);

    const updatedTabSearchItems =
        splitNewTabPage.shadowRoot.querySelectorAll('tab-search-item');
    assertEquals(3, updatedTabSearchItems.length);
  });

  test('Updates on tabs changed', async () => {
    await splitNewTabPageSetup();
    assertEquals(
        3,
        splitNewTabPage.shadowRoot.querySelectorAll('tab-search-item').length);

    const windowData = createWindowData();
    windowData[0]!.tabs.push(createTab({
      index: 6,
      tabId: 12,
      title: 'YouTube',
      url: {url: 'https://www.youtube.com'},
    }));
    testApiProxy.getCallbackRouterRemote().tabsChanged(createProfileData({
      windows: windowData,
    }));
    await eventToPromise('viewport-filled', splitNewTabPage.$.splitTabsList);

    assertEquals(
        4,
        splitNewTabPage.shadowRoot.querySelectorAll('tab-search-item').length);
  });

  test('Updates on tabs removed', async () => {
    await splitNewTabPageSetup();
    assertEquals(
        3,
        splitNewTabPage.shadowRoot.querySelectorAll('tab-search-item').length);

    testApiProxy.getCallbackRouterRemote().tabsRemoved({
      tabIds: [6],
      recentlyClosedTabs: [],
    });
    await eventToPromise('viewport-filled', splitNewTabPage.$.splitTabsList);

    assertEquals(
        2,
        splitNewTabPage.shadowRoot.querySelectorAll('tab-search-item').length);
  });

  test('Closes current tab', async () => {
    await splitNewTabPageSetup();
    assertEquals(0, testApiProxy.getCallCount('closeTab'));
    const closeButton =
        splitNewTabPage.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!closeButton);
    closeButton.click();
    const [tabId] = await testApiProxy.whenCalled('closeTab');
    assertEquals(ACTIVE_TAB_ID, tabId);
  });

  test('Replaces tab', async () => {
    await splitNewTabPageSetup();
    assertEquals(0, testApiProxy.getCallCount('replaceActiveSplitTab'));
    const tabSearchItem =
        splitNewTabPage.shadowRoot.querySelector('tab-search-item');
    assertTrue(!!tabSearchItem);
    const tabSearchItemId = (tabSearchItem.data.tab as Tab).tabId;
    tabSearchItem.click();
    const [replacementTabId] =
        await testApiProxy.whenCalled('replaceActiveSplitTab');
    assertEquals(tabSearchItemId, replacementTabId);
  });
});
