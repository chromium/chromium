// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Tab, TabData, TabItemType, TabSearchItem} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertDeepEquals, assertEquals, assertNotEquals} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

suite('TabSearchItemTest', () => {
  /** @type {!TabSearchItem} */
  let tabSearchItem;

  /** @param {!TabData} data */
  async function setupTest(data) {
    tabSearchItem = /** @type {!TabSearchItem} */ (
        document.createElement('tab-search-item'));
    tabSearchItem.data = data;
    document.body.innerHTML = '';
    document.body.appendChild(tabSearchItem);
    await flushTasks();
  }

  /**
   * @param {string} text
   * @param {?Array<{start:number, length:number}>} highlightRanges
   * @param {!Array<string>} expected
   */
  async function assertTabSearchItemHighlights(
      text, highlightRanges, expected) {
    const data = /** @type {!TabData} */ ({
      titleHighlightRanges: highlightRanges,
      hostname: text,
      hostnameHighlightRanges: highlightRanges,
      tab: {
        active: true,
        index: 0,
        isDefaultFavicon: true,
        lastActiveTimeTicks: {internalValue: BigInt(0)},
        pinned: false,
        showIcon: true,
        tabId: 0,
        url: 'https://example.com',
        title: text,
      }
    });
    await setupTest(data);

    assertHighlight(
        /** @type {!HTMLElement} */ (tabSearchItem.$['primaryText']), expected);
    assertHighlight(
        /** @type {!HTMLElement} */ (tabSearchItem.$['secondaryText']),
        expected);
  }

  /**
   * @param {!HTMLElement} node
   * @param {!Array<string>} expected
   */
  function assertHighlight(node, expected) {
    assertDeepEquals(
        expected,
        [].slice.call(node.querySelectorAll('.search-highlight-hit'))
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
    const tab = /** @type {!Tab} */ ({
      active: true,
      index: 0,
      isDefaultFavicon: true,
      lastActiveTimeTicks: {internalValue: BigInt(0)},
      pinned: false,
      showIcon: true,
      tabId: 0,
      url: 'https://example.com',
      title: 'Example.com site',
    });

    await setupTest(/** @type {!TabData} */ (
        {hostname: 'example', tab, type: TabItemType.OPEN}));

    let tabSearchItemCloseButton = /** @type {!HTMLElement} */ (
        tabSearchItem.shadowRoot.querySelector('cr-icon-button'));
    assertNotEquals(null, tabSearchItemCloseButton);

    await setupTest(/** @type {!TabData} */ (
        {hostname: 'example', tab, type: TabItemType.RECENTLY_CLOSED}));

    tabSearchItemCloseButton = /** @type {!HTMLElement} */ (
        tabSearchItem.shadowRoot.querySelector('cr-icon-button'));
    assertEquals(null, tabSearchItemCloseButton);
  });
});
