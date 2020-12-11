// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TabData, TabSearchItem} from 'chrome://tab-search/tab_search.js';

import {assertDeepEquals} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

suite('TabSearchItemTest', () => {
  /**
   * @param {string} text
   * @param {?Array<{start:number, length:number}>} highlightRanges
   * @param {!Array<string>} expected
   */
  async function assertTabSearchItemHighlights(
      text, highlightRanges, expected) {
    const tabSearchItem = /** @type {!TabSearchItem} */ (
        document.createElement('tab-search-item'));
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
    tabSearchItem.data = data;
    document.body.innerHTML = '';
    document.body.appendChild(tabSearchItem);
    await flushTasks();
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

  test('highlight', async () => {
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
});
