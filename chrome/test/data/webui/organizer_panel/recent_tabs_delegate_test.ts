// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxyFactory, PageHandlerRemote, RecentTabsDelegate} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerListSectionClient, OrganizerListSectionItem, PageRemote, ProfileData, RecentlyClosedTab} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

function createRecentlyClosedTab(overrides: Partial<RecentlyClosedTab>):
    RecentlyClosedTab {
  return Object.assign(
      {
        tabId: 0,
        groupId: null,
        splitId: null,
        title: '',
        url: '',
        lastActiveTime: {internalValue: 0n},
        lastActiveElapsedText: '',
      },
      overrides);
}

class TestClient implements OrganizerListSectionClient {
  items: Array<OrganizerListSectionItem<unknown>> = [];

  onItemsChanged(items: Array<OrganizerListSectionItem<unknown>>) {
    this.items = items;
  }
}

suite('RecentTabsDelegateTest', () => {
  let mockPageHandler: PageHandlerRemote&TestMock<PageHandlerRemote>;
  let remotePage: PageRemote;
  let delegate: RecentTabsDelegate;

  const googleTab: RecentlyClosedTab = createRecentlyClosedTab({
    tabId: 1,
    title: 'Google',
    url: 'https://www.google.com',
    lastActiveTime: {internalValue: 100n},
    lastActiveElapsedText: '10m ago',
  });

  const youtubeTab: RecentlyClosedTab = createRecentlyClosedTab({
    tabId: 2,
    title: 'YouTube',
    url: 'https://www.youtube.com',
    lastActiveTime: {internalValue: 300n},
    lastActiveElapsedText: '1m ago',
  });

  const chromiumTab: RecentlyClosedTab = createRecentlyClosedTab({
    tabId: 3,
    title: 'Chromium',
    url: 'https://www.chromium.org',
    lastActiveTime: {internalValue: 200n},
    lastActiveElapsedText: '5m ago',
  });

  const gmailTab: RecentlyClosedTab = createRecentlyClosedTab({
    tabId: 4,
    title: 'Gmail',
    url: 'https://mail.google.com',
    lastActiveTime: {internalValue: 400n},
    lastActiveElapsedText: 'just now',
  });

  const mockProfileData: ProfileData = {
    windows: [],
    recentlyClosedTabs: [googleTab, youtubeTab, chromiumTab],
    recentlyClosedTabGroups: [],
    recentlyClosedSplitViews: [],
    recentlyClosedSectionExpanded: false,
    tabGroups: [],
  };

  setup(() => {
    loadTimeData.resetForTesting({
      recentlyClosed: 'Recently Closed',
    });
    mockPageHandler = TestMock.fromClass(PageHandlerRemote);
    const {instance, remote} =
        browserProxyFactory.createForTest(mockPageHandler);
    browserProxyFactory.setInstance(instance);
    remotePage = remote;

    mockPageHandler.setResultFor(
        'getProfileData', Promise.resolve({profileData: mockProfileData}));

    delegate = new RecentTabsDelegate();
  });

  test('returns header', () => {
    assertEquals('Recently Closed', delegate.getHeader());
  });

  test('returns tabs sorted by MRU', async () => {
    const items = await delegate.getItems();
    assertEquals(3, items.length);

    // Most recent tab.
    assertEquals(youtubeTab.title, items[0]!.title);
    assertEquals(2, items[0]!.description?.length);
    assertEquals('www.youtube.com', items[0]!.description?.[0]);
    assertEquals(youtubeTab.lastActiveElapsedText, items[0]!.description?.[1]);
    assertEquals(youtubeTab.url, items[0]!.prefixIcon?.urls?.[0]);

    // Second most recent tab.
    assertEquals(chromiumTab.title, items[1]!.title);
    assertEquals(2, items[1]!.description?.length);
    assertEquals('www.chromium.org', items[1]!.description?.[0]);
    assertEquals(chromiumTab.lastActiveElapsedText, items[1]!.description?.[1]);
    assertEquals(chromiumTab.url, items[1]!.prefixIcon?.urls?.[0]);

    // Least recent tab.
    assertEquals(googleTab.title, items[2]!.title);
    assertEquals(2, items[2]!.description?.length);
    assertEquals('www.google.com', items[2]!.description?.[0]);
    assertEquals(googleTab.lastActiveElapsedText, items[2]!.description?.[1]);
    assertEquals(googleTab.url, items[2]!.prefixIcon?.urls?.[0]);
  });

  test('notifies client when tabs are changed', async () => {
    const client = new TestClient();
    delegate.init(client);

    // Initial fetch to populate delegate tabs
    await delegate.getItems();

    const updatedProfileData: ProfileData = {
      windows: [],
      recentlyClosedTabs: [googleTab, gmailTab],
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

  test('opens tab when an item is clicked', async () => {
    const client = new TestClient();
    delegate.init(client);

    const items = await delegate.getItems();
    assertEquals(3, items.length);

    delegate.onItemClick(items[1]!);

    assertEquals(1, mockPageHandler.getCallCount('openRecentlyClosedEntry'));
    const arg = mockPageHandler.getArgs('openRecentlyClosedEntry')[0];
    assertEquals(chromiumTab.tabId, arg);
  });
});
