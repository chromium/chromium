// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxyFactory, Color, isActive, isSplitTab, OpenTabsDelegate, OpenTabsItemType, PageHandlerRemote, SplitTabLayout, TabAlertState, TabGroupDotSize} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {OrganizerListSectionClient, OrganizerListSectionItem, PageRemote, ProfileData, Tab} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
        split: false,
        splitLayout: null,
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

function createProfileData(tabs: Tab[]): ProfileData {
  return {
    windows: [{
      active: true,
      isHostWindow: true,
      height: WINDOW_HEIGHT,
      tabs,
    }],
    recentlyClosedTabs: [],
    recentlyClosedTabGroups: [],
    recentlyClosedSplitViews: [],
    recentlyClosedSectionExpanded: false,
    tabGroups: [],
  };
}

class TestClient implements OrganizerListSectionClient {
  items: Array<OrganizerListSectionItem<unknown>> = [];

  onItemsChanged(items: Array<OrganizerListSectionItem<unknown>>) {
    this.items = items;
  }
}

const SPLIT_TOKEN = {
  high: 10n,
  low: 20n,
};
const SPLIT_TAB_1_ID = 10;
const SPLIT_TAB_1_TITLE = 'Docs';
const SPLIT_TAB_1_URL = 'https://docs.google.com';
const SPLIT_TAB_2_ID = 11;
const SPLIT_TAB_2_TITLE = 'Sheets';
const SPLIT_TAB_2_URL = 'https://sheets.google.com';
const WINDOW_HEIGHT = 600;
const AUDIO_ICON = 'organizer-panel:volume-up';

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

  const musicTab: Tab = createTab({
    tabId: 5,
    title: 'Music',
    url: 'https://music.youtube.com',
    lastActiveTimeTicks: {internalValue: 50n},
    alertStates: [TabAlertState.kAudioPlaying],
  });

  const mutedTab: Tab = createTab({
    tabId: 6,
    title: 'Muted video',
    url: 'https://www.netflix.com',
    lastActiveTimeTicks: {internalValue: 75n},
    alertStates: [TabAlertState.kAudioMuting],
  });

  const activeTab: Tab = createTab({
    tabId: 7,
    title: 'Active Tab',
    url: 'https://active.google.com',
    lastActiveTimeTicks: {internalValue: 500n},
    lastActiveElapsedText: 'just now',
    active: true,
  });

  const mockProfileData: ProfileData = {
    windows: [
      {
        active: true,
        isHostWindow: true,
        height: WINDOW_HEIGHT,
        tabs: [googleTab, youtubeTab],
      },
      {
        active: false,
        isHostWindow: false,
        height: WINDOW_HEIGHT,
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
    loadTimeData.resetForTesting({
      openTabs: 'Open Tabs',
      closeTab: 'Close tab',
    });
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
    assertDeepEquals([youtubeTab.title], items[0]!.title);
    assertEquals(2, items[0]!.description?.length);
    assertDeepEquals(
        {text: 'www.youtube.com', elideFromStart: true},
        items[0]!.description?.[0]);
    assertDeepEquals(
        {text: youtubeTab.lastActiveElapsedText}, items[0]!.description?.[1]);
    assertEquals(youtubeTab.url, items[0]!.prefixIcon?.url);
    assertEquals('cr:close', items[0]!.hoveredActionButton?.icon);
    assertEquals('Close tab', items[0]!.hoveredActionButton?.ariaLabel);
    assertDeepEquals(
        {type: OpenTabsItemType.TAB, tab: youtubeTab}, items[0]!.data);

    // Second most recent tab.
    assertDeepEquals([chromiumTab.title], items[1]!.title);
    assertEquals(2, items[1]!.description?.length);
    assertDeepEquals(
        {text: 'www.chromium.org', elideFromStart: true},
        items[1]!.description?.[0]);
    assertDeepEquals(
        {text: chromiumTab.lastActiveElapsedText}, items[1]!.description?.[1]);
    assertEquals(chromiumTab.url, items[1]!.prefixIcon?.url);
    assertDeepEquals(
        {type: OpenTabsItemType.TAB, tab: chromiumTab}, items[1]!.data);

    // Least recent tab.
    assertDeepEquals([googleTab.title], items[2]!.title);
    assertEquals(2, items[2]!.description?.length);
    assertDeepEquals(
        {text: 'www.google.com', elideFromStart: true},
        items[2]!.description?.[0]);
    assertDeepEquals(
        {text: googleTab.lastActiveElapsedText}, items[2]!.description?.[1]);
    assertEquals(googleTab.url, items[2]!.prefixIcon?.url);
    assertDeepEquals(
        {type: OpenTabsItemType.TAB, tab: googleTab}, items[2]!.data);
  });

  test('sorts tabs with audio alerts first with an audio icon', async () => {
    mockPageHandler.setResultFor('getProfileData', Promise.resolve({
      profileData:
          createProfileData([googleTab, musicTab, youtubeTab, mutedTab]),
    }));

    const items = await delegate.getItems();
    assertEquals(4, items.length);

    // Audio tabs come first, ordered by MRU within the audio group.
    assertDeepEquals([mutedTab.title], items[0]!.title);
    assertEquals(AUDIO_ICON, items[0]!.trailingIcon);
    assertDeepEquals([musicTab.title], items[1]!.title);
    assertEquals(AUDIO_ICON, items[1]!.trailingIcon);

    // Remaining tabs keep their MRU order and have no trailing icon.
    assertDeepEquals([youtubeTab.title], items[2]!.title);
    assertEquals(undefined, items[2]!.trailingIcon);
    assertDeepEquals([googleTab.title], items[3]!.title);
    assertEquals(undefined, items[3]!.trailingIcon);
  });

  test('shows the audio icon when one split view tab plays audio', async () => {
    const splitTab1 = createTab({
      tabId: SPLIT_TAB_1_ID,
      title: SPLIT_TAB_1_TITLE,
      url: SPLIT_TAB_1_URL,
      split: true,
      splitId: SPLIT_TOKEN,
      lastActiveTimeTicks: {internalValue: 10n},
    });
    const splitTab2 = createTab({
      tabId: SPLIT_TAB_2_ID,
      title: SPLIT_TAB_2_TITLE,
      url: SPLIT_TAB_2_URL,
      split: true,
      splitId: SPLIT_TOKEN,
      lastActiveTimeTicks: {internalValue: 20n},
      alertStates: [TabAlertState.kAudioPlaying],
    });

    mockPageHandler.setResultFor('getProfileData', Promise.resolve({
      profileData: createProfileData([googleTab, splitTab1, splitTab2]),
    }));

    const items = await delegate.getItems();
    assertEquals(2, items.length);

    assertTrue(isSplitTab(items[0]!.data!));
    assertEquals(AUDIO_ICON, items[0]!.trailingIcon);
    assertDeepEquals([googleTab.title], items[1]!.title);
    assertEquals(undefined, items[1]!.trailingIcon);
  });

  test('sorts currently active tab to the end of the list', async () => {
    mockPageHandler.setResultFor('getProfileData', Promise.resolve({
      profileData:
          createProfileData([googleTab, activeTab, youtubeTab, chromiumTab]),
    }));

    const items = await delegate.getItems();
    assertEquals(4, items.length);

    // Other tabs are sorted by MRU recency.
    assertDeepEquals([youtubeTab.title], items[0]!.title);
    assertDeepEquals([chromiumTab.title], items[1]!.title);
    assertDeepEquals([googleTab.title], items[2]!.title);

    // The currently active tab is at the end despite being the most recent.
    assertDeepEquals([activeTab.title], items[3]!.title);
    assertDeepEquals(
        {type: OpenTabsItemType.TAB, tab: activeTab}, items[3]!.data);
    assertTrue(isActive(items[3]!.data!));
  });

  test('sorts active split view tab to the end of the list', async () => {
    const splitTab1 = createTab({
      tabId: SPLIT_TAB_1_ID,
      title: SPLIT_TAB_1_TITLE,
      url: SPLIT_TAB_1_URL,
      split: true,
      splitId: SPLIT_TOKEN,
      lastActiveTimeTicks: {internalValue: 500n},
      active: true,
    });
    const splitTab2 = createTab({
      tabId: SPLIT_TAB_2_ID,
      title: SPLIT_TAB_2_TITLE,
      url: SPLIT_TAB_2_URL,
      split: true,
      splitId: SPLIT_TOKEN,
      lastActiveTimeTicks: {internalValue: 450n},
    });

    mockPageHandler.setResultFor('getProfileData', Promise.resolve({
      profileData:
          createProfileData([googleTab, youtubeTab, splitTab1, splitTab2]),
    }));

    const items = await delegate.getItems();
    assertEquals(3, items.length);

    assertDeepEquals([youtubeTab.title], items[0]!.title);
    assertDeepEquals([googleTab.title], items[1]!.title);
    assertDeepEquals([SPLIT_TAB_1_TITLE, SPLIT_TAB_2_TITLE], items[2]!.title);
    assertTrue(isSplitTab(items[2]!.data!));
    assertTrue(isActive(items[2]!.data));
  });

  test(
      'sorts active tab to the end even when it has an audio alert',
      async () => {
        const activeAudioTab = createTab({
          tabId: 8,
          title: 'Active Audio Tab',
          url: 'https://active.youtube.com',
          lastActiveTimeTicks: {internalValue: 600n},
          active: true,
          alertStates: [TabAlertState.kAudioPlaying],
        });

        mockPageHandler.setResultFor('getProfileData', Promise.resolve({
          profileData: createProfileData(
              [googleTab, musicTab, youtubeTab, activeAudioTab]),
        }));

        const items = await delegate.getItems();
        assertEquals(4, items.length);

        // Non-active audio tab comes first.
        assertDeepEquals([musicTab.title], items[0]!.title);
        assertEquals(AUDIO_ICON, items[0]!.trailingIcon);

        // Remaining non-active tabs in MRU order.
        assertDeepEquals([youtubeTab.title], items[1]!.title);
        assertDeepEquals([googleTab.title], items[2]!.title);

        // Active tab is at the end even with audio.
        assertDeepEquals([activeAudioTab.title], items[3]!.title);
        assertEquals(AUDIO_ICON, items[3]!.trailingIcon);
        assertTrue(isActive(items[3]!.data!));
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
    assertDeepEquals([gmailTab.title], client.items[0]!.title);
    assertDeepEquals([googleTab.title], client.items[1]!.title);
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
    assertDeepEquals([updatedTab.title], client.items[2]!.title);
    assertDeepEquals(
        {text: updatedTab.lastActiveElapsedText},
        client.items[2]!.description?.[1]);
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
    assertDeepEquals([chromiumTab.title], client.items[0]!.title);
    assertDeepEquals([googleTab.title], client.items[1]!.title);
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

  test('closes tab when action button is clicked', async () => {
    const client = new TestClient();
    delegate.init(client);

    const items = await delegate.getItems();
    assertEquals(3, items.length);

    delegate.onItemActionButtonClicked(items[1]!);

    assertEquals(1, mockPageHandler.getCallCount('closeTab'));
    const args = mockPageHandler.getArgs('closeTab')[0];
    assertEquals(chromiumTab.tabId, args);
  });

  test(
      'returns split view tabs with stacked favicons icon and orientation',
      async () => {
        const splitTab1Hostname = 'docs.google.com';
        const splitTab1ElapsedText = '3m ago';
        const splitTab2Hostname = 'sheets.google.com';
        const splitTab2ElapsedText = '7m ago';

        const splitTab1 = createTab({
          tabId: SPLIT_TAB_1_ID,
          title: SPLIT_TAB_1_TITLE,
          url: SPLIT_TAB_1_URL,
          split: true,
          splitId: SPLIT_TOKEN,
          splitLayout: SplitTabLayout.kSideBySide,
          lastActiveTimeTicks: {internalValue: 250n},
          lastActiveElapsedText: splitTab1ElapsedText,
        });
        const splitTab2 = createTab({
          tabId: SPLIT_TAB_2_ID,
          title: SPLIT_TAB_2_TITLE,
          url: SPLIT_TAB_2_URL,
          split: true,
          splitId: SPLIT_TOKEN,
          splitLayout: SplitTabLayout.kSideBySide,
          lastActiveTimeTicks: {internalValue: 150n},
          lastActiveElapsedText: splitTab2ElapsedText,
        });

        const splitProfileData: ProfileData =
            createProfileData([splitTab1, splitTab2]);

        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData: splitProfileData}));

        const items = await delegate.getItems();
        assertEquals(1, items.length);

        const item = items[0]!;
        assertDeepEquals([SPLIT_TAB_1_TITLE, SPLIT_TAB_2_TITLE], item.title);
        assertEquals(3, item.description?.length);
        assertDeepEquals(
            {text: splitTab1Hostname, elideFromStart: true},
            item.description?.[0]);
        assertDeepEquals(
            {text: splitTab2Hostname, elideFromStart: true},
            item.description?.[1]);
        assertDeepEquals({text: splitTab1ElapsedText}, item.description?.[2]);
        assertEquals(2, item.prefixIcon?.stackedFavicons?.urls?.length);
        assertEquals(
            SPLIT_TAB_1_URL, item.prefixIcon?.stackedFavicons?.urls?.[0]);
        assertEquals(
            SPLIT_TAB_2_URL, item.prefixIcon?.stackedFavicons?.urls?.[1]);
        assertFalse(!!item.prefixIcon?.stackedFavicons?.stackVertically);

        assertTrue(isSplitTab(item.data!));
        assertDeepEquals(
            {
              type: OpenTabsItemType.SPLIT_TAB,
              tabs: [splitTab1, splitTab2],
            },
            item.data);
      });

  test(
      'passes vertical stacking orientation for stacked split view tabs',
      async () => {
        const splitTab1 = createTab({
          tabId: SPLIT_TAB_1_ID,
          title: SPLIT_TAB_1_TITLE,
          url: SPLIT_TAB_1_URL,
          split: true,
          splitId: SPLIT_TOKEN,
          splitLayout: SplitTabLayout.kStacked,
        });
        const splitTab2 = createTab({
          tabId: SPLIT_TAB_2_ID,
          title: SPLIT_TAB_2_TITLE,
          url: SPLIT_TAB_2_URL,
          split: true,
          splitId: SPLIT_TOKEN,
          splitLayout: SplitTabLayout.kStacked,
        });

        const splitProfileData: ProfileData =
            createProfileData([splitTab1, splitTab2]);

        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData: splitProfileData}));

        const items = await delegate.getItems();
        assertEquals(1, items.length);

        assertTrue(!!items[0]!.prefixIcon?.stackedFavicons?.stackVertically);
      });

  test('switches to first tab when split view item is clicked', async () => {
    const splitTab1 = createTab({
      tabId: SPLIT_TAB_1_ID,
      title: SPLIT_TAB_1_TITLE,
      url: SPLIT_TAB_1_URL,
      split: true,
      splitId: SPLIT_TOKEN,
    });
    const splitTab2 = createTab({
      tabId: SPLIT_TAB_2_ID,
      title: SPLIT_TAB_2_TITLE,
      url: SPLIT_TAB_2_URL,
      split: true,
      splitId: SPLIT_TOKEN,
    });

    const splitProfileData: ProfileData =
        createProfileData([splitTab1, splitTab2]);

    mockPageHandler.setResultFor(
        'getProfileData', Promise.resolve({profileData: splitProfileData}));

    const items = await delegate.getItems();
    assertEquals(1, items.length);

    delegate.onItemClick(items[0]!);

    assertEquals(1, mockPageHandler.getCallCount('switchToTab'));
    const args = mockPageHandler.getArgs('switchToTab')[0];
    assertEquals(SPLIT_TAB_1_ID, args.tabId);
  });

  test(
      'closes both tabs when split view action button is clicked', async () => {
        const splitTab1 = createTab({
          tabId: SPLIT_TAB_1_ID,
          title: SPLIT_TAB_1_TITLE,
          url: SPLIT_TAB_1_URL,
          split: true,
          splitId: SPLIT_TOKEN,
        });
        const splitTab2 = createTab({
          tabId: SPLIT_TAB_2_ID,
          title: SPLIT_TAB_2_TITLE,
          url: SPLIT_TAB_2_URL,
          split: true,
          splitId: SPLIT_TOKEN,
        });

        const splitProfileData: ProfileData =
            createProfileData([splitTab1, splitTab2]);

        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData: splitProfileData}));

        const items = await delegate.getItems();
        assertEquals(1, items.length);
        assertEquals('cr:close', items[0]!.hoveredActionButton?.icon);
        assertEquals('Close tab', items[0]!.hoveredActionButton?.ariaLabel);

        delegate.onItemActionButtonClicked(items[0]!);

        assertEquals(1, mockPageHandler.getCallCount('closeTabs'));
        const args = mockPageHandler.getArgs('closeTabs')[0];
        assertDeepEquals([SPLIT_TAB_1_ID, SPLIT_TAB_2_ID], args);
      });

  test(
      'includes tab group dot and title as the last description part',
      async () => {
        const groupId = {high: 1n, low: 2n};
        const groupedTab = createTab({
          tabId: 20,
          groupId,
          title: 'Grouped Tab',
          url: 'https://www.example.com',
          lastActiveTimeTicks: {internalValue: 500n},
          lastActiveElapsedText: '2m ago',
        });

        const profileData: ProfileData = {
          ...createProfileData([groupedTab]),
          tabGroups: [{
            id: groupId,
            color: Color.kBlue,
            title: 'Work Group',
          }],
        };
        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData}));

        const items = await delegate.getItems();
        assertEquals(1, items.length);

        const description = items[0]!.description;
        assertEquals(3, description?.length);
        assertDeepEquals(
            {text: 'www.example.com', elideFromStart: true}, description?.[0]);
        assertDeepEquals({text: '2m ago'}, description?.[1]);
        assertEquals('Work Group', description?.[2]!.text);

        const container = document.createElement('div');
        document.body.appendChild(container);
        render(description![2]!.prefixElement!, container);
        const dot = container.querySelector('tab-group-dot');
        assertTrue(!!dot);
        assertEquals(Color.kBlue, dot.color);
        assertTrue(dot.filled);
        assertEquals(TabGroupDotSize.SMALL, dot.size);
      });

  test(
      'includes tab group dot and title for grouped split view tabs',
      async () => {
        const groupId = {high: 3n, low: 4n};
        const splitTab1 = createTab({
          tabId: SPLIT_TAB_1_ID,
          groupId,
          title: SPLIT_TAB_1_TITLE,
          url: SPLIT_TAB_1_URL,
          split: true,
          splitId: SPLIT_TOKEN,
          lastActiveTimeTicks: {internalValue: 250n},
          lastActiveElapsedText: '3m ago',
        });
        const splitTab2 = createTab({
          tabId: SPLIT_TAB_2_ID,
          groupId,
          title: SPLIT_TAB_2_TITLE,
          url: SPLIT_TAB_2_URL,
          split: true,
          splitId: SPLIT_TOKEN,
          lastActiveTimeTicks: {internalValue: 150n},
          lastActiveElapsedText: '7m ago',
        });

        const profileData: ProfileData = {
          ...createProfileData([splitTab1, splitTab2]),
          tabGroups: [{
            id: groupId,
            color: Color.kGreen,
            title: 'Split Group',
          }],
        };
        mockPageHandler.setResultFor(
            'getProfileData', Promise.resolve({profileData}));

        const items = await delegate.getItems();
        assertEquals(1, items.length);

        const description = items[0]!.description;
        assertEquals(4, description?.length);
        assertEquals('Split Group', description?.[3]!.text);

        const container = document.createElement('div');
        document.body.appendChild(container);
        render(description![3]!.prefixElement!, container);
        const dot = container.querySelector('tab-group-dot');
        assertTrue(!!dot);
        assertEquals(Color.kGreen, dot.color);
        assertTrue(dot.filled);
        assertEquals(TabGroupDotSize.SMALL, dot.size);
      });
});
