// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MostVisitedBrowserProxy} from 'chrome://resources/cr_components/most_visited/browser_proxy.js';
import {MAX_TILES_DEFAULT, MAX_TILES_FOR_CUSTOM_LINKS, MostVisitedElement} from 'chrome://resources/cr_components/most_visited/most_visited.js';
import type {MostVisitedPageRemote} from 'chrome://resources/cr_components/most_visited/most_visited.mojom-webui.js';
import {MostVisitedPageCallbackRouter, MostVisitedPageHandlerRemote} from 'chrome://resources/cr_components/most_visited/most_visited.mojom-webui.js';
import {TextDirection} from 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-webui.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertFocus, keydown} from './most_visited_test_support.js';

const MAX_TILES_BEFORE_SHOW_MORE = 5;

interface SetUpTestOptions {
  expandableTilesEnabled: boolean;
  maxTilesBeforeShowMore: number;
}

suite('CrComponentsMostVisitedFocusTest', () => {
  let mostVisited: MostVisitedElement;
  let callbackRouterRemote: MostVisitedPageRemote;

  function setupTest(providedOptions: Partial<SetUpTestOptions> = {}) {
    const defaultOptions = {
      expandableTilesEnabled: false,
      maxTilesBeforeShowMore: MAX_TILES_BEFORE_SHOW_MORE,
    };
    const options = {...defaultOptions, ...providedOptions};

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const handler = TestMock.fromClass(MostVisitedPageHandlerRemote);
    handler.setResultFor(
        'getMostVisitedExpandedState', Promise.resolve({isExpanded: false}));
    const callbackRouter = new MostVisitedPageCallbackRouter();
    MostVisitedBrowserProxy.setInstance(
        new MostVisitedBrowserProxy(handler, callbackRouter));
    callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

    mostVisited = new MostVisitedElement();
    if (options.expandableTilesEnabled) {
      mostVisited.setAttribute('expandable-tiles-enabled', '');
      mostVisited.setAttribute(
          'max-tiles-before-show-more',
          options.maxTilesBeforeShowMore.toString());
    }
    document.body.appendChild(mostVisited);
  }

  function queryTiles() {
    return Array.from(
        mostVisited.shadowRoot.querySelectorAll<HTMLElement>('.tile'));
  }

  async function addTiles(
      n: number, customLinksEnabled: boolean = true, visible: boolean = true,
      enterpriseShortcutsEnabled: boolean = false): Promise<void> {
    const tiles = Array(n).fill(0).map((_x, i) => {
      const char = String.fromCharCode(i + /* 'a' */ 97);
      return {
        title: char,
        titleDirection: TextDirection.LEFT_TO_RIGHT,
        url: {url: `https://${char}/`},
        source: i,
        titleSource: i,
        isQueryTile: false,
        allowUserEdit: true,
        allowUserDelete: true,
      };
    });
    callbackRouterRemote.setMostVisitedInfo({
      customLinksEnabled,
      enterpriseShortcutsEnabled,
      tiles,
      visible,
    });
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished;
  }

  setup(() => {
    return setupTest();
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

  test('down/right focuses showMore from last visible shortcut', async () => {
    setupTest({expandableTilesEnabled: true});
    await addTiles(MAX_TILES_BEFORE_SHOW_MORE + 1);
    const tile = queryTiles()[MAX_TILES_BEFORE_SHOW_MORE]!;
    for (const key of ['ArrowRight', 'ArrowDown']) {
      // Focus on the clickable link within the shortcut.
      tile.querySelector('a')!.focus();
      keydown(tile, key);
      assertFocus(mostVisited.$.showMore);
    }
  });

  test('up/left focuses last visible shortcut from showMore', async () => {
    setupTest({expandableTilesEnabled: true});
    await addTiles(MAX_TILES_BEFORE_SHOW_MORE + 1);
    const tile = queryTiles()[MAX_TILES_BEFORE_SHOW_MORE]!;
    for (const key of ['ArrowLeft', 'ArrowUp']) {
      mostVisited.$.showMore.focus();
      keydown(mostVisited.$.showMore, key);
      // The clickable link within the last visible shortcut should be focused.
      assertFocus(tile.querySelector('a')!);
    }
  });

  test('down/right focuses showLess from last shortcut', async () => {
    setupTest({expandableTilesEnabled: true});
    // Add max number of tiles so "Add shortcut" button is hidden.
    await addTiles(MAX_TILES_FOR_CUSTOM_LINKS);
    keydown(mostVisited.$.showMore, 'Enter');
    await microtasksFinished();

    const tile = queryTiles()[MAX_TILES_FOR_CUSTOM_LINKS - 1]!;
    for (const key of ['ArrowRight', 'ArrowDown']) {
      // Focus on the clickable link within the last shortcut.
      tile.querySelector('a')!.focus();
      keydown(tile, key);
      assertFocus(mostVisited.$.showLess);
    }
  });


  test('up/left focuses last shortcut from showLess', async () => {
    setupTest({expandableTilesEnabled: true});
    // Add max number of tiles so "Add shortcut" button is hidden.
    await addTiles(MAX_TILES_FOR_CUSTOM_LINKS);
    keydown(mostVisited.$.showMore, 'Enter');
    await microtasksFinished();

    const tile = queryTiles()[MAX_TILES_FOR_CUSTOM_LINKS - 1]!;
    for (const key of ['ArrowLeft', 'ArrowUp']) {
      mostVisited.$.showLess.focus();
      keydown(mostVisited.$.showLess, key);
      // The clickable link within the last shortcut should be focused.
      assertFocus(tile.querySelector('a')!);
    }
  });

  test('down/right focuses showLess from addShortcut', async () => {
    setupTest({expandableTilesEnabled: true});
    await addTiles(MAX_TILES_BEFORE_SHOW_MORE + 1);
    keydown(mostVisited.$.showMore, 'Enter');
    await microtasksFinished();

    for (const key of ['ArrowRight', 'ArrowDown']) {
      mostVisited.$.addShortcut.focus();
      keydown(mostVisited.$.addShortcut, key);
      assertFocus(mostVisited.$.showLess);
    }
  });

  test('up/left focuses addShortcut from showLess', async () => {
    setupTest({expandableTilesEnabled: true});
    await addTiles(MAX_TILES_BEFORE_SHOW_MORE + 1);
    keydown(mostVisited.$.showMore, 'Enter');
    await microtasksFinished();

    for (const key of ['ArrowLeft', 'ArrowUp']) {
      mostVisited.$.showLess.focus();
      keydown(mostVisited.$.showLess, key);
      assertFocus(mostVisited.$.addShortcut);
    }
  });

  test('down/right focuses showMore from last visible MV tile', async () => {
    setupTest({expandableTilesEnabled: true});
    await addTiles(MAX_TILES_DEFAULT, /* customLinksEnabled= */ false);
    await microtasksFinished();

    const tile = queryTiles()[MAX_TILES_BEFORE_SHOW_MORE]!;
    for (const key of ['ArrowRight', 'ArrowDown']) {
      // Focus on the clickable link within the last visible tile.
      tile.querySelector('a')!.focus();
      keydown(tile, key);
      assertFocus(mostVisited.$.showMore);
    }
  });

  test('up/left focuses last visible MV tile from showMore', async () => {
    setupTest({expandableTilesEnabled: true});
    await addTiles(MAX_TILES_DEFAULT, /* customLinksEnabled= */ false);
    await microtasksFinished();

    const tile = queryTiles()[MAX_TILES_BEFORE_SHOW_MORE]!;
    for (const key of ['ArrowLeft', 'ArrowUp']) {
      mostVisited.$.showMore.focus();
      keydown(mostVisited.$.showMore, key);
      // The clickable link within the last visible tile should be focused.
      assertFocus(tile.querySelector('a')!);
    }
  });

  test('down/right focuses showLess from last MV tile', async () => {
    setupTest({expandableTilesEnabled: true});
    await addTiles(MAX_TILES_DEFAULT, /* customLinksEnabled= */ false);
    keydown(mostVisited.$.showMore, 'Enter');
    await microtasksFinished();

    const tile = queryTiles()[MAX_TILES_DEFAULT - 1]!;
    for (const key of ['ArrowRight', 'ArrowDown']) {
      // Focus on the clickable link within the last tile.
      tile.querySelector('a')!.focus();
      keydown(tile, key);
      assertFocus(mostVisited.$.showLess);
    }
  });

  test('up/left focuses the last MV tile from showLess', async () => {
    setupTest({expandableTilesEnabled: true});
    await addTiles(MAX_TILES_DEFAULT, /* customLinksEnabled= */ false);
    keydown(mostVisited.$.showMore, 'Enter');
    await microtasksFinished();

    const tile = queryTiles()[MAX_TILES_DEFAULT - 1]!;
    for (const key of ['ArrowLeft', 'ArrowUp']) {
      mostVisited.$.showLess.focus();
      keydown(mostVisited.$.showLess, key);
      // The clickable link within the last tile should be focused.
      assertFocus(tile.querySelector('a')!);
    }
  });
});
