// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../mojo_webui_test_support.js';

import {MostVisitedBrowserProxy} from 'chrome://resources/cr_components/most_visited/browser_proxy.js';
import {MostVisitedElement} from 'chrome://resources/cr_components/most_visited/most_visited.js';
import {MostVisitedPageCallbackRouter, MostVisitedPageHandlerRemote} from 'chrome://resources/cr_components/most_visited/most_visited.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {TextDirection} from 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-webui.js';

import {TestBrowserProxy} from '../test_browser_proxy.js';
import {eventToPromise} from '../test_util.js';

import {$$, assertFocus, keydown} from './most_visited_test_support.js';

suite('CrComponentsMostVisitedFocusTest', () => {
  /** @type {!MostVisitedElement} */
  let mostVisited;

  /** @extends {TestBrowserProxy} */
  let callbackRouterRemote;

  /** @return {!Array<!Element>} */
  function queryTiles() {
    return Array.from(mostVisited.shadowRoot.querySelectorAll('.tile'));
  }

  /**
   * @param {number} n
   * @return {!Promise<void>}
   */
  async function addTiles(n) {
    const tiles = Array(n).fill(0).map((x, i) => {
      const char = String.fromCharCode(i + /* 'a' */ 97);
      return {
        title: char,
        titleDirection: TextDirection.LEFT_TO_RIGHT,
        url: {url: `https://${char}/`},
        source: i,
        titleSource: i,
      };
    });
    const tilesRendered = eventToPromise('dom-change', mostVisited.$.tiles);
    callbackRouterRemote.setMostVisitedInfo({
      customLinksEnabled: true,
      tiles: tiles,
      visible: true,
    });
    await callbackRouterRemote.$.flushForTesting();
    await tilesRendered;
  }

  setup(/** @suppress {checkTypes} */ () => {
    document.innerHTML = '';

    const handler = TestBrowserProxy.fromClass(MostVisitedPageHandlerRemote);
    const callbackRouter = new MostVisitedPageCallbackRouter();
    MostVisitedBrowserProxy.setInstance(
        new MostVisitedBrowserProxy(handler, callbackRouter));
    callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

    mostVisited = new MostVisitedElement();
    document.body.appendChild(mostVisited);
  });

  test('right focuses on addShortcut', async () => {
    await addTiles(1);
    const [tile] = queryTiles();
    tile.focus();
    keydown(tile, 'ArrowRight');
    assertFocus($$(mostVisited, '#addShortcut'));
  });

  test('right focuses on addShortcut when menu button focused', async () => {
    await addTiles(1);
    const [tile] = queryTiles();
    tile.querySelector('cr-icon-button').focus();
    keydown(tile, 'ArrowRight');
    assertFocus($$(mostVisited, '#addShortcut'));
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

  test('down focuses on addShortcut', async () => {
    await addTiles(1);
    const [tile] = queryTiles();
    tile.focus();
    keydown(tile, 'ArrowDown');
    assertFocus($$(mostVisited, '#addShortcut'));
  });

  test('down focuses next tile', async () => {
    await addTiles(2);
    const [first, second] = queryTiles();
    first.focus();
    keydown(first, 'ArrowDown');
    assertFocus(second);
  });

  test('up focuses on previous tile from addShortcut', async () => {
    await addTiles(1);
    $$(mostVisited, '#addShortcut').focus();
    keydown($$(mostVisited, '#addShortcut'), 'ArrowUp');
    assertFocus(queryTiles()[0]);
  });

  test('up focuses on previous tile', async () => {
    await addTiles(2);
    const [first, second] = queryTiles();
    second.focus();
    keydown(second, 'ArrowUp');
    assertFocus(first);
  });

  test('up/left does not change focus when on first tile', async () => {
    await addTiles(1);
    const [tile] = queryTiles();
    tile.focus();
    keydown(tile, 'ArrowUp');
    assertFocus(tile);
    keydown(tile, 'ArrowLeft');
  });

  test('up/left/right/down addShortcut and no tiles', async () => {
    await addTiles(0);
    $$(mostVisited, '#addShortcut').focus();
    for (const key of ['ArrowUp', 'ArrowLeft', 'ArrowRight', 'ArrowDown']) {
      keydown($$(mostVisited, '#addShortcut'), key);
      assertFocus($$(mostVisited, '#addShortcut'));
    }
  });
});
