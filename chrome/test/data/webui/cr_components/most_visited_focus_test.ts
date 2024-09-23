// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MostVisitedBrowserProxy} from 'chrome://resources/cr_components/most_visited/browser_proxy.js';
import {MostVisitedElement} from 'chrome://resources/cr_components/most_visited/most_visited.js';
import type {MostVisitedPageRemote} from 'chrome://resources/cr_components/most_visited/most_visited.mojom-webui.js';
import {MostVisitedPageCallbackRouter, MostVisitedPageHandlerRemote} from 'chrome://resources/cr_components/most_visited/most_visited.mojom-webui.js';
import {TextDirection} from 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-webui.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertFocus, keydown} from './most_visited_test_support.js';

suite('CrComponentsMostVisitedFocusTest', () => {
  let mostVisited: MostVisitedElement;
  let callbackRouterRemote: MostVisitedPageRemote;

  function queryTiles() {
    return Array.from(
        mostVisited.shadowRoot!.querySelectorAll<HTMLElement>('.tile'));
  }

  async function addTiles(n: number): Promise<void> {
    const tiles = Array(n).fill(0).map((_x, i) => {
      const char = String.fromCharCode(i + /* 'a' */ 97);
      return {
        title: char,
        titleDirection: TextDirection.LEFT_TO_RIGHT,
        url: {url: `https://${char}/`},
        source: i,
        titleSource: i,
        isQueryTile: false,
      };
    });
    callbackRouterRemote.setMostVisitedInfo({
      customLinksEnabled: true,
      tiles: tiles,
      visible: true,
    });
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished;
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const handler = TestMock.fromClass(MostVisitedPageHandlerRemote);
    const callbackRouter = new MostVisitedPageCallbackRouter();
    MostVisitedBrowserProxy.setInstance(
        new MostVisitedBrowserProxy(handler, callbackRouter));
    callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

    mostVisited = new MostVisitedElement();
    document.body.appendChild(mostVisited);
  });

  test('right focuses on addShortcut', async () => {
    await addTiles(1);
    const tile = queryTiles()[0]!;
    tile.querySelector('a')!.focus();
    keydown(tile, 'ArrowRight');
    assertFocus(mostVisited.$.addShortcut);
  });

  test('right focuses on addShortcut when menu button focused', async () => {
    await addTiles(1);
    const tile = queryTiles()[0]!;
    tile.querySelector('cr-icon-button')!.focus();
    keydown(tile, 'ArrowRight');
    assertFocus(mostVisited.$.addShortcut);
  });

  test('right focuses next tile', async () => {
    await addTiles(2);
    const tiles = queryTiles();
    tiles[0]!.querySelector('a')!.focus();
    keydown(tiles[0]!, 'ArrowRight');
    assertFocus(tiles[1]!.querySelector('a')!);
  });

  test('right focuses on next tile when menu button focused', async () => {
    await addTiles(2);
    const tiles = queryTiles();
    tiles[0]!.querySelector('cr-icon-button')!.focus();
    keydown(tiles[0]!, 'ArrowRight');
    assertFocus(tiles[1]!.querySelector('a')!);
  });

  test('down focuses on addShortcut', async () => {
    await addTiles(1);
    const tile = queryTiles()[0]!;
    tile.querySelector('a')!.focus();
    keydown(tile, 'ArrowDown');
    assertFocus(mostVisited.$.addShortcut);
  });

  test('down focuses next tile', async () => {
    await addTiles(2);
    const tiles = queryTiles();
    tiles[0]!.querySelector('a')!.focus();
    keydown(tiles[0]!, 'ArrowDown');
    assertFocus(tiles[1]!.querySelector('a')!);
  });

  test('up focuses on previous tile from addShortcut', async () => {
    await addTiles(1);
    mostVisited.$.addShortcut.focus();
    keydown(mostVisited.$.addShortcut, 'ArrowUp');
    assertFocus(queryTiles()[0]!.querySelector('a')!);
  });

  test('up focuses on previous tile', async () => {
    await addTiles(2);
    const tiles = queryTiles();
    tiles[1]!.querySelector('a')!.focus();
    keydown(tiles[1]!, 'ArrowUp');
    assertFocus(tiles[0]!.querySelector('a')!);
  });

  test('up/left does not change focus when on first tile', async () => {
    await addTiles(1);
    const tile = queryTiles()[0]!;
    tile.querySelector('a')!.focus();
    keydown(tile, 'ArrowUp');
    assertFocus(tile.querySelector('a')!);
    keydown(tile, 'ArrowLeft');
  });

  test('up/left/right/down addShortcut and no tiles', async () => {
    await addTiles(0);
    mostVisited.$.addShortcut.focus();
    for (const key of ['ArrowUp', 'ArrowLeft', 'ArrowRight', 'ArrowDown']) {
      keydown(mostVisited.$.addShortcut, key);
      assertFocus(mostVisited.$.addShortcut);
    }
  });
});
