// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome://new-tab-page-third-party/new_tab_page_third_party.js';
import {assertFocus, createTestProxy, keydown} from 'chrome://test/new_tab_page_third_party/test_support.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

suite('NewTabPageThirdPartyMostVisitedFocusTest', () => {
  /** @type {!MostVisitedElement} */
  let mostVisited;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  /**
   * @param {string}
   * @return {!Array<!HTMLElement>}
   * @private
   */
  function queryAll(q) {
    return Array.from(mostVisited.shadowRoot.querySelectorAll(q));
  }

  /**
   * @return {!Array<!HTMLElement>}
   * @private
   */
  function queryTiles() {
    return queryAll('.tile');
  }

  /**
   * @param {number} n
   * @return {!Promise}
   * @private
   */
  async function addTiles(n) {
    const tiles = Array(n).fill(0).map((x, i) => {
      const char = String.fromCharCode(i + /* 'a' */ 97);
      return {
        title: char,
        titleDirection: mojoBase.mojom.TextDirection.LEFT_TO_RIGHT,
        url: {url: `https://${char}/`},
        source: i,
        titleSource: i,
        dataGenerationTime: {internalValue: 0},
      };
    });
    const tilesRendered = eventToPromise('dom-change', mostVisited.$.tiles);
    testProxy.callbackRouterRemote.setMostVisitedTiles(tiles);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await tilesRendered;
  }

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    testProxy.setResultMapperFor('matchMedia', () => ({
                                                 addListener() {},
                                                 removeListener() {},
                                               }));
    BrowserProxy.instance_ = testProxy;

    mostVisited = document.createElement('ntp3p-most-visited');
    document.body.appendChild(mostVisited);
  });

  test('right focuses next tile', async () => {
    await addTiles(2);
    const [first, second] = queryTiles();
    first.focus();
    keydown(first, 'ArrowRight');
    assertFocus(second);
  });

  test('right focuses on next tile when menu button focused', async () => {
    await addTiles(2);
    const [first, second] = queryTiles();
    first.querySelector('cr-icon-button').focus();
    keydown(first, 'ArrowRight');
    assertFocus(second);
  });

  test('down focuses next tile', async () => {
    await addTiles(2);
    const [first, second] = queryTiles();
    first.focus();
    keydown(first, 'ArrowDown');
    assertFocus(second);
  });

  test('up focuses on previous tile', async () => {
    await addTiles(2);
    const [first, second] = queryTiles();
    second.focus();
    keydown(second, 'ArrowUp');
    assertFocus(first);
  });

  test('up/left/right/down one tiles', async () => {
    await addTiles(1);
    const [tile] = queryTiles();
    tile.focus();
    ['ArrowUp', 'ArrowLeft', 'ArrowRight', 'ArrowDown'].forEach(key => {
      keydown(tile, key);
      assertFocus(tile);
    });
  });
});
