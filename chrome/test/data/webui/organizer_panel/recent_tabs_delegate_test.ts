// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxyFactory, PageHandlerRemote, RecentTabsDelegate} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerListSectionClient, OrganizerListSectionItem, PageRemote, ProfileData, RecentlyClosedTab, RecentlyClosedTabGroup} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
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

function createRecentlyClosedTabGroup(
    overrides: Partial<RecentlyClosedTabGroup>): RecentlyClosedTabGroup {
  const defaultTabGroup: RecentlyClosedTabGroup = {
    sessionId: 0,
    id: {high: 0n, low: 0n},
    color: 0,
    title: '',
    tabCount: 1,
    lastActiveTime: {internalValue: 0n},
    lastActiveElapsedText: '',
  };
  return {...defaultTabGroup, ...overrides};
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
      oneTab: '1 tab',
      tabCount: '$1 tabs',
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

  test('returns tabs and tab groups sorted by MRU', async () => {
    const tabGroup: RecentlyClosedTabGroup = createRecentlyClosedTabGroup({
      sessionId: 10,
      id: {high: 0n, low: 10n},
      title: 'Work Group',
      tabCount: 3,
      lastActiveTime: {internalValue: 250n},
      lastActiveElapsedText: '3m ago',
    });

    const profileDataWithGroup: ProfileData = {
      ...mockProfileData,
      recentlyClosedTabGroups: [tabGroup],
    };
    mockPageHandler.setResultFor(
        'getProfileData', Promise.resolve({profileData: profileDataWithGroup}));

    const items = await delegate.getItems();
    assertEquals(4, items.length);

    // Most recent: youtubeTab (300n)
    assertEquals(youtubeTab.title, items[0]!.title);
    assertEquals(youtubeTab, items[0]!.data);

    // Second: tabGroup (250n)
    assertEquals(tabGroup.title, items[1]!.title);
    assertEquals(2, items[1]!.description?.length);
    assertEquals('3 tabs', items[1]!.description?.[0]);
    assertEquals(tabGroup.lastActiveElapsedText, items[1]!.description?.[1]);
    assertEquals(undefined, items[1]!.prefixIcon);
    assertEquals(tabGroup, items[1]!.data);

    // Third: chromiumTab (200n)
    assertEquals(chromiumTab.title, items[2]!.title);
    assertEquals(chromiumTab, items[2]!.data);

    // Fourth: googleTab (100n)
    assertEquals(googleTab.title, items[3]!.title);
    assertEquals(googleTab, items[3]!.data);
  });

  test(
      'filters out tabs belonging to a recently closed tab group', async () => {
        const groupId = {high: 1n, low: 2n};
        const tabGroup: RecentlyClosedTabGroup = createRecentlyClosedTabGroup({
          sessionId: 10,
          id: groupId,
          title: 'Grouped Tabs',
          tabCount: 2,
          lastActiveTime: {internalValue: 350n},
          lastActiveElapsedText: 'just now',
        });

        const groupedTab1: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 11,
          groupId: groupId,
          title: 'Grouped Tab 1',
          url: 'https://example.com/1',
          lastActiveTime: {internalValue: 340n},
        });

        const groupedTab2: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 12,
          groupId: groupId,
          title: 'Grouped Tab 2',
          url: 'https://example.com/2',
          lastActiveTime: {internalValue: 330n},
        });

        const standaloneTab: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 13,
          groupId: null,
          title: 'Standalone Tab',
          url: 'https://example.com/3',
          lastActiveTime: {internalValue: 300n},
        });

        const otherGroupTab: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 14,
          groupId: {high: 9n, low: 9n},
          title: 'Other Group Tab',
          url: 'https://example.com/4',
          lastActiveTime: {internalValue: 200n},
        });

        const profileData: ProfileData = {
          windows: [],
          recentlyClosedTabs:
              [groupedTab1, groupedTab2, standaloneTab, otherGroupTab],
          recentlyClosedTabGroups: [tabGroup],
          recentlyClosedSplitViews: [],
          recentlyClosedSectionExpanded: false,
          tabGroups: [],
        };
        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData}));

        const items = await delegate.getItems();
        // groupedTab1 and groupedTab2 are filtered out, leaving tabGroup,
        // standaloneTab, otherGroupTab
        assertEquals(3, items.length);
        assertEquals(tabGroup.title, items[0]!.title);
        assertEquals(standaloneTab.title, items[1]!.title);
        assertEquals(otherGroupTab.title, items[2]!.title);
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

  test('notifies client when tab groups are changed', async () => {
    const client = new TestClient();
    delegate.init(client);

    await delegate.getItems();

    const tabGroup: RecentlyClosedTabGroup = createRecentlyClosedTabGroup({
      sessionId: 50,
      id: {high: 0n, low: 50n},
      title: 'Updated Group',
      tabCount: 2,
      lastActiveTime: {internalValue: 500n},
    });

    const updatedProfileData: ProfileData = {
      windows: [],
      recentlyClosedTabs: [googleTab],
      recentlyClosedTabGroups: [tabGroup],
      recentlyClosedSplitViews: [],
      recentlyClosedSectionExpanded: false,
      tabGroups: [],
    };

    remotePage.tabsChanged(updatedProfileData);
    await microtasksFinished();

    assertEquals(2, client.items.length);
    assertEquals(tabGroup.title, client.items[0]!.title);
    assertEquals(googleTab.title, client.items[1]!.title);
  });

  test(
      'notifies client on tabsRemoved while filtering tabs in closed groups',
      async () => {
        const groupId = {high: 0n, low: 77n};
        const tabGroup: RecentlyClosedTabGroup = createRecentlyClosedTabGroup({
          sessionId: 77,
          id: groupId,
          title: 'Group 77',
          tabCount: 2,
          lastActiveTime: {internalValue: 600n},
        });

        const initialProfileData: ProfileData = {
          windows: [],
          recentlyClosedTabs: [googleTab],
          recentlyClosedTabGroups: [tabGroup],
          recentlyClosedSplitViews: [],
          recentlyClosedSectionExpanded: false,
          tabGroups: [],
        };
        mockPageHandler.setResultFor(
            'getProfileData',
            Promise.resolve({profileData: initialProfileData}));

        const client = new TestClient();
        delegate.init(client);
        const initialItems = await delegate.getItems();
        assertEquals(2, initialItems.length);

        const tabInGroup: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 88,
          groupId: groupId,
          title: 'Tab in Group',
          url: 'https://tab-in-group.com',
          lastActiveTime: {internalValue: 700n},
        });

        const newStandaloneTab: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 99,
          groupId: null,
          title: 'New Standalone',
          url: 'https://new-standalone.com',
          lastActiveTime: {internalValue: 800n},
        });

        remotePage.tabsRemoved({
          tabIds: [88, 99],
          recentlyClosedTabs: [tabInGroup, newStandaloneTab],
        });
        await microtasksFinished();

        // tabInGroup is filtered out because it belongs to Group 77
        assertEquals(3, client.items.length);
        assertEquals(newStandaloneTab.title, client.items[0]!.title);
        assertEquals(tabGroup.title, client.items[1]!.title);
        assertEquals(googleTab.title, client.items[2]!.title);
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

  test(
      'opens tab group with session id when a tab group item is clicked',
      async () => {
        const tabGroup: RecentlyClosedTabGroup = createRecentlyClosedTabGroup({
          sessionId: 42,
          id: {high: 0n, low: 42n},
          title: 'Reading List',
          tabCount: 1,
          lastActiveTime: {internalValue: 500n},
        });

        const profileData: ProfileData = {
          windows: [],
          recentlyClosedTabs: [],
          recentlyClosedTabGroups: [tabGroup],
          recentlyClosedSplitViews: [],
          recentlyClosedSectionExpanded: false,
          tabGroups: [],
        };
        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData}));

        const items = await delegate.getItems();
        assertEquals(1, items.length);
        assertEquals('1 tab', items[0]!.description?.[0]);

        delegate.onItemClick(items[0]!);

        assertEquals(
            1, mockPageHandler.getCallCount('openRecentlyClosedEntry'));
        const arg = mockPageHandler.getArgs('openRecentlyClosedEntry')[0];
        assertEquals(tabGroup.sessionId, arg);
      });
});
