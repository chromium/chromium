// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxyFactory, PageHandlerRemote, RecentTabsDelegate, SplitTabLayout} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerListSectionClient, OrganizerListSectionItem, PageRemote, ProfileData, RecentlyClosedSplitView, RecentlyClosedTab, RecentlyClosedTabGroup} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

function createRecentlyClosedSplitView(
    overrides: Partial<RecentlyClosedSplitView>): RecentlyClosedSplitView {
  const defaultSplitView: RecentlyClosedSplitView = {
    sessionId: 0,
    id: {high: 0n, low: 0n},
    tabCount: 2,
    lastActiveTime: {internalValue: 0n},
    lastActiveElapsedText: '',
    tabUrls: ['', ''],
    tabTitles: ['', ''],
    layout: SplitTabLayout.kSideBySide,
    groupId: null,
  };
  return {...defaultSplitView, ...overrides};
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
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      noRecentlyClosedTabs: 'No recently closed tabs',
      recentlyClosed: 'Recently Closed',
      oneTab: '1 tab',
      splitView: 'Split view',
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

  test('returns id and header', () => {
    assertEquals('recently-closed', delegate.getId());
    assertEquals('Recently Closed', delegate.getHeader());
  });

  test('returns tabs sorted by MRU', async () => {
    const items = await delegate.getItems();
    assertEquals(3, items.length);

    // Most recent tab.
    assertDeepEquals([youtubeTab.title], items[0]!.title);
    assertEquals(1, items[0]!.description?.length);
    assertDeepEquals(
        {text: youtubeTab.lastActiveElapsedText, matchable: false},
        items[0]!.description?.[0]);
    assertDeepEquals(['www.youtube.com'], items[0]!.extraSearchTerms);
    assertEquals(youtubeTab.url, items[0]!.prefixIcon?.url);

    // Second most recent tab.
    assertDeepEquals([chromiumTab.title], items[1]!.title);
    assertEquals(1, items[1]!.description?.length);
    assertDeepEquals(
        {text: chromiumTab.lastActiveElapsedText, matchable: false},
        items[1]!.description?.[0]);
    assertDeepEquals(['www.chromium.org'], items[1]!.extraSearchTerms);
    assertEquals(chromiumTab.url, items[1]!.prefixIcon?.url);

    // Least recent tab.
    assertDeepEquals([googleTab.title], items[2]!.title);
    assertEquals(1, items[2]!.description?.length);
    assertDeepEquals(
        {text: googleTab.lastActiveElapsedText, matchable: false},
        items[2]!.description?.[0]);
    assertDeepEquals(['www.google.com'], items[2]!.extraSearchTerms);
    assertEquals(googleTab.url, items[2]!.prefixIcon?.url);
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
    assertDeepEquals([youtubeTab.title], items[0]!.title);
    assertEquals(youtubeTab, items[0]!.data);

    // Second: tabGroup (250n)
    assertDeepEquals([tabGroup.title], items[1]!.title);
    assertEquals(1, items[1]!.description?.length);
    assertDeepEquals(
        {text: tabGroup.lastActiveElapsedText, matchable: false},
        items[1]!.description?.[0]);
    assertEquals(undefined, items[1]!.extraSearchTerms);
    assertEquals(undefined, items[1]!.prefixIcon);
    assertEquals(tabGroup, items[1]!.data);

    // Third: chromiumTab (200n)
    assertDeepEquals([chromiumTab.title], items[2]!.title);
    assertEquals(chromiumTab, items[2]!.data);

    // Fourth: googleTab (100n)
    assertDeepEquals([googleTab.title], items[3]!.title);
    assertEquals(googleTab, items[3]!.data);
  });

  test(
      'uses tab count as title when recently closed tab group has no title',
      async () => {
        const tabGroup: RecentlyClosedTabGroup = createRecentlyClosedTabGroup({
          sessionId: 10,
          id: {high: 0n, low: 10n},
          title: '',
          tabCount: 3,
          lastActiveTime: {internalValue: 250n},
          lastActiveElapsedText: '3m ago',
        });

        const profileDataWithGroup: ProfileData = {
          ...mockProfileData,
          recentlyClosedTabs: [],
          recentlyClosedTabGroups: [tabGroup],
        };
        mockPageHandler.setResultFor(
            'getProfileData',
            Promise.resolve({profileData: profileDataWithGroup}));

        const items = await delegate.getItems();
        assertEquals(1, items.length);
        assertDeepEquals(['3 tabs'], items[0]!.title);
        assertEquals(1, items[0]!.description?.length);
        assertDeepEquals(
            {text: '3m ago', matchable: false}, items[0]!.description?.[0]);
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
        assertDeepEquals([tabGroup.title], items[0]!.title);
        assertDeepEquals([standaloneTab.title], items[1]!.title);
        assertDeepEquals([otherGroupTab.title], items[2]!.title);
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
    assertDeepEquals([gmailTab.title], client.items[0]!.title);
    assertDeepEquals([googleTab.title], client.items[1]!.title);
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
    assertDeepEquals([tabGroup.title], client.items[0]!.title);
    assertDeepEquals([googleTab.title], client.items[1]!.title);
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
        assertDeepEquals([newStandaloneTab.title], client.items[0]!.title);
        assertDeepEquals([tabGroup.title], client.items[1]!.title);
        assertDeepEquals([googleTab.title], client.items[2]!.title);
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
          lastActiveElapsedText: '5m ago',
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
        assertDeepEquals(
            {text: '5m ago', matchable: false}, items[0]!.description?.[0]);

        delegate.onItemClick(items[0]!);

        assertEquals(
            1, mockPageHandler.getCallCount('openRecentlyClosedEntry'));
        const arg = mockPageHandler.getArgs('openRecentlyClosedEntry')[0];
        assertEquals(tabGroup.sessionId, arg);
      });

  test('returns zero state', () => {
    const zeroState = delegate.getZeroState();
    assertTrue(!!zeroState);

    const container = document.createElement('div');
    document.body.appendChild(container);
    render(zeroState, container);

    const zeroStateElement = container.querySelector('organizer-zero-state');
    assertTrue(!!zeroStateElement);
    assertEquals(
        'No recently closed tabs', zeroStateElement.textContent.trim());
  });

  test(
      'returns split views with stacked favicons and orientation', async () => {
        const splitView: RecentlyClosedSplitView =
            createRecentlyClosedSplitView({
              sessionId: 20,
              id: {high: 1n, low: 2n},
              layout: SplitTabLayout.kSideBySide,
              tabUrls: ['https://docs.google.com', 'https://sheets.google.com'],
              tabTitles: ['Docs', 'Sheets'],
              lastActiveTime: {internalValue: 250n},
              lastActiveElapsedText: '3m ago',
            });

        const profileData: ProfileData = {
          ...mockProfileData,
          recentlyClosedTabs: [],
          recentlyClosedSplitViews: [splitView],
        };
        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData}));

        const items = await delegate.getItems();
        assertEquals(1, items.length);

        const item = items[0]!;
        assertDeepEquals([loadTimeData.getString('splitView')], item.title);
        assertEquals(3, item.description?.length);
        assertDeepEquals(
            {text: '3m ago', matchable: false}, item.description?.[0]);
        assertDeepEquals({text: 'Docs'}, item.description?.[1]);
        assertDeepEquals({text: 'Sheets'}, item.description?.[2]);
        assertDeepEquals(
            ['docs.google.com', 'sheets.google.com'], item.extraSearchTerms);
        assertDeepEquals(
            ['https://docs.google.com', 'https://sheets.google.com'],
            item.prefixIcon?.stackedFavicons?.urls);
        assertFalse(!!item.prefixIcon?.stackedFavicons?.stackVertically);
        assertEquals(splitView, item.data);
      });

  test(
      'passes vertical stacking orientation for stacked split views',
      async () => {
        const splitView: RecentlyClosedSplitView =
            createRecentlyClosedSplitView({
              sessionId: 21,
              id: {high: 1n, low: 3n},
              layout: SplitTabLayout.kStacked,
              tabUrls: ['https://docs.google.com', 'https://sheets.google.com'],
            });

        const profileData: ProfileData = {
          ...mockProfileData,
          recentlyClosedTabs: [],
          recentlyClosedSplitViews: [splitView],
        };
        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData}));

        const items = await delegate.getItems();
        assertEquals(1, items.length);
        assertTrue(!!items[0]!.prefixIcon?.stackedFavicons?.stackVertically);
      });

  test('returns tabs, tab groups, and split views sorted by MRU', async () => {
    const tabGroup: RecentlyClosedTabGroup = createRecentlyClosedTabGroup({
      sessionId: 10,
      id: {high: 0n, low: 10n},
      title: 'Work Group',
      tabCount: 3,
      lastActiveTime: {internalValue: 250n},
      lastActiveElapsedText: '3m ago',
    });

    const splitView: RecentlyClosedSplitView = createRecentlyClosedSplitView({
      sessionId: 20,
      id: {high: 0n, low: 20n},
      tabUrls: ['https://docs.google.com', 'https://sheets.google.com'],
      lastActiveTime: {internalValue: 275n},
      lastActiveElapsedText: '2m ago',
    });

    const profileData: ProfileData = {
      ...mockProfileData,
      recentlyClosedTabGroups: [tabGroup],
      recentlyClosedSplitViews: [splitView],
    };
    mockPageHandler.setResultFor(
        'getProfileData', Promise.resolve({profileData}));

    const items = await delegate.getItems();
    assertEquals(5, items.length);
    assertEquals(youtubeTab, items[0]!.data);
    assertEquals(splitView, items[1]!.data);
    assertEquals(tabGroup, items[2]!.data);
    assertEquals(chromiumTab, items[3]!.data);
    assertEquals(googleTab, items[4]!.data);
  });

  test(
      'filters out tabs belonging to a recently closed split view',
      async () => {
        const splitId = {high: 3n, low: 4n};
        const splitView: RecentlyClosedSplitView =
            createRecentlyClosedSplitView({
              sessionId: 20,
              id: splitId,
              tabUrls: ['https://example.com/1', 'https://example.com/2'],
              lastActiveTime: {internalValue: 350n},
              lastActiveElapsedText: 'just now',
            });

        const splitTab1: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 11,
          splitId,
          title: 'Split Tab 1',
          url: 'https://example.com/1',
          lastActiveTime: {internalValue: 340n},
        });

        const splitTab2: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 12,
          splitId,
          title: 'Split Tab 2',
          url: 'https://example.com/2',
          lastActiveTime: {internalValue: 330n},
        });

        const standaloneTab: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 13,
          splitId: null,
          title: 'Standalone Tab',
          url: 'https://example.com/3',
          lastActiveTime: {internalValue: 300n},
        });

        const otherSplitTab: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 14,
          splitId: {high: 9n, low: 9n},
          title: 'Other Split Tab',
          url: 'https://example.com/4',
          lastActiveTime: {internalValue: 200n},
        });

        const profileData: ProfileData = {
          windows: [],
          recentlyClosedTabs:
              [splitTab1, splitTab2, standaloneTab, otherSplitTab],
          recentlyClosedTabGroups: [],
          recentlyClosedSplitViews: [splitView],
          recentlyClosedSectionExpanded: false,
          tabGroups: [],
        };
        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData}));

        const items = await delegate.getItems();
        assertEquals(3, items.length);
        assertDeepEquals(
            [loadTimeData.getString('splitView')], items[0]!.title);
        assertDeepEquals([standaloneTab.title], items[1]!.title);
        assertDeepEquals([otherSplitTab.title], items[2]!.title);
      });

  test('notifies client when split views are changed', async () => {
    const client = new TestClient();
    delegate.init(client);

    await delegate.getItems();

    const splitView: RecentlyClosedSplitView = createRecentlyClosedSplitView({
      sessionId: 60,
      id: {high: 0n, low: 60n},
      tabUrls: ['https://docs.google.com', 'https://sheets.google.com'],
      lastActiveTime: {internalValue: 500n},
    });

    const updatedProfileData: ProfileData = {
      windows: [],
      recentlyClosedTabs: [googleTab],
      recentlyClosedTabGroups: [],
      recentlyClosedSplitViews: [splitView],
      recentlyClosedSectionExpanded: false,
      tabGroups: [],
    };

    remotePage.tabsChanged(updatedProfileData);
    await microtasksFinished();

    assertEquals(2, client.items.length);
    assertDeepEquals(
        [loadTimeData.getString('splitView')], client.items[0]!.title);
    assertDeepEquals([googleTab.title], client.items[1]!.title);
  });

  test(
      'notifies client on tabsRemoved while filtering split view tabs',
      async () => {
        const splitId = {high: 0n, low: 88n};
        const splitView: RecentlyClosedSplitView =
            createRecentlyClosedSplitView({
              sessionId: 88,
              id: splitId,
              tabUrls: ['https://docs.google.com', 'https://sheets.google.com'],
              lastActiveTime: {internalValue: 600n},
            });

        const initialProfileData: ProfileData = {
          windows: [],
          recentlyClosedTabs: [googleTab],
          recentlyClosedTabGroups: [],
          recentlyClosedSplitViews: [splitView],
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

        const tabInSplit: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 89,
          splitId,
          title: 'Docs',
          url: 'https://docs.google.com',
          lastActiveTime: {internalValue: 700n},
        });

        const newStandaloneTab: RecentlyClosedTab = createRecentlyClosedTab({
          tabId: 90,
          splitId: null,
          title: 'New Standalone',
          url: 'https://new-standalone.com',
          lastActiveTime: {internalValue: 800n},
        });

        remotePage.tabsRemoved({
          tabIds: [89, 90],
          recentlyClosedTabs: [tabInSplit, newStandaloneTab],
        });
        await microtasksFinished();

        assertEquals(3, client.items.length);
        assertDeepEquals([newStandaloneTab.title], client.items[0]!.title);
        assertDeepEquals(
            [loadTimeData.getString('splitView')], client.items[1]!.title);
        assertDeepEquals([googleTab.title], client.items[2]!.title);
      });

  test(
      'opens split view with session id when a split view item is clicked',
      async () => {
        const splitView: RecentlyClosedSplitView =
            createRecentlyClosedSplitView({
              sessionId: 55,
              id: {high: 0n, low: 55n},
              tabUrls: ['https://docs.google.com', 'https://sheets.google.com'],
              lastActiveTime: {internalValue: 500n},
            });

        const profileData: ProfileData = {
          windows: [],
          recentlyClosedTabs: [],
          recentlyClosedTabGroups: [],
          recentlyClosedSplitViews: [splitView],
          recentlyClosedSectionExpanded: false,
          tabGroups: [],
        };
        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData}));

        const items = await delegate.getItems();
        assertEquals(1, items.length);

        delegate.onItemClick(items[0]!);

        assertEquals(
            1, mockPageHandler.getCallCount('openRecentlyClosedEntry'));
        const arg = mockPageHandler.getArgs('openRecentlyClosedEntry')[0];
        assertEquals(splitView.sessionId, arg);
      });
});
