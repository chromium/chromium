// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {RecentlyClosedTab, Tab, TabAlertState, TabData, TabGroup, TabGroupColor, TabItemType, TabSearchItem} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertDeepEquals, assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createTab, sampleToken} from './tab_search_test_data.js';

suite('TabSearchItemTest', () => {
  let tabSearchItem: TabSearchItem;

  async function setupTest(data: TabData) {
    tabSearchItem = document.createElement('tab-search-item');
    tabSearchItem.data = data;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(tabSearchItem);
    await flushTasks();
  }

  async function assertTabSearchItemHighlights(
      text: string,
      fieldHighlightRanges: Array<{start: number, length: number}>|null,
      expected: string[]) {
    const data = new TabData(
        createTab({
          active: true,
          isDefaultFavicon: true,
          showIcon: true,
          title: text,
        }),
        TabItemType.OPEN_TAB, text);
    if (fieldHighlightRanges !== null) {
      data.highlightRanges = {
        'tab.title': fieldHighlightRanges,
        hostname: fieldHighlightRanges,
      };
    }
    await setupTest(data);

    assertHighlight(tabSearchItem.$['primaryText'], expected);
    assertHighlight(tabSearchItem.$['secondaryText'], expected);
  }

  function assertHighlight(node: HTMLElement, expected: string[]) {
    assertDeepEquals(
        expected,
        Array.from(node.querySelectorAll('.search-highlight-hit'))
            .map(e => e ? e.textContent : ''));
  }

  test('Highlight', async () => {
    const text = 'Make work better';
    await assertTabSearchItemHighlights(text, null, []);
    await assertTabSearchItemHighlights(
        text, [{start: 0, length: text.length}], ['Make work better']);
    await assertTabSearchItemHighlights(
        text, [{start: 0, length: 4}], ['Make']);
    await assertTabSearchItemHighlights(
        text, [{start: 0, length: 4}, {start: 10, length: 6}],
        ['Make', 'better']);
    await assertTabSearchItemHighlights(
        text, [{start: 5, length: 4}], ['work']);
  });

  test('CloseButtonPresence', async () => {
    await setupTest(new TabData(
        createTab({
          active: true,
          isDefaultFavicon: true,
          showIcon: true,
        }),
        TabItemType.OPEN_TAB, 'example'));

    let tabSearchItemCloseButton =
        tabSearchItem.shadowRoot!.querySelector('cr-icon-button');
    assertNotEquals(null, tabSearchItemCloseButton);

    await setupTest(new TabData(
        {
          tabId: 0,
          title: 'Example.com site',
          url: {url: 'https://example.com'},
          groupId: undefined,
          lastActiveTime: {internalValue: BigInt(0)},
          lastActiveElapsedText: '',
        } as RecentlyClosedTab,
        TabItemType.RECENTLY_CLOSED_TAB, 'example'));

    tabSearchItemCloseButton =
        tabSearchItem.shadowRoot!.querySelector('cr-icon-button');
    assertEquals(null, tabSearchItemCloseButton);
  });

  test('GroupDetailsPresence', async () => {
    const token = sampleToken(1n, 1n);
    const tab: Tab = createTab({
      active: true,
      isDefaultFavicon: true,
      showIcon: true,
      groupId: token,
    });

    const tabGroup: TabGroup = {
      id: token,
      color: TabGroupColor.kBlue,
      title: 'Examples',
    };

    const tabData = new TabData(tab, TabItemType.OPEN_TAB, 'example');
    tabData.tabGroup = tabGroup;
    await setupTest(tabData);

    const groupDotElement =
        tabSearchItem.shadowRoot!.querySelector('#groupDot')!;
    assertNotEquals(null, groupDotElement);
    const groupDotComputedStyle = getComputedStyle(groupDotElement!);
    assertEquals(
        groupDotComputedStyle.getPropertyValue('--tab-group-color-blue'),
        groupDotComputedStyle.getPropertyValue('--group-dot-color'));

    assertNotEquals(
        null, tabSearchItem.shadowRoot!.querySelector('#groupTitle'));
  });

  test('MediaAlertIndicatorPresence', async () => {
    const token = sampleToken(1n, 1n);
    const tab: Tab = createTab({
      active: true,
      alertStates: [TabAlertState.kMediaRecording, TabAlertState.kAudioPlaying],
      isDefaultFavicon: true,
      showIcon: true,
      groupId: token,
    });

    await setupTest(new TabData(tab, TabItemType.OPEN_TAB, 'example'));

    const recordingMediaAlert =
        tabSearchItem.shadowRoot!.querySelector<HTMLElement>('#mediaAlert');
    assertNotEquals(null, recordingMediaAlert);
    assertEquals('media-recording', recordingMediaAlert!.getAttribute('class'));
  });

});
