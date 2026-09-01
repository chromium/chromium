// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxyFactory, OpenTabsDelegate, PageHandlerRemote} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerListSectionClient, OrganizerListSectionItem, PageRemote, ProfileData, Tab} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

function createTab(overrides: Partial<Tab>): Tab {
  return Object.assign(
      {
        active: false,
        visible: false,
        faviconUrl: null,
        tabId: 0,
        groupId: null,
        splitId: null,
        title: '',
        url: '',
        lastActiveTime: {internalValue: 0n},
        lastActiveElapsedText: '',
        lastActiveTimeTicks: null,
        alertStates: [],
        showGroupTitle: false,
        isPinned: false,
      },
      overrides);
}

class TestClient implements OrganizerListSectionClient {
  items: Array<OrganizerListSectionItem<unknown>> = [];

  onItemsChanged(items: Array<OrganizerListSectionItem<unknown>>) {
    this.items = items;
  }
}

suite('OpenTabsDelegateTest', () => {
  let mockPageHandler: PageHandlerRemote&TestMock<PageHandlerRemote>;
  let remotePage: PageRemote;
  let delegate: OpenTabsDelegate;

  const googleTab: Tab = createTab({
    tabId: 1,
    title: 'Google',
    url: 'https://www.google.com',
    lastActiveTimeTicks: {internalValue: 100n},
    lastActiveElapsedText: '10m ago',
  });

  const youtubeTab: Tab = createTab({
    tabId: 2,
    title: 'YouTube',
    url: 'https://www.youtube.com',
    lastActiveTimeTicks: {internalValue: 300n},
    lastActiveElapsedText: '1m ago',
  });

  const chromiumTab: Tab = createTab({
    tabId: 3,
    title: 'Chromium',
    url: 'https://www.chromium.org',
    lastActiveTimeTicks: {internalValue: 200n},
    lastActiveElapsedText: '5m ago',
  });

  const gmailTab: Tab = createTab({
    tabId: 4,
    title: 'Gmail',
    url: 'https://mail.google.com',
    lastActiveTimeTicks: {internalValue: 400n},
    lastActiveElapsedText: 'just now',
  });

  const mockProfileData: ProfileData = {
    windows: [
      {
        active: true,
        isHostWindow: true,
        height: 600,
        tabs: [googleTab, youtubeTab],
      },
      {
        active: false,
        isHostWindow: false,
        height: 600,
        tabs: [chromiumTab],
      },
    ],
    recentlyClosedTabs: [],
    recentlyClosedTabGroups: [],
    recentlyClosedSplitViews: [],
    recentlyClosedSectionExpanded: false,
    tabGroups: [],
  };

  setup(() => {
    mockPageHandler = TestMock.fromClass(PageHandlerRemote);
    const {instance, remote} =
        browserProxyFactory.createForTest(mockPageHandler);
    browserProxyFactory.setInstance(instance);
    remotePage = remote;

    mockPageHandler.setResultFor(
        'getProfileData', Promise.resolve({profileData: mockProfileData}));

    delegate = new OpenTabsDelegate();
  });

  test('returns header', () => {
    assertEquals('Open Tabs', delegate.getHeader());
  });

  test('returns tabs sorted by MRU across windows', async () => {
    const items = await delegate.getItems();
    assertEquals(3, items.length);

    // Most recent tab.
    assertEquals(youtubeTab.title, items[0]!.title);
    assertEquals(2, items[0]!.description?.length);
    assertEquals('www.youtube.com', items[0]!.description?.[0]);
    assertEquals(youtubeTab.lastActiveElapsedText, items[0]!.description?.[1]);
    assertEquals(youtubeTab.url, items[0]!.prefixIcon?.urls?.[0]);
    assertEquals(youtubeTab, items[0]!.data);

    // Second most recent tab.
    assertEquals(chromiumTab.title, items[1]!.title);
    assertEquals(2, items[1]!.description?.length);
    assertEquals('www.chromium.org', items[1]!.description?.[0]);
    assertEquals(chromiumTab.lastActiveElapsedText, items[1]!.description?.[1]);
    assertEquals(chromiumTab.url, items[1]!.prefixIcon?.urls?.[0]);
    assertEquals(chromiumTab, items[1]!.data);

    // Least recent tab.
    assertEquals(googleTab.title, items[2]!.title);
    assertEquals(2, items[2]!.description?.length);
    assertEquals('www.google.com', items[2]!.description?.[0]);
    assertEquals(googleTab.lastActiveElapsedText, items[2]!.description?.[1]);
    assertEquals(googleTab.url, items[2]!.prefixIcon?.urls?.[0]);
    assertEquals(googleTab, items[2]!.data);
  });

  test('notifies client when tabs are changed', async () => {
    const client = new TestClient();
    delegate.init(client);

    // Initial fetch to populate delegate tabs
    await delegate.getItems();

    const updatedProfileData: ProfileData = {
      windows: [
        {
          active: true,
          isHostWindow: true,
          height: 600,
          tabs: [googleTab, gmailTab],
        },
      ],
      recentlyClosedTabs: [],
      recentlyClosedTabGroups: [],
      recentlyClosedSplitViews: [],
      recentlyClosedSectionExpanded: false,
      tabGroups: [],
    };

    remotePage.tabsChanged(updatedProfileData);
    await microtasksFinished();

    assertEquals(2, client.items.length);
    assertEquals(gmailTab.title, client.items[0]!.title);
    assertEquals(googleTab.title, client.items[1]!.title);
  });

  test('notifies client when a tab is updated', async () => {
    const client = new TestClient();
    delegate.init(client);

    await delegate.getItems();

    const updatedTab = createTab({
      tabId: 1,
      title: 'Google Search Updated',
      url: 'https://www.google.com/search?q=test',
      lastActiveTimeTicks: {internalValue: 100n},
      lastActiveElapsedText: 'just now',
    });

    remotePage.tabUpdated({
      inActiveWindow: true,
      inHostWindow: true,
      tab: updatedTab,
    });
    await microtasksFinished();

    assertEquals(3, client.items.length);
    assertEquals(updatedTab.title, client.items[2]!.title);
    assertEquals(
        updatedTab.lastActiveElapsedText, client.items[2]!.description?.[1]);
  });

  test('notifies client when tabs are removed', async () => {
    const client = new TestClient();
    delegate.init(client);

    await delegate.getItems();

    remotePage.tabsRemoved({
      tabIds: [youtubeTab.tabId],
      recentlyClosedTabs: [],
    });
    await microtasksFinished();

    assertEquals(2, client.items.length);
    assertEquals(chromiumTab.title, client.items[0]!.title);
    assertEquals(googleTab.title, client.items[1]!.title);
  });

  test('switches to tab when an item is clicked', async () => {
    const client = new TestClient();
    delegate.init(client);

    const items = await delegate.getItems();
    assertEquals(3, items.length);

    delegate.onItemClick(items[1]!);

    assertEquals(1, mockPageHandler.getCallCount('switchToTab'));
    const args = mockPageHandler.getArgs('switchToTab')[0];
    assertEquals(chromiumTab.tabId, args.tabId);
  });
});
