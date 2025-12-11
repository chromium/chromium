// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TileSource} from '//resources/mojo/components/ntp_tiles/tile_source.mojom-webui.js';
import {MostVisitedBrowserProxy} from 'chrome://resources/cr_components/most_visited/browser_proxy.js';
import {MAX_TILES_FOR_CUSTOM_LINKS, MostVisitedElement} from 'chrome://resources/cr_components/most_visited/most_visited.js';
import type {MostVisitedPageRemote, MostVisitedTile} from 'chrome://resources/cr_components/most_visited/most_visited.mojom-webui.js';
import {MostVisitedPageCallbackRouter, MostVisitedPageHandlerRemote} from 'chrome://resources/cr_components/most_visited/most_visited.mojom-webui.js';
import {MostVisitedWindowProxy} from 'chrome://resources/cr_components/most_visited/window_proxy.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {TextDirection} from 'chrome://resources/mojo/mojo/public/mojom/base/text_direction.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {$$, assertStyle, keydown} from './most_visited_test_support.js';

const MAX_TILES_BEFORE_SHOW_MORE = 5;

let mostVisited: MostVisitedElement;
let windowProxy: TestMock<MostVisitedWindowProxy>&MostVisitedWindowProxy;
let handler: TestMock<MostVisitedPageHandlerRemote>&
    MostVisitedPageHandlerRemote;
let callbackRouterRemote: MostVisitedPageRemote;
const mediaListenerLists: Map<number, FakeMediaQueryList> = new Map();

function queryAll<E extends Element = Element>(q: string): E[] {
  return Array.from(mostVisited.shadowRoot.querySelectorAll<E>(q));
}

function queryTiles(): HTMLAnchorElement[] {
  return queryAll<HTMLAnchorElement>('.tile');
}

function queryHiddenTiles(): HTMLAnchorElement[] {
  return queryAll<HTMLAnchorElement>('.tile[hidden]');
}

function getShowMoreButton(): CrButtonElement|null {
  return $$<CrButtonElement>(mostVisited, '#showMore');
}

function getShowLessButton(): CrButtonElement|null {
  return $$<CrButtonElement>(mostVisited, '#showLess');
}

function assertTileLength(length: number) {
  assertEquals(length, queryTiles().length);
}

function assertHiddenTileLength(length: number) {
  assertEquals(length, queryHiddenTiles().length);
}

async function addTiles(
    n: number|MostVisitedTile[], customLinksEnabled: boolean = true,
    visible: boolean = true, enterpriseShortcutsEnabled: boolean = false) {
  const tiles = Array.isArray(n) ? n : Array(n).fill(0).map((_x, i) => {
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
  await microtasksFinished();
}

function assertAddShortcutHidden() {
  assertTrue(mostVisited.$.addShortcut.hidden);
}

function assertAddShortcutShown() {
  assertFalse(mostVisited.$.addShortcut.hidden);
}

function createBrowserProxy() {
  handler = TestMock.fromClass(MostVisitedPageHandlerRemote);
  const callbackRouter = new MostVisitedPageCallbackRouter();
  MostVisitedBrowserProxy.setInstance(
      new MostVisitedBrowserProxy(handler, callbackRouter));
  callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();

  handler.setResultFor('addMostVisitedTile', Promise.resolve({
    success: true,
  }));
  handler.setResultFor('updateMostVisitedTile', Promise.resolve({
    success: true,
  }));
  handler.setResultFor(
      'getMostVisitedExpandedState', Promise.resolve({isExpanded: false}));
}

class FakeMediaQueryList extends EventTarget implements MediaQueryList {
  matches: boolean = false;
  media: string;

  constructor(query: string) {
    super();
    this.media = query;
  }

  addListener() {}
  removeListener() {}
  onchange() {}
}

function createWindowProxy() {
  windowProxy = TestMock.fromClass(MostVisitedWindowProxy);
  windowProxy.setResultMapperFor('matchMedia', (query: string) => {
    const result = query.match(/\(min-width: (\d+)px\)/);
    assertTrue(!!result);
    const mediaListenerList = new FakeMediaQueryList(query);
    mediaListenerLists.set(parseInt(result[1]!), mediaListenerList);
    return mediaListenerList;
  });
  MostVisitedWindowProxy.setInstance(windowProxy);
}

function updateScreenWidth(isWide: boolean, isMedium: boolean) {
  mediaListenerLists.forEach(list => list.matches = false);
  const mediaListenerWideWidth =
      mediaListenerLists.get(Math.max(...mediaListenerLists.keys()));
  const mediaListenerMediumWidth = mediaListenerLists.get(560);
  assertTrue(!!mediaListenerWideWidth);
  assertTrue(!!mediaListenerMediumWidth);
  mediaListenerWideWidth.matches = isWide;
  mediaListenerMediumWidth.matches = isMedium;
  mediaListenerMediumWidth.dispatchEvent(new Event('change'));
  return microtasksFinished();
}

function wide() {
  return updateScreenWidth(true, true);
}

function medium() {
  return updateScreenWidth(false, true);
}

function narrow() {
  return updateScreenWidth(false, false);
}

function leaveUrlInput() {
  $$(mostVisited, '#dialogInputUrl').dispatchEvent(new Event('blur'));
  return microtasksFinished();
}

interface SetUpTestOptions {
  singleRow: boolean;
  reflowOnOverflow: boolean;
  expandableTilesEnabled: boolean;
  maxTilesBeforeShowMore: number;
}

function setUpTest(providedOptions: Partial<SetUpTestOptions> = {}) {
  const defaultOptions = {
    singleRow: false,
    reflowOnOverflow: false,
    expandableTilesEnabled: false,
    maxTilesBeforeShowMore: MAX_TILES_BEFORE_SHOW_MORE,
  };
  const options = {...defaultOptions, ...providedOptions};
  document.body.innerHTML = window.trustedTypes!.emptyHTML;

  createBrowserProxy();
  createWindowProxy();

  mostVisited = new MostVisitedElement();
  mostVisited.singleRow = options.singleRow;
  mostVisited.reflowOnOverflow = options.reflowOnOverflow;
  if (options.expandableTilesEnabled) {
    mostVisited.setAttribute('expandable-tiles-enabled', '');
    mostVisited.setAttribute(
        'max-tiles-before-show-more',
        options.maxTilesBeforeShowMore.toString());
  }
  document.body.appendChild(mostVisited);
  assertEquals(1, handler.getCallCount('updateMostVisitedInfo'));
  return wide();
}

suite('General', () => {
  setup(async () => {
    await setUpTest();
  });

  test('empty shows add shortcut only', async () => {
    assertAddShortcutHidden();
    await addTiles(0);
    assertEquals(0, queryTiles().length);
    assertAddShortcutShown();
  });

  test('clicking on add shortcut opens dialog', () => {
    assertFalse(mostVisited.$.dialog.open);
    mostVisited.$.addShortcut.click();
    assertTrue(mostVisited.$.dialog.open);
  });

  test('pressing enter when add shortcut has focus opens dialog', () => {
    mostVisited.$.addShortcut.focus();
    assertFalse(mostVisited.$.dialog.open);
    keydown(mostVisited.$.addShortcut, 'Enter');
    assertTrue(mostVisited.$.dialog.open);
  });

  test('pressing space when add shortcut has focus opens dialog', () => {
    mostVisited.$.addShortcut.focus();
    assertFalse(mostVisited.$.dialog.open);
    mostVisited.$.addShortcut.dispatchEvent(
        new KeyboardEvent('keydown', {key: ' '}));
    mostVisited.$.addShortcut.dispatchEvent(
        new KeyboardEvent('keyup', {key: ' '}));
    assertTrue(mostVisited.$.dialog.open);
  });
});

suite('ShowAddButton', () => {
  setup(async () => {
    await setUpTest({reflowOnOverflow: true});
  });

  test('add shortcut button shows when custom links enabled', async () => {
    await addTiles(0, /*customLinksEnabled=*/ true);
    assertAddShortcutShown();
  });

  test('add shortcut button hidden when custom links disabled', async () => {
    await addTiles(0, /*customLinksEnabled=*/ false);
    assertAddShortcutHidden();
  });

  test(
      'add shortcut button hidden when custom links disabled and max tiles',
      async () => {
        const tiles = Array(MAX_TILES_FOR_CUSTOM_LINKS).fill(0).map((_x, i) => {
          const char = String.fromCharCode(i + /* 'a' */ 97);
          return {
            title: char,
            titleDirection: TextDirection.LEFT_TO_RIGHT,
            url: {url: `https://${char}/`},
            source: i % 2 === 0 ? TileSource.TOP_SITES :
                                  TileSource.CUSTOM_LINKS,
            titleSource: i,
            isQueryTile: false,
            allowUserEdit: true,
            allowUserDelete: true,
          };
        });
        await addTiles(tiles, /*customLinksEnabled=*/ true);
        assertAddShortcutHidden();
      });

  test(
      'add shortcut button shown when custom links disabled and max enterprise tiles',
      async () => {
        const tiles = Array(MAX_TILES_FOR_CUSTOM_LINKS).fill(0).map((_x, i) => {
          const char = String.fromCharCode(i + /* 'a' */ 97);
          return {
            title: char,
            titleDirection: TextDirection.LEFT_TO_RIGHT,
            url: {url: `https://${char}/`},
            source: TileSource.ENTERPRISE_SHORTCUTS,
            titleSource: i,
            isQueryTile: false,
            allowUserEdit: true,
            allowUserDelete: true,
          };
        });
        await addTiles(
            tiles, /*customLinksEnabled=*/ true, /*visible=*/ true,
            /*enterpriseShortcutsEnabled=*/ true);
        assertAddShortcutShown();
      });
});

suite('ExpandableTiles', () => {
  test('initializes isExpanded to true from pref', async () => {
    createBrowserProxy();
    handler.setResultFor(
        'getMostVisitedExpandedState', Promise.resolve({isExpanded: true}));
    createWindowProxy();

    mostVisited = new MostVisitedElement();
    mostVisited.setAttribute('expandable-tiles-enabled', '');
    document.body.appendChild(mostVisited);
    await wide();

    await handler.whenCalled('getMostVisitedExpandedState');
    await microtasksFinished();
    await addTiles(MAX_TILES_BEFORE_SHOW_MORE + 1);
    assertTrue(mostVisited['showAll_']);
    assertTrue(isVisible(getShowLessButton()));
    assertFalse(isVisible(getShowMoreButton()));
  });

  test('Show more button is shown with 6 or more tiles', async () => {
    await setUpTest({reflowOnOverflow: true, expandableTilesEnabled: true});
    await addTiles(MAX_TILES_BEFORE_SHOW_MORE + 1);
    assertTrue(isVisible(getShowMoreButton()));
    assertAddShortcutHidden();
    assertHiddenTileLength(0);
  });

  test(
      'Show more and show less buttons are hidden with 5 or fewer tiles',
      async () => {
        await setUpTest({reflowOnOverflow: true, expandableTilesEnabled: true});
        await addTiles(MAX_TILES_BEFORE_SHOW_MORE);
        assertFalse(isVisible(getShowMoreButton()));
        assertFalse(isVisible(getShowLessButton()));
        assertAddShortcutShown();
      });

  test(
      'When the number of tiles is 6, toggle between show more and show less',
      async () => {
        await setUpTest({reflowOnOverflow: true, expandableTilesEnabled: true});
        await addTiles(MAX_TILES_BEFORE_SHOW_MORE + 1);
        const showMoreButton = getShowMoreButton();
        assertTrue(isVisible(showMoreButton));
        assertAddShortcutHidden();
        assertHiddenTileLength(0);

        // Click "Show more", expect "Show less" and "Add shortcut" to be shown.
        showMoreButton!.click();
        const isExpanded =
            await handler.whenCalled('setMostVisitedExpandedState');
        assertTrue(isExpanded);
        handler.resetResolver('setMostVisitedExpandedState');
        await microtasksFinished();

        assertFalse(isVisible(getShowMoreButton()));
        assertTrue(isVisible(getShowLessButton()));
        assertAddShortcutShown();
        assertHiddenTileLength(0);

        // Click "Show less", "Show more" shown and "Add shortcut" hidden
        const ShowLessButton = getShowLessButton();
        ShowLessButton!.click();
        const isExpanded2 =
            await handler.whenCalled('setMostVisitedExpandedState');
        assertFalse(isExpanded2);
        await microtasksFinished();

        assertTrue(isVisible(getShowMoreButton()));
        assertFalse(isVisible(getShowLessButton()));
        assertAddShortcutHidden();
        assertHiddenTileLength(0);
      });

  test('clicking show more shows all tiles and show less button', async () => {
    await setUpTest({reflowOnOverflow: true, expandableTilesEnabled: true});
    await addTiles(MAX_TILES_BEFORE_SHOW_MORE + 2);  // 7 tiles.
    const showMoreButton = getShowMoreButton();
    assertTrue(isVisible(showMoreButton));
    assertAddShortcutHidden();
    assertHiddenTileLength(1);  // 7 tiles, 6 visible, so 1 hidden.

    showMoreButton!.click();
    await microtasksFinished();

    assertFalse(isVisible(getShowMoreButton()));
    assertTrue(isVisible(getShowLessButton()));
    assertAddShortcutShown();
    assertHiddenTileLength(0);
  });

  test('clicking show less hides tiles and show more button', async () => {
    await setUpTest({reflowOnOverflow: true, expandableTilesEnabled: true});
    await addTiles(MAX_TILES_BEFORE_SHOW_MORE + 2);  // 7 tiles.
    const showMoreButton = getShowMoreButton();
    const showLessButton = getShowLessButton();

    assertTrue(isVisible(showMoreButton));
    assertFalse(isVisible(showLessButton));
    assertAddShortcutHidden();
    assertHiddenTileLength(1);

    // Click "Show more".
    showMoreButton!.click();
    await microtasksFinished();

    assertFalse(isVisible(showMoreButton));
    assertTrue(isVisible(showLessButton));
    assertAddShortcutShown();
    assertHiddenTileLength(0);

    // Click "Show less".
    showLessButton!.click();
    await microtasksFinished();

    assertTrue(isVisible(showMoreButton));
    assertFalse(isVisible(showLessButton));
    assertAddShortcutHidden();
    assertHiddenTileLength(1);
  });

  test('clicking show less with max tiles correctly collapses UI', async () => {
    // This test reproduces a bug where having max tiles (10) would cause the
    // "Show less" button to appear on a new row, and clicking it would fail
    // to collapse the layout correctly, leaving a blank second row.
    await setUpTest({reflowOnOverflow: true, expandableTilesEnabled: true});
    await addTiles(MAX_TILES_FOR_CUSTOM_LINKS);  // 10 tiles.

    const showMoreButton = getShowMoreButton();
    assertTrue(isVisible(showMoreButton));

    // Click "Show more" to expand.
    showMoreButton!.click();
    await microtasksFinished();

    // Verify the expanded layout. With 10 tiles and a "Show less" button,
    // we expect 3 rows on a wide screen (5 columns).
    const showLessButton = getShowLessButton();
    assertTrue(isVisible(showLessButton));
    assertHiddenTileLength(0);
    const expandedItems =
        queryAll<HTMLElement>('.tile:not([hidden]), #showLess');
    assertEquals(MAX_TILES_FOR_CUSTOM_LINKS + 1, expandedItems.length);
    const firstRowTop = expandedItems[0]!.offsetTop;
    const secondRowTop = expandedItems[5]!.offsetTop;
    const thirdRowTop = expandedItems[10]!.offsetTop;
    assertNotEquals(firstRowTop, secondRowTop);
    assertNotEquals(secondRowTop, thirdRowTop);
    const expandedHeight = mostVisited.$.container.offsetHeight;

    // Click "Show less" to collapse.
    showLessButton!.click();
    await microtasksFinished();

    // Verify the collapsed layout. We should have 2 rows with 6 tiles and
    // the "Show more" button.
    assertTrue(isVisible(getShowMoreButton()));
    // There are 10 tiles total. When collapsed, 6 are visible. 4 are hidden.
    assertHiddenTileLength(4);
    const collapsedItems =
        queryAll<HTMLElement>('.tile:not([hidden]), #showMore');
    assertEquals(MAX_TILES_BEFORE_SHOW_MORE + 1 + 1, collapsedItems.length);
    const collapsedHeight = mostVisited.$.container.offsetHeight;
    assertNotEquals(
        expandedHeight, collapsedHeight,
        'Collapsed layout should be shorter than expanded layout');
  });

  test(
      'show less button on first row with 8 tiles and custom links disabled',
      async () => {
        await setUpTest({
          singleRow: true,
          reflowOnOverflow: true,
          expandableTilesEnabled: true,
        });
        await addTiles(8, /*customLinksEnabled=*/ false);

        const showMoreButton = getShowMoreButton();
        assertTrue(isVisible(showMoreButton));

        // Click "Show more" to expand.
        showMoreButton!.click();
        await microtasksFinished();

        // We expect 1 row with 8 tiles and a "Show less" button.
        const showLessButton = getShowLessButton();
        assertTrue(isVisible(showLessButton));
        assertHiddenTileLength(0);
        const expandedItems =
            queryAll<HTMLElement>('.tile:not([hidden]), #showLess');
        assertEquals(8 + 1, expandedItems.length);
        const firstItemTop = expandedItems[0]!.offsetTop;
        for (const item of expandedItems) {
          assertEquals(firstItemTop, item.offsetTop);
        }
        assertEquals(1, rowCount());
      });

  test(
      'show more and show less buttons do not move during drag and drop',
      async () => {
        await setUpTest({reflowOnOverflow: true, expandableTilesEnabled: true});
        await addTiles(MAX_TILES_BEFORE_SHOW_MORE + 2);  // 7 tiles.

        const showMoreButton = getShowMoreButton()!;
        assertTrue(isVisible(showMoreButton));
        const showMoreButtonRect = showMoreButton.getBoundingClientRect();

        // Simulate drag and drop.
        const tiles = queryTiles();
        const first = tiles[0]!;
        const second = tiles[1]!;
        const firstRect = first.getBoundingClientRect();
        const secondRect = second.getBoundingClientRect();
        first.dispatchEvent(new DragEvent('dragstart', {
          clientX: firstRect.x + firstRect.width / 2,
          clientY: firstRect.y + firstRect.height / 2,
        }));
        await microtasksFinished();

        // Check position of "Show more" button immediately after drag starts.
        let newShowMoreButtonRect = showMoreButton.getBoundingClientRect();
        assertDeepEquals(showMoreButtonRect, newShowMoreButtonRect);

        const reorderCalled = handler.whenCalled('reorderMostVisitedTile');
        document.dispatchEvent(new DragEvent('drop', {
          clientX: secondRect.x + 1,
          clientY: secondRect.y + 1,
        }));
        document.dispatchEvent(new DragEvent('dragend', {
          clientX: secondRect.x + 1,
          clientY: secondRect.y + 1,
        }));
        await mostVisited.updateComplete;
        await reorderCalled;

        // Check position of "Show more" button.
        newShowMoreButtonRect = showMoreButton.getBoundingClientRect();
        assertDeepEquals(showMoreButtonRect, newShowMoreButtonRect);

        // Expand to show "Show less" button.
        showMoreButton.click();
        await microtasksFinished();

        const showLessButton = getShowLessButton()!;
        assertTrue(isVisible(showLessButton));
        const showLessButtonRect = showLessButton.getBoundingClientRect();

        // Simulate another drag and drop.
        const newTiles = queryTiles();
        const newFirst = newTiles[0]!;
        const newSecond = newTiles[1]!;
        const newFirstRect = newFirst.getBoundingClientRect();
        const newSecondRect = newSecond.getBoundingClientRect();
        newFirst.dispatchEvent(new DragEvent('dragstart', {
          clientX: newFirstRect.x + newFirstRect.width / 2,
          clientY: newFirstRect.y + newFirstRect.height / 2,
        }));
        await microtasksFinished();

        // Check position of "Show less" button immediately after drag starts.
        let newShowLessButtonRect = showLessButton.getBoundingClientRect();
        assertDeepEquals(showLessButtonRect, newShowLessButtonRect);

        const reorderCalledAgain = handler.whenCalled('reorderMostVisitedTile');
        document.dispatchEvent(new DragEvent('drop', {
          clientX: newSecondRect.x + 1,
          clientY: newSecondRect.y + 1,
        }));
        document.dispatchEvent(new DragEvent('dragend', {
          clientX: newSecondRect.x + 1,
          clientY: newSecondRect.y + 1,
        }));
        await mostVisited.updateComplete;
        await reorderCalledAgain;

        // Check position of "Show less" button.
        newShowLessButtonRect = showLessButton.getBoundingClientRect();
        assertDeepEquals(showLessButtonRect, newShowLessButtonRect);
      });
});

function createLayoutsSuite(singleRow: boolean, reflowOnOverflow: boolean) {
  setup(async () => {
    await setUpTest({singleRow, reflowOnOverflow});
  });

  test('four tiles fit on one line with addShortcut', async () => {
    await addTiles(4);
    assertEquals(4, queryTiles().length);
    assertAddShortcutShown();
    const tops = queryAll<HTMLElement>('.tile, #addShortcut')
                     .map(({offsetTop}) => offsetTop);
    assertEquals(5, tops.length);
    tops.forEach(top => {
      assertEquals(tops[0], top);
    });
  });

  test('five tiles are displayed with addShortcut', async () => {
    await addTiles(5);
    assertEquals(5, queryTiles().length);
    assertAddShortcutShown();
    const tops = queryAll<HTMLElement>('.tile, #addShortcut')
                     .map(({offsetTop}) => offsetTop);
    assertEquals(6, tops.length);
    const firstRowTop = tops[0];
    const secondRowTop = tops[3];
    if (singleRow) {
      assertEquals(firstRowTop, secondRowTop);
    } else {
      assertNotEquals(firstRowTop, secondRowTop);
    }
    tops.slice(0, 3).forEach(top => {
      assertEquals(firstRowTop, top);
    });
    tops.slice(3).forEach(top => {
      assertEquals(secondRowTop, top);
    });
  });

  test('nine tiles are displayed with addShortcut', async () => {
    await addTiles(9);
    assertEquals(9, queryTiles().length);
    assertAddShortcutShown();
    const tops = queryAll<HTMLElement>('.tile, #addShortcut')
                     .map(({offsetTop}) => offsetTop);
    assertEquals(10, tops.length);
    const firstRowTop = tops[0];
    const secondRowTop = tops[5];
    if (singleRow) {
      assertEquals(firstRowTop, secondRowTop);
    } else {
      assertNotEquals(firstRowTop, secondRowTop);
    }
    tops.slice(0, 5).forEach(top => {
      assertEquals(firstRowTop, top);
    });
    tops.slice(5).forEach(top => {
      assertEquals(secondRowTop, top);
    });
  });

  test('ten tiles are displayed without addShortcut', async () => {
    await addTiles(10);
    assertEquals(10, queryTiles().length);
    assertAddShortcutHidden();
    const tops =
        queryAll<HTMLElement>('.tile:not([hidden])').map(a => a.offsetTop);
    assertEquals(10, tops.length);
    const firstRowTop = tops[0];
    const secondRowTop = tops[5];
    if (singleRow) {
      assertEquals(firstRowTop, secondRowTop);
    } else {
      assertNotEquals(firstRowTop, secondRowTop);
    }
    tops.slice(0, 5).forEach(top => {
      assertEquals(firstRowTop, top);
    });
    tops.slice(5).forEach(top => {
      assertEquals(secondRowTop, top);
    });
  });

  test('ten tiles is the max tiles displayed', async () => {
    await addTiles(11);
    assertEquals(10, queryTiles().length);
    assertAddShortcutHidden();
  });

  test('eight tiles is the max (customLinksEnabled=false)', async () => {
    await addTiles(11, /* customLinksEnabled */ true);
    assertEquals(10, queryTiles().length);
    assertEquals(0, queryAll('.tile[hidden]').length);
    assertAddShortcutHidden();
    await addTiles(11, /* customLinksEnabled */ false);
    assertEquals(8, queryTiles().length);
    assertEquals(0, queryAll('.tile[hidden]').length);
    assertAddShortcutHidden();
    await addTiles(11, /* customLinksEnabled */ true);
    assertEquals(10, queryTiles().length);
    assertEquals(0, queryAll('.tile[hidden]').length);
  });

  test('7 tiles and no add shortcut (customLinksEnabled=false)', async () => {
    await addTiles(7, /* customLinksEnabled */ true);
    assertAddShortcutShown();
    await addTiles(7, /* customLinksEnabled */ false);
    assertAddShortcutHidden();
    await addTiles(7, /* customLinksEnabled */ true);
    assertAddShortcutShown();
  });

  test('no tiles shown when (visible=false)', async () => {
    await addTiles(1);
    assertEquals(1, queryTiles().length);
    assertEquals(0, queryAll('.tile[hidden]').length);
    assertTrue(mostVisited.hasAttribute('visible_'));
    assertFalse(mostVisited.$.container.hidden);
    await addTiles(1, /* customLinksEnabled */ true, /* visible */ false);
    assertEquals(1, queryTiles().length);
    assertEquals(0, queryAll('.tile[hidden]').length);
    assertFalse(mostVisited.hasAttribute('visible_'));
    assertTrue(mostVisited.$.container.hidden);
    await addTiles(1, /* customLinksEnabled */ true, /* visible */ true);
    assertEquals(1, queryTiles().length);
    assertEquals(0, queryAll('.tile[hidden]').length);
    assertTrue(mostVisited.hasAttribute('visible_'));
    assertFalse(mostVisited.$.container.hidden);
  });
}

function createLayoutsWidthsSuite(singleRow: boolean) {
  suite('test various widths', () => {
    setup(async () => {
      await setUpTest({singleRow});
    });

    test('six / three is max for narrow', async () => {
      await addTiles(7);
      await medium();
      assertTileLength(7);
      assertHiddenTileLength(singleRow ? 3 : 0);
      await narrow();
      assertTileLength(7);
      assertHiddenTileLength(singleRow ? 4 : 1);
      await medium();
      assertTileLength(7);
      assertHiddenTileLength(singleRow ? 3 : 0);
    });

    test('eight / four is max for medium', async () => {
      await addTiles(8);
      await narrow();
      assertTileLength(8);
      assertHiddenTileLength(singleRow ? 5 : 2);
      await medium();
      assertTileLength(8);
      assertHiddenTileLength(singleRow ? 4 : 0);
      await narrow();
      assertTileLength(8);
      assertHiddenTileLength(singleRow ? 5 : 2);
    });

    test('eight is max for wide', async () => {
      await addTiles(8);
      await narrow();
      assertTileLength(8);
      assertHiddenTileLength(singleRow ? 5 : 2);
      await wide();
      assertTileLength(8);
      assertHiddenTileLength(0);
      await narrow();
      assertTileLength(8);
      assertHiddenTileLength(singleRow ? 5 : 2);
    });

    test('hide add shortcut (narrow)', async () => {
      await addTiles(6);
      await medium();
      if (singleRow) {
        assertAddShortcutHidden();
      } else {
        assertAddShortcutShown();
      }
      await narrow();
      assertAddShortcutHidden();
      await medium();
      if (singleRow) {
        assertAddShortcutHidden();
      } else {
        assertAddShortcutShown();
      }
    });

    test('hide add shortcut with 8 tiles (medium)', async () => {
      await addTiles(8);
      await wide();
      assertAddShortcutShown();
      await medium();
      assertAddShortcutHidden();
      await wide();
      assertAddShortcutShown();
    });

    test('hide add shortcut with 9 tiles (medium)', async () => {
      await addTiles(9);
      await wide();
      assertAddShortcutShown();
      await addTiles(10);
      assertAddShortcutHidden();
    });

    if (singleRow) {
      test('shows correct number of tiles for all widths', async () => {
        await addTiles(12);
        mediaListenerLists.forEach(list => list.matches = false);
        [...mediaListenerLists.keys()]
            .sort((a, b) => a - b)
            .forEach(async (width, i) => {
              const list = mediaListenerLists.get(width)!;
              list.matches = true;
              list.dispatchEvent(new Event('change'));
              await microtasksFinished();
              assertHiddenTileLength(6 - i);
            });
      });
    }
  });
}

function rowCount(): number {
  return Number(getComputedStyle(mostVisited.$.container)
                    .getPropertyValue('--row-count'));
}

function columnCount(): number {
  return Number(getComputedStyle(mostVisited.$.container)
                    .getPropertyValue('--column-count'));
}

function createLayoutsWidthsReflowSuite(singleRow: boolean) {
  suite('test reflow on various widths', () => {
    setup(async () => {
      await setUpTest({singleRow, reflowOnOverflow: true});
    });

    test('No hidden tiles', async () => {
      await addTiles(7);
      await updateScreenWidth(false, true);
      assertTileLength(7);
      assertHiddenTileLength(0);
      await updateScreenWidth(false, false);
      assertTileLength(7);
      assertHiddenTileLength(0);
      assertAddShortcutShown();
    });

    test(
        'Eight tiles + shortcut reflow to 3c x 3r in narrow layout',
        async () => {
          await narrow();
          await addTiles(8);
          assertAddShortcutShown();
          assertEquals(columnCount(), 3);
          assertEquals(rowCount(), 3);
        });

    test(
        'Eight tiles + shortcut reflow to 4c x 3r in medium layout',
        async () => {
          await medium();
          await addTiles(8);
          assertAddShortcutShown();
          assertEquals(columnCount(), 4);
          assertEquals(rowCount(), 3);
        });

    test('Eight tiles + shortcut reflow in wide layout', async () => {
      await wide();
      await addTiles(8);
      assertAddShortcutShown();
      assertEquals(columnCount(), singleRow ? 9 : 5);
      assertEquals(rowCount(), singleRow ? 1 : 2);
    });
  });
}

suite('Layouts', () => {
  suite('double row', () => {
    createLayoutsSuite(/*singleRow=*/ false, /*reflowOnOverflow=*/ false);
    createLayoutsWidthsSuite(/*singleRow=*/ false);
  });

  suite('single row', () => {
    createLayoutsSuite(/*singleRow=*/ true, /*reflowOnOverflow=*/ false);
    createLayoutsWidthsSuite(/*singleRow=*/ true);
  });
});

suite('Reflow Layouts', () => {
  suite('double row', () => {
    createLayoutsSuite(/*singleRow=*/ false, /*reflowOnOverflow=*/ true);
    createLayoutsWidthsReflowSuite(/*singleRow=*/ false);
  });

  suite('single row', () => {
    createLayoutsSuite(/*singleRow=*/ false, /*reflowOnOverflow=*/ true);
    createLayoutsWidthsReflowSuite(/*singleRow=*/ true);
  });
});

suite('LoggingAndUpdates', () => {
  setup(async () => {
    await setUpTest();
  });

  test('rendering tiles logs event', async () => {
    // Clear promise resolvers created during setup.
    handler.reset();

    // Arrange.
    windowProxy.setResultFor('now', 123);

    // Act.
    await addTiles(2);

    // Assert.
    const [tiles, time] =
        await handler.whenCalled('onMostVisitedTilesRendered');
    assertEquals(time, 123);
    assertEquals(tiles.length, 2);
    assertDeepEquals(tiles[0], {
      title: 'a',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://a/'},
      source: 0,
      titleSource: 0,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    });
    assertDeepEquals(tiles[1], {
      title: 'b',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://b/'},
      source: 1,
      titleSource: 1,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    });
  });

  test('clicking tile logs event', async () => {
    // Arrange.
    await addTiles(1);

    // Act.
    const tileLink = queryTiles()[0]!.querySelector('a')!;
    // Prevent triggering a navigation, which would break the test.
    tileLink.href = '#';
    tileLink.click();

    // Assert.
    const [tile, index] =
        await handler.whenCalled('onMostVisitedTileNavigation');
    assertEquals(index, 0);
    assertDeepEquals(tile, {
      title: 'a',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://a/'},
      source: 0,
      titleSource: 0,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    });
  });

  test('making tab visible refreshes most visited tiles', () => {
    // Arrange.
    handler.resetResolver('updateMostVisitedInfo');

    // Act.
    document.dispatchEvent(new Event('visibilitychange'));

    // Assert.
    assertEquals(1, handler.getCallCount('updateMostVisitedInfo'));
  });
});

suite('Modification', () => {
  suiteSetup(() => {
    loadTimeData.overrideValues({
      invalidUrl: 'Type a valid URL',
      linkAddedMsg: 'Shortcut added',
      linkCantCreate: 'Can\'t create shortcut',
      linkEditedMsg: 'Shortcut edited',
      restoreDefaultLinks: 'Restore default shortcuts',
      shortcutAlreadyExists: 'Shortcut already exists',
    });
  });

  setup(async () => {
    await setUpTest();
  });

  suite('add dialog', () => {
    let dialog: CrDialogElement;
    let inputName: CrInputElement;
    let inputUrl: CrInputElement;
    let saveButton: CrButtonElement;
    let cancelButton: CrButtonElement;

    setup(async () => {
      dialog = mostVisited.$.dialog;
      inputName = $$<CrInputElement>(mostVisited, '#dialogInputName')!;
      inputUrl = $$<CrInputElement>(mostVisited, '#dialogInputUrl')!;
      saveButton = dialog.querySelector('.action-button')!;
      cancelButton = dialog.querySelector('.cancel-button')!;
      await microtasksFinished();

      mostVisited.$.addShortcut.click();
      assertTrue(dialog.open);
    });

    test('policy subtitle is hidden', () => {
      const policySubtitleContainer =
          mostVisited.$.dialog.querySelector<HTMLElement>(
              '#policySubtitleContainer');
      assertFalse(isVisible(policySubtitleContainer));
    });

    test('inputs are initially empty', () => {
      assertEquals('', inputName.value);
      assertEquals('', inputUrl.value);
    });

    test('saveButton is enabled with URL is not empty', async () => {
      assertTrue(saveButton.disabled);
      inputName.value = 'name';
      await inputName.updateComplete;
      assertTrue(saveButton.disabled);
      inputUrl.value = 'url';
      await inputUrl.updateComplete;
      assertFalse(saveButton.disabled);
      inputUrl.value = '';
      await inputUrl.updateComplete;
      assertTrue(saveButton.disabled);
      inputUrl.value = 'url';
      await inputUrl.updateComplete;
      assertFalse(saveButton.disabled);
      inputUrl.value = '                                \n\n\n        ';
      await inputUrl.updateComplete;
      assertTrue(saveButton.disabled);
    });

    test('cancel closes dialog', () => {
      assertTrue(dialog.open);
      cancelButton.click();
      assertFalse(dialog.open);
    });

    test('inputs are clear after dialog reuse', async () => {
      inputName.value = 'name';
      inputUrl.value = 'url';
      await Promise.all([inputName.updateComplete, inputUrl.updateComplete]);
      cancelButton.click();
      mostVisited.$.addShortcut.click();
      await microtasksFinished();
      assertEquals('', inputName.value);
      assertEquals('', inputUrl.value);
    });

    test('use URL input for title when title empty', async () => {
      inputUrl.value = 'url';
      const addCalled = handler.whenCalled('addMostVisitedTile');
      await inputUrl.updateComplete;
      saveButton.click();
      const [_url, title] = await addCalled;
      assertEquals('url', title);
    });

    test('toast shown on save', async () => {
      inputUrl.value = 'url';
      await inputUrl.updateComplete;
      assertFalse(mostVisited.$.toastManager.isToastOpen);
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      await addCalled;
      assertTrue(mostVisited.$.toastManager.isToastOpen);
    });

    test('toast has undo buttons when action successful', async () => {
      handler.setResultFor('addMostVisitedTile', Promise.resolve({
        success: true,
      }));
      inputUrl.value = 'url';
      await inputUrl.updateComplete;
      saveButton.click();
      await handler.whenCalled('addMostVisitedTile');
      await microtasksFinished();
      assertFalse($$<HTMLElement>(mostVisited, '#undo').hidden);
    });

    test('toast has no undo buttons when action not successful', async () => {
      handler.setResultFor('addMostVisitedTile', Promise.resolve({
        success: false,
      }));
      inputUrl.value = 'url';
      await inputUrl.updateComplete;
      saveButton.click();
      await handler.whenCalled('addMostVisitedTile');
      await microtasksFinished();
      assertFalse(isVisible($$(mostVisited, '#undo')));
    });

    test('save name and URL', async () => {
      inputName.value = 'name';
      inputUrl.value = 'https://url/';
      await Promise.all([inputName.updateComplete, inputUrl.updateComplete]);
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      const [{url}, title] = await addCalled;
      assertEquals('name', title);
      assertEquals('https://url/', url);
    });

    test('dialog closes on save', async () => {
      inputUrl.value = 'url';
      await inputUrl.updateComplete;
      assertTrue(dialog.open);
      saveButton.click();
      assertFalse(dialog.open);
    });

    test('https:// is added if no scheme is used', async () => {
      inputUrl.value = 'url';
      await inputUrl.updateComplete;
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      const [{url}, _title] = await addCalled;
      assertEquals('https://url/', url);
    });

    test('http is a valid scheme', async () => {
      assertTrue(saveButton.disabled);
      inputUrl.value = 'http://url';
      await inputUrl.updateComplete;
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      await addCalled;
      assertFalse(saveButton.disabled);
    });

    test('https is a valid scheme', async () => {
      inputUrl.value = 'https://url';
      await inputUrl.updateComplete;
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      await addCalled;
    });

    test('chrome is not a valid scheme', async () => {
      assertTrue(saveButton.disabled);
      inputUrl.value = 'chrome://url';
      await inputUrl.updateComplete;
      assertFalse(inputUrl.invalid);
      await leaveUrlInput();
      assertTrue(inputUrl.invalid);
      assertTrue(saveButton.disabled);
    });

    test('invalid cleared when text entered', async () => {
      inputUrl.value = '%';
      await inputUrl.updateComplete;
      assertFalse(inputUrl.invalid);
      await leaveUrlInput();
      assertTrue(inputUrl.invalid);
      assertEquals('Type a valid URL', inputUrl.errorMessage);
      inputUrl.value = '';
      await inputUrl.updateComplete;
      assertFalse(inputUrl.invalid);
    });

    test('shortcut already exists', async () => {
      await addTiles(2);
      inputUrl.value = 'b';
      await inputUrl.updateComplete;
      assertFalse(inputUrl.invalid);
      await leaveUrlInput();
      assertTrue(inputUrl.invalid);
      assertEquals('Shortcut already exists', inputUrl.errorMessage);
      inputUrl.value = 'c';
      await inputUrl.updateComplete;
      assertFalse(inputUrl.invalid);
      await leaveUrlInput();
      assertFalse(inputUrl.invalid);
      inputUrl.value = '%';
      await inputUrl.updateComplete;
      assertFalse(inputUrl.invalid);
      await leaveUrlInput();
      assertTrue(inputUrl.invalid);
      assertEquals('Type a valid URL', inputUrl.errorMessage);
    });
  });

  test('open edit dialog', async () => {
    await addTiles(2);
    const actionMenu = mostVisited.$.actionMenu;
    const dialog = mostVisited.$.dialog;
    assertFalse(actionMenu.open);
    queryTiles()[0]!.querySelector<HTMLElement>('#actionMenuButton')!.click();
    assertTrue(actionMenu.open);
    assertFalse(dialog.open);
    $$<HTMLElement>(mostVisited, '#actionMenuViewOrEdit').click();
    assertFalse(actionMenu.open);
    assertTrue(dialog.open);
  });

  suite('edit dialog', () => {
    let actionMenuButton: HTMLElement;
    let inputName: CrInputElement;
    let inputUrl: CrInputElement;
    let saveButton: HTMLElement;
    let tile: HTMLAnchorElement;

    setup(async () => {
      inputName = $$<CrInputElement>(mostVisited, '#dialogInputName')!;
      inputUrl = $$<CrInputElement>(mostVisited, '#dialogInputUrl')!;

      const dialog = mostVisited.$.dialog;
      saveButton = dialog.querySelector('.action-button')!;

      await addTiles(2);
      tile = queryTiles()[1]!;
      actionMenuButton = tile.querySelector<HTMLElement>('#actionMenuButton')!;
      actionMenuButton.click();
      $$<HTMLElement>(mostVisited, '#actionMenuViewOrEdit').click();
    });

    test('policy subtitle is hidden', () => {
      const policySubtitleContainer =
          mostVisited.$.dialog.querySelector<HTMLElement>(
              '#policySubtitleContainer');
      assertFalse(isVisible(policySubtitleContainer));
    });

    test('edit a tile URL', async () => {
      assertEquals('https://b/', inputUrl.value);
      const updateCalled = handler.whenCalled('updateMostVisitedTile');
      inputUrl.value = 'updated-url';
      await inputUrl.updateComplete;
      saveButton.click();
      const [_url, newUrl, _newTitle] = await updateCalled;
      assertEquals('https://updated-url/', newUrl.url);
    });

    test('toast shown when tile editted', async () => {
      inputUrl.value = 'updated-url';
      await inputUrl.updateComplete;
      assertFalse(mostVisited.$.toastManager.isToastOpen);
      saveButton.click();
      await handler.whenCalled('updateMostVisitedTile');
      assertTrue(mostVisited.$.toastManager.isToastOpen);
    });

    test('no toast when not editted', () => {
      assertFalse(mostVisited.$.toastManager.isToastOpen);
      saveButton.click();
      assertFalse(mostVisited.$.toastManager.isToastOpen);
    });

    test('edit a tile title', async () => {
      assertEquals('b', inputName.value);
      const updateCalled = handler.whenCalled('updateMostVisitedTile');
      inputName.value = 'updated name';
      await inputName.updateComplete;
      saveButton.click();
      const [_url, _newUrl, newTitle] = await updateCalled;
      assertEquals('updated name', newTitle);
    });

    test('update not called when name and URL not changed', async () => {
      // |updateMostVisitedTile| will be called only after either the title or
      // url has changed.
      const updateCalled = handler.whenCalled('updateMostVisitedTile');
      saveButton.click();
      // Reopen dialog and edit URL.
      actionMenuButton.click();
      $$<HTMLElement>(mostVisited, '#actionMenuViewOrEdit').click();
      inputUrl.value = 'updated-url';
      await inputUrl.updateComplete;
      saveButton.click();
      const [_url, newUrl, _newTitle] = await updateCalled;
      assertEquals('https://updated-url/', newUrl.url);
    });

    test('shortcut already exists', async () => {
      inputUrl.value = 'a';
      await inputUrl.updateComplete;
      assertFalse(inputUrl.invalid);
      await leaveUrlInput();
      assertTrue(inputUrl.invalid);
      assertEquals('Shortcut already exists', inputUrl.errorMessage);
      // The shortcut being editted has a URL of https://b/. Entering the same
      // URL is not an error.
      inputUrl.value = 'b';
      await inputUrl.updateComplete;
      assertFalse(inputUrl.invalid);
      await leaveUrlInput();
      assertFalse(inputUrl.invalid);
    });

    test(
        'shortcut already exists (enterprise and custom link with same url)',
        async () => {
          await addTiles(
              [
                {
                  title: 'e1',
                  titleDirection: TextDirection.LEFT_TO_RIGHT,
                  url: {url: `https://e1/`},
                  source: TileSource.ENTERPRISE_SHORTCUTS,
                  titleSource: 0,
                  isQueryTile: false,
                  allowUserEdit: true,
                  allowUserDelete: true,
                },
                {
                  title: 'c1',
                  titleDirection: TextDirection.LEFT_TO_RIGHT,
                  url: {url: `https://e1/`},
                  source: TileSource.CUSTOM_LINKS,
                  titleSource: 1,
                  isQueryTile: false,
                  allowUserEdit: true,
                  allowUserDelete: true,
                },
              ],
              /*customLinksEnabled=*/ true, /*visible=*/ true,
              /*enterpriseShortcutsEnabled=*/ true);

          // Open edit dialog for the enteprise shortcut (index 0).
          const enterpriseShortcutTile = queryTiles()[0]!;
          enterpriseShortcutTile
              .querySelector<HTMLElement>('#actionMenuButton')!.click();
          $$<HTMLElement>(mostVisited, '#actionMenuViewOrEdit').click();
          await microtasksFinished();

          // Action button should be visible and clickable.
          assertTrue(mostVisited.$.dialog.open);
          const actionButton =
              mostVisited.$.dialog.querySelector<CrButtonElement>(
                  '.action-button')!;
          assertFalse(actionButton.disabled);
          actionButton.click();
          await microtasksFinished();
          assertFalse(mostVisited.$.dialog.open);

          // Open edit dialog for the custom link (index 1).
          const customLinkTile = queryTiles()[1]!;
          customLinkTile.querySelector<HTMLElement>(
                            '#actionMenuButton')!.click();
          $$<HTMLElement>(mostVisited, '#actionMenuViewOrEdit').click();
          await microtasksFinished();

          // Try to set its URL to the enterprise shortcut's URL (which is the
          // same as its own, but we're testing the logic).
          inputUrl.value = 'https://e1/';
          await inputUrl.updateComplete;
          assertFalse(mostVisited['dialogShortcutAlreadyExists_']);
          assertFalse(inputUrl.invalid);
          await leaveUrlInput();
          assertFalse(inputUrl.invalid);

          // Save button should be visible and clickable.
          assertTrue(mostVisited.$.dialog.open);
          const saveButton =
              mostVisited.$.dialog.querySelector<CrButtonElement>(
                  '.action-button')!;
          assertFalse(saveButton.disabled);
          saveButton.click();
          await microtasksFinished();
          assertFalse(mostVisited.$.dialog.open);
        });

    test('edit custom link to duplicate enterprise shortcut url', async () => {
      await addTiles(
          [
            {
              title: 'e1',
              titleDirection: TextDirection.LEFT_TO_RIGHT,
              url: {url: `https://e1/`},
              source: TileSource.ENTERPRISE_SHORTCUTS,
              titleSource: 0,
              isQueryTile: false,
              allowUserEdit: true,
              allowUserDelete: true,
            },
            {
              title: 'c1',
              titleDirection: TextDirection.LEFT_TO_RIGHT,
              url: {url: `https://c1/`},
              source: TileSource.CUSTOM_LINKS,
              titleSource: 1,
              isQueryTile: false,
              allowUserEdit: true,
              allowUserDelete: true,
            },
          ],
          /*customLinksEnabled=*/ true, /*visible=*/ true,
          /*enterpriseShortcutsEnabled=*/ true);

      // Open edit dialog for the custom link (index 1).
      const customLinkTile = queryTiles()[1]!;
      customLinkTile.querySelector<HTMLElement>('#actionMenuButton')!.click();
      $$<HTMLElement>(mostVisited, '#actionMenuViewOrEdit').click();
      await microtasksFinished();

      // Try to set its URL to the enterprise shortcut's URL.
      // Save button should be visible and clickable.
      inputUrl.value = 'https://e1/';
      await inputUrl.updateComplete;
      assertFalse(mostVisited['dialogShortcutAlreadyExists_']);
      assertFalse(inputUrl.invalid);
      await leaveUrlInput();
      assertFalse(inputUrl.invalid);

      const saveButton = mostVisited.$.dialog.querySelector<CrButtonElement>(
          '.action-button')!;
      assertFalse(saveButton.disabled);
      const updateCalled = handler.whenCalled('updateMostVisitedTile');
      saveButton.click();
      const [_oldTile, newUrl, _newTitle] = await updateCalled;
      assertEquals('https://e1/', newUrl.url);
      assertFalse(mostVisited.$.dialog.open);
    });
  });

  test('remove with action menu', async () => {
    const actionMenu = mostVisited.$.actionMenu;
    const removeButton = $$<HTMLElement>(mostVisited, '#actionMenuRemove');
    await addTiles(2);
    const secondTile = queryTiles()[1]!;
    const actionMenuButton =
        secondTile.querySelector<HTMLElement>('#actionMenuButton')!;
    assertFalse(actionMenu.open);
    actionMenuButton.click();
    assertTrue(actionMenu.open);
    const deleteCalled = handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toastManager.isToastOpen);
    removeButton.click();
    assertFalse(actionMenu.open);
    assertEquals('https://b/', (await deleteCalled).url.url);
    assertTrue(mostVisited.$.toastManager.isToastOpen);
    // Toast buttons are visible.
    assertTrue(isVisible($$(mostVisited, '#undo')));
    assertTrue(isVisible($$(mostVisited, '#restore')));
  });

  test('remove query with action menu', async () => {
    const actionMenu = mostVisited.$.actionMenu;
    const removeButton = $$<HTMLElement>(mostVisited, '#actionMenuRemove');
    await addTiles([{
      title: 'title',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://search-url/'},
      source: 0,
      titleSource: 0,
      isQueryTile: true,
      allowUserEdit: true,
      allowUserDelete: true,
    }]);
    const actionMenuButton =
        queryTiles()[0]!.querySelector<HTMLElement>('#actionMenuButton')!;
    assertFalse(actionMenu.open);
    actionMenuButton.click();
    assertTrue(actionMenu.open);
    const deleteCalled = handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toastManager.isToastOpen);
    removeButton.click();
    assertEquals('https://search-url/', (await deleteCalled).url.url);
    assertTrue(mostVisited.$.toastManager.isToastOpen);
    // Toast buttons are visible.
    assertTrue(isVisible($$(mostVisited, '#undo')));
    assertTrue(isVisible($$(mostVisited, '#restore')));
  });

  test('remove with icon button (customLinksEnabled=false)', async () => {
    await addTiles(1, /* customLinksEnabled */ false);
    const removeButton =
        queryTiles()[0]!.querySelector<HTMLElement>('#removeButton')!;
    const deleteCalled = handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toastManager.isToastOpen);
    removeButton.click();
    assertEquals('https://a/', (await deleteCalled).url.url);
    assertTrue(mostVisited.$.toastManager.isToastOpen);
    // Toast buttons are visible.
    assertTrue(isVisible($$(mostVisited, '#undo')));
    assertTrue(isVisible($$(mostVisited, '#restore')));
  });

  test('remove query with icon button (customLinksEnabled=false)', async () => {
    await addTiles(
        [{
          title: 'title',
          titleDirection: TextDirection.LEFT_TO_RIGHT,
          url: {url: 'https://search-url/'},
          source: 0,
          titleSource: 0,
          isQueryTile: true,
          allowUserEdit: true,
          allowUserDelete: true,
        }],
        /* customLinksEnabled */ false);
    const removeButton =
        queryTiles()[0]!.querySelector<HTMLElement>('#removeButton')!;
    const deleteCalled = handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toastManager.isToastOpen);
    removeButton.click();
    assertEquals('https://search-url/', (await deleteCalled).url.url);
    assertTrue(mostVisited.$.toastManager.isToastOpen);
    // Toast buttons are not visible.
    assertFalse(isVisible($$(mostVisited, '#undo')));
    assertFalse(isVisible($$(mostVisited, '#restore')));
  });

  test('tile url is set to href of <a>', async () => {
    await addTiles(1);
    const tile = queryTiles()[0]!;
    assertEquals('https://a/', tile.querySelector('a')!.href);
  });

  test('delete first tile', async () => {
    await addTiles(1);
    const tile = queryTiles()[0]!;
    const deleteCalled = handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toastManager.isToastOpen);
    keydown(tile, 'Delete');
    assertEquals('https://a/', (await deleteCalled).url.url);
    assertTrue(mostVisited.$.toastManager.isToastOpen);
  });

  test('ctrl+z triggers undo and hides toast', async () => {
    const toastManager = mostVisited.$.toastManager;
    assertFalse(toastManager.isToastOpen);

    // Add a tile and remove it to show the toast.
    await addTiles(1);
    const tile = queryTiles()[0]!;
    keydown(tile, 'Delete');
    await handler.whenCalled('deleteMostVisitedTile');
    assertTrue(toastManager.isToastOpen);

    const undoCalled = handler.whenCalled('undoMostVisitedTileAction');
    mostVisited.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      ctrlKey: !isMac,
      key: 'z',
      metaKey: isMac,
    }));
    await undoCalled;
    assertFalse(toastManager.isToastOpen);
  });

  test('ctrl+z does nothing if toast buttons are not showing', async () => {
    const toastManager = mostVisited.$.toastManager;
    assertFalse(toastManager.isToastOpen);

    // A failed attempt at adding a shortcut to show the toast with no buttons.
    handler.setResultFor('addMostVisitedTile', Promise.resolve({
      success: false,
    }));
    mostVisited.$.addShortcut.click();
    await microtasksFinished();
    const inputUrl = $$<CrInputElement>(mostVisited, '#dialogInputUrl');
    inputUrl.value = 'url';
    await inputUrl.updateComplete;
    const saveButton =
        mostVisited.$.dialog.querySelector<HTMLElement>('.action-button')!;
    saveButton.click();
    await handler.whenCalled('addMostVisitedTile');

    assertTrue(toastManager.isToastOpen);
    mostVisited.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      ctrlKey: !isMac,
      key: 'z',
      metaKey: isMac,
    }));
    await microtasksFinished();
    assertEquals(0, handler.getCallCount('undoMostVisitedTileAction'));
    assertTrue(toastManager.isToastOpen);
  });

  test('toast restore defaults button', async () => {
    const wait = handler.whenCalled('restoreMostVisitedDefaults');
    const toastManager = mostVisited.$.toastManager;
    assertFalse(toastManager.isToastOpen);

    // Add a tile and remove it to show the toast.
    await addTiles(1);
    const tile = queryTiles()[0]!;
    keydown(tile, 'Delete');
    await handler.whenCalled('deleteMostVisitedTile');

    assertTrue(toastManager.isToastOpen);
    mostVisited.$.toastManager.querySelector<HTMLElement>('#restore')!.click();
    await wait;
    assertFalse(toastManager.isToastOpen);
  });

  test('toast undo button', async () => {
    const wait = handler.whenCalled('undoMostVisitedTileAction');
    const toastManager = mostVisited.$.toastManager;
    assertFalse(toastManager.isToastOpen);

    // Add a tile and remove it to show the toast.
    await addTiles(1);
    const tile = queryTiles()[0]!;
    keydown(tile, 'Delete');
    await handler.whenCalled('deleteMostVisitedTile');

    assertTrue(toastManager.isToastOpen);
    mostVisited.$.toastManager.querySelector<HTMLElement>('#undo')!.click();
    await wait;
    assertFalse(toastManager.isToastOpen);
  });
});


function createDragAndDropSuite(singleRow: boolean, reflowOnOverflow: boolean) {
  setup(async () => {
    await setUpTest({singleRow, reflowOnOverflow});
  });

  test('drag first tile to second position', async () => {
    await addTiles(2);
    const tiles = queryTiles();
    const first = tiles[0]!;
    const second = tiles[1]!;
    assertEquals('https://a/', first.querySelector('a')!.href);
    assertTrue(first.draggable);
    assertEquals('https://b/', second.querySelector('a')!.href);
    assertTrue(second.draggable);
    const firstRect = first.getBoundingClientRect();
    const secondRect = second.getBoundingClientRect();
    first.dispatchEvent(new DragEvent('dragstart', {
      clientX: firstRect.x + firstRect.width / 2,
      clientY: firstRect.y + firstRect.height / 2,
    }));
    const reorderCalled = handler.whenCalled('reorderMostVisitedTile');
    document.dispatchEvent(new DragEvent('drop', {
      clientX: secondRect.x + 1,
      clientY: secondRect.y + 1,
    }));
    document.dispatchEvent(new DragEvent('dragend', {
      clientX: secondRect.x + 1,
      clientY: secondRect.y + 1,
    }));
    await mostVisited.updateComplete;
    const [tile, newPos] = await reorderCalled;
    assertEquals('https://a/', tile.url.url);
    assertEquals(1, newPos);
    const [newFirst, newSecond] = queryTiles();
    assertEquals('https://b/', newFirst!.querySelector('a')!.href);
    assertEquals('https://a/', newSecond!.querySelector('a')!.href);
  });

  test('drag second tile to first position', async () => {
    await addTiles(2);
    const tiles = queryTiles();
    const first = tiles[0]!;
    const second = tiles[1]!;
    assertEquals('https://a/', first.querySelector('a')!.href);
    assertTrue(first.draggable);
    assertEquals('https://b/', second.querySelector('a')!.href);
    assertTrue(second.draggable);
    const firstRect = first.getBoundingClientRect();
    const secondRect = second.getBoundingClientRect();
    second.dispatchEvent(new DragEvent('dragstart', {
      clientX: secondRect.x + secondRect.width / 2,
      clientY: secondRect.y + secondRect.height / 2,
    }));
    const reorderCalled = handler.whenCalled('reorderMostVisitedTile');
    document.dispatchEvent(new DragEvent('drop', {
      clientX: firstRect.x + 1,
      clientY: firstRect.y + 1,
    }));
    document.dispatchEvent(new DragEvent('dragend', {
      clientX: firstRect.x + 1,
      clientY: firstRect.y + 1,
    }));
    await mostVisited.updateComplete;
    const [tile, newPos] = await reorderCalled;
    assertEquals('https://b/', tile.url.url);
    assertEquals(0, newPos);
    const [newFirst, newSecond] = queryTiles();
    assertEquals('https://b/', newFirst!.querySelector('a')!.href);
    assertEquals('https://a/', newSecond!.querySelector('a')!.href);
  });

  test('most visited tiles cannot be reordered', async () => {
    await addTiles(2, /* customLinksEnabled= */ false);
    const tiles = queryTiles();
    const first = tiles[0]!;
    const second = tiles[1]!;
    assertEquals('https://a/', first.querySelector('a')!.href);
    assertTrue(first.draggable);
    assertEquals('https://b/', second.querySelector('a')!.href);
    assertTrue(second.draggable);
    const firstRect = first.getBoundingClientRect();
    const secondRect = second.getBoundingClientRect();
    first.dispatchEvent(new DragEvent('dragstart', {
      clientX: firstRect.x + firstRect.width / 2,
      clientY: firstRect.y + firstRect.height / 2,
    }));
    document.dispatchEvent(new DragEvent('drop', {
      clientX: secondRect.x + 1,
      clientY: secondRect.y + 1,
    }));
    document.dispatchEvent(new DragEvent('dragend', {
      clientX: secondRect.x + 1,
      clientY: secondRect.y + 1,
    }));
    await mostVisited.updateComplete;
    assertEquals(0, handler.getCallCount('reorderMostVisitedTile'));
    const [newFirst, newSecond] = queryTiles();
    assertEquals('https://a/', newFirst!.querySelector('a')!.href);
    assertEquals('https://b/', newSecond!.querySelector('a')!.href);
  });

  test('new index is adjusted by enterprise shortcuts', async () => {
    const enterpriseShortcut = {
      title: 'e1',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: `https://e1/`},
      source: TileSource.ENTERPRISE_SHORTCUTS,
      titleSource: 0,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    };
    const customLink1 = {
      title: 'c1',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: `https://c1/`},
      source: TileSource.CUSTOM_LINKS,
      titleSource: 1,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    };
    const customLink2 = {
      title: 'c2',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: `https://c2/`},
      source: TileSource.CUSTOM_LINKS,
      titleSource: 2,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    };
    await addTiles(
        [enterpriseShortcut, customLink1, customLink2],
        /*customLinksEnabled=*/ true, /*visible=*/ true,
        /*enterpriseShortcutsEnabled=*/ true);

    const tiles = queryTiles();
    const customLink1Element = tiles[1]!;
    const customLink2Element = tiles[2]!;

    const customLink1Rect = customLink1Element.getBoundingClientRect();
    const customLink2Rect = customLink2Element.getBoundingClientRect();

    // Drag customLink1 (index 1) to customLink2's position (index 2).
    customLink1Element.dispatchEvent(new DragEvent('dragstart', {
      clientX: customLink1Rect.x + customLink1Rect.width / 2,
      clientY: customLink1Rect.y + customLink1Rect.height / 2,
    }));

    const reorderCalled = handler.whenCalled('reorderMostVisitedTile');

    document.dispatchEvent(new DragEvent('drop', {
      clientX: customLink2Rect.x + 1,
      clientY: customLink2Rect.y + 1,
    }));
    document.dispatchEvent(new DragEvent('dragend', {
      clientX: customLink2Rect.x + 1,
      clientY: customLink2Rect.y + 1,
    }));
    await mostVisited.updateComplete;

    const [tile, newPos] = await reorderCalled;
    assertEquals('https://c1/', tile.url.url);
    // Expected new position: original index of c1 in custom group (0) + 1
    // (because it moved past c2 in the custom group).
    // The dropIndex is 2, but there is 1 enterprise shortcut, so 2 - 1 = 1.
    assertEquals(1, newPos);

    const [newEnterprise, newCustom2, newCustom1] = queryTiles();
    assertEquals('https://e1/', newEnterprise!.querySelector('a')!.href);
    assertEquals('https://c2/', newCustom2!.querySelector('a')!.href);
    assertEquals('https://c1/', newCustom1!.querySelector('a')!.href);
  });

  test('cannot drag custom link to enterprise shortcut position', async () => {
    const enterpriseShortcut = {
      title: 'a',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: `https://a/`},
      source: TileSource.ENTERPRISE_SHORTCUTS,
      titleSource: 0,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    };
    const customLink = {
      title: 'b',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: `https://b/`},
      source: TileSource.CUSTOM_LINKS,
      titleSource: 1,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    };
    await addTiles(
        [enterpriseShortcut, customLink],
        /*customLinksEnabled=*/ true, /*visible=*/ true,
        /*enterpriseShortcutsEnabled=*/ true);

    const tiles = queryTiles();
    const first = tiles[0]!;
    const second = tiles[1]!;
    assertEquals('https://a/', first.querySelector('a')!.href);
    assertTrue(first.draggable);
    assertEquals('https://b/', second.querySelector('a')!.href);
    assertTrue(second.draggable);
    const firstRect = first.getBoundingClientRect();
    const secondRect = second.getBoundingClientRect();
    second.dispatchEvent(new DragEvent('dragstart', {
      clientX: secondRect.x + secondRect.width / 2,
      clientY: secondRect.y + secondRect.height / 2,
    }));
    document.dispatchEvent(new DragEvent('drop', {
      clientX: firstRect.x + 1,
      clientY: firstRect.y + 1,
    }));
    document.dispatchEvent(new DragEvent('dragend', {
      clientX: firstRect.x + 1,
      clientY: firstRect.y + 1,
    }));
    await mostVisited.updateComplete;
    assertEquals(0, handler.getCallCount('reorderMostVisitedTile'));
    const [newFirst, newSecond] = queryTiles();
    assertEquals('https://a/', newFirst!.querySelector('a')!.href);
    assertEquals('https://b/', newSecond!.querySelector('a')!.href);
  });

  test('cannot drag enterprise shortcut to custom link position', async () => {
    const enterpriseShortcut = {
      title: 'a',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: `https://a/`},
      source: TileSource.ENTERPRISE_SHORTCUTS,
      titleSource: 0,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    };
    const customLink = {
      title: 'b',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: `https://b/`},
      source: TileSource.CUSTOM_LINKS,
      titleSource: 1,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    };
    await addTiles(
        [enterpriseShortcut, customLink],
        /*customLinksEnabled=*/ true, /*visible=*/ true,
        /*enterpriseShortcutsEnabled=*/ true);

    const tiles = queryTiles();
    const first = tiles[0]!;
    const second = tiles[1]!;
    assertEquals('https://a/', first.querySelector('a')!.href);
    assertTrue(first.draggable);
    assertEquals('https://b/', second.querySelector('a')!.href);
    assertTrue(second.draggable);
    const firstRect = first.getBoundingClientRect();
    const secondRect = second.getBoundingClientRect();
    first.dispatchEvent(new DragEvent('dragstart', {
      clientX: firstRect.x + firstRect.width / 2,
      clientY: firstRect.y + firstRect.height / 2,
    }));
    document.dispatchEvent(new DragEvent('drop', {
      clientX: secondRect.x + 1,
      clientY: secondRect.y + 1,
    }));
    document.dispatchEvent(new DragEvent('dragend', {
      clientX: secondRect.x + 1,
      clientY: secondRect.y + 1,
    }));
    await mostVisited.updateComplete;
    assertEquals(0, handler.getCallCount('reorderMostVisitedTile'));
    const [newFirst, newSecond] = queryTiles();
    assertEquals('https://a/', newFirst!.querySelector('a')!.href);
    assertEquals('https://b/', newSecond!.querySelector('a')!.href);
  });
}

suite('DragAndDrop', () => {
  suite('double row', () => {
    createDragAndDropSuite(/*singleRow=*/ false, /*reflowOnOverflow=*/ false);
    createDragAndDropSuite(/*singleRow=*/ false, /*reflowOnOverflow=*/ true);
  });
  suite('single row', () => {
    createDragAndDropSuite(/*singleRow=*/ true, /*reflowOnOverflow=*/ false);
    createDragAndDropSuite(/*singleRow=*/ true, /*reflowOnOverflow=*/ true);
  });
});

suite('Theming', () => {
  setup(async () => {
    await setUpTest();
  });

  test('RIGHT_TO_LEFT tile title text direction', async () => {
    await addTiles([{
      title: 'title',
      titleDirection: TextDirection.RIGHT_TO_LEFT,
      url: {url: 'https://url/'},
      source: 0,
      titleSource: 0,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    }]);
    const tile = queryTiles()[0]!;
    const titleElement = tile.querySelector('.tile-title')!;
    assertEquals('rtl', window.getComputedStyle(titleElement).direction);
  });

  test('LEFT_TO_RIGHT tile title text direction', async () => {
    await addTiles([{
      title: 'title',
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://url/'},
      source: 0,
      titleSource: 0,
      isQueryTile: false,
      allowUserEdit: true,
      allowUserDelete: true,
    }]);
    const tile = queryTiles()[0]!;
    const titleElement = tile.querySelector('.tile-title')!;
    assertEquals('ltr', window.getComputedStyle(titleElement).direction);
  });

  test('setting color styles tile color', async () => {
    // Act.
    mostVisited.$.container.style.setProperty(
        '--most-visited-text-color', 'blue');
    mostVisited.$.container.style.setProperty('--tile-background-color', 'red');
    await microtasksFinished();

    // Assert.
    queryAll('.tile-title').forEach(tile => {
      assertStyle(tile, 'color', 'rgb(0, 0, 255)');
    });
    queryAll('.tile-icon').forEach(tile => {
      assertStyle(tile, 'background-color', 'rgb(255, 0, 0)');
    });
  });

  test('add shortcut white', async () => {
    assertStyle(
        $$(mostVisited, '#addShortcutIcon'), 'background-color',
        'rgb(32, 33, 36)');
    mostVisited.toggleAttribute('use-white-tile-icon_', true);
    await microtasksFinished();
    assertStyle(
        $$(mostVisited, '#addShortcutIcon'), 'background-color',
        'rgb(255, 255, 255)');
  });
});

suite('Preloading', () => {
  suiteSetup(() => {});

  setup(async () => {
    await setUpTest();
  });

  test('onMouseHover Trigger', async () => {
    // Arrange.
    await addTiles(1);

    // Act.
    const tileLink = queryTiles()[0]!.querySelector('a')!;
    // Prevent triggering a navigation, which would break the test.
    tileLink.href = '#';
    // simulate a mousedown event.
    const mouseEvent = document.createEvent('MouseEvents');
    mouseEvent.initEvent('mouseenter', true, true);
    tileLink.dispatchEvent(mouseEvent);

    await microtasksFinished();

    // Make sure preconnect has been triggered.
    await handler.whenCalled('preconnectMostVisitedTile');

    // Make sure prefetch has been triggered.
    await handler.whenCalled('prefetchMostVisitedTile');
  });

  test('onMouseDown Trigger', async () => {
    // Arrange.
    await addTiles(1);

    // Act.
    const tileLink = queryTiles()[0]!.querySelector('a')!;
    // Prevent triggering a navigation, which would break the test.
    tileLink.href = '#';
    // simulate a mousedown event.
    const mouseEvent = document.createEvent('MouseEvents');
    mouseEvent.initEvent('mousedown', true, true);
    tileLink.dispatchEvent(mouseEvent);

    // Make sure Prerendering has been triggered.
    await handler.whenCalled('prerenderMostVisitedTile');
  });
});

suite('EnterpriseShortcuts', () => {
  suiteSetup(() => {});

  setup(async () => {
    await setUpTest();
  });

  function createEnterpriseShortcut(
      i: number, allowUserEdit: boolean,
      allowUserDelete: boolean): MostVisitedTile {
    const char = String.fromCharCode(i + /* 'a' */ 97);
    return {
      title: char,
      titleDirection: TextDirection.LEFT_TO_RIGHT,
      url: {url: `https://${char}/`},
      source: TileSource.ENTERPRISE_SHORTCUTS,
      titleSource: i,
      isQueryTile: false,
      allowUserEdit: allowUserEdit,
      allowUserDelete: allowUserDelete,
    };
  }

  test('shows managed icon', async () => {
    await addTiles(
        [createEnterpriseShortcut(
            0, /*allowUserEdit=*/ true, /*allowUserDelete=*/ true)],
        /*customLinksEnabled=*/ false, /*visible=*/ true,
        /*enterpriseShortcutsEnabled=*/ true);
    const tile = queryTiles()[0]!;
    const managedIcon = tile.querySelector('.managed-tile-icon');
    assertTrue(isVisible(managedIcon));
  });

  test('edit dialog has readonly url', async () => {
    await addTiles(
        [createEnterpriseShortcut(
            0, /*allowUserEdit=*/ true, /*allowUserDelete=*/ true)],
        /*customLinksEnabled=*/ false, /*visible=*/ true,
        /*enterpriseShortcutsEnabled=*/ true);
    const tile = queryTiles()[0]!;
    tile.querySelector<HTMLElement>('#actionMenuButton')!.click();
    $$<HTMLElement>(mostVisited, '#actionMenuViewOrEdit').click();
    await microtasksFinished();

    assertTrue(mostVisited.$.dialog.open);
    assertEquals(
        'Edit shortcut',
        mostVisited.$.dialog.querySelector(
                                '[slot="title"]')!.textContent.trim());
    const policySubtitleContainer =
        mostVisited.$.dialog.querySelector<HTMLElement>(
            '#policySubtitleContainer');
    assertTrue(isVisible(policySubtitleContainer));
    const urlInput = $$<CrInputElement>(mostVisited, '#dialogInputUrl');
    assertTrue(urlInput.readonly);
    const nameInput = $$<CrInputElement>(mostVisited, '#dialogInputName');
    assertFalse(nameInput.readonly);

    // Check that we can still save a title change.
    nameInput.value = 'new title';
    await nameInput.updateComplete;
    const saveButton =
        mostVisited.$.dialog.querySelector<CrButtonElement>('.action-button')!;
    assertFalse(saveButton.disabled);
    const updateCalled = handler.whenCalled('updateMostVisitedTile');
    saveButton.click();
    const [_url, _newUrl, newTitle] = await updateCalled;
    assertEquals('new title', newTitle);
  });

  test('view dialog is readonly', async () => {
    await addTiles(
        [createEnterpriseShortcut(
            0, /*allowUserEdit=*/ false, /*allowUserDelete=*/ true)],
        /*customLinksEnabled=*/ false, /*visible=*/ true,
        /*enterpriseShortcutsEnabled=*/ true);
    const tile = queryTiles()[0]!;
    tile.querySelector<HTMLElement>('#actionMenuButton')!.click();
    $$<HTMLElement>(mostVisited, '#actionMenuViewOrEdit').click();
    await microtasksFinished();

    assertTrue(mostVisited.$.dialog.open);
    assertEquals(
        'Shortcut',
        mostVisited.$.dialog.querySelector(
                                '[slot="title"]')!.textContent.trim());
    const policySubtitleContainer =
        mostVisited.$.dialog.querySelector<HTMLElement>(
            '#policySubtitleContainer');
    assertTrue(isVisible(policySubtitleContainer));
    const urlInput = $$<CrInputElement>(mostVisited, '#dialogInputUrl');
    assertTrue(urlInput.readonly);
    const nameInput = $$<CrInputElement>(mostVisited, '#dialogInputName');
    assertTrue(nameInput.readonly);

    const saveButton =
        mostVisited.$.dialog.querySelector<CrButtonElement>('.action-button')!;
    assertFalse(saveButton.disabled);
    const cancelButton =
        mostVisited.$.dialog.querySelector<CrButtonElement>('.cancel-button')!;
    assertTrue(cancelButton.hidden);
    saveButton.click();
    await microtasksFinished();
    assertFalse(mostVisited.$.dialog.open);
    assertEquals(0, handler.getCallCount('updateMostVisitedTile'));
  });

  test('view enterprise shortcut then add shortcut', async () => {
    await addTiles(
        [
          createEnterpriseShortcut(
              0, /*allowUserEdit=*/ false, /*allowUserDelete=*/ true),
          {
            title: 'c',
            titleDirection: TextDirection.LEFT_TO_RIGHT,
            url: {url: `https://c/`},
            source: TileSource.CUSTOM_LINKS,
            titleSource: 1,
            isQueryTile: false,
            allowUserEdit: true,
            allowUserDelete: true,
          },
        ],
        /*customLinksEnabled=*/ true, /*visible=*/ true,
        /*enterpriseShortcutsEnabled=*/ true);
    const enterpriseTile = queryTiles()[0]!;
    enterpriseTile.querySelector<HTMLElement>('#actionMenuButton')!.click();
    $$<HTMLElement>(mostVisited, '#actionMenuViewOrEdit').click();
    await microtasksFinished();

    // Verify dialog state for enterprise shortcut.
    assertTrue(mostVisited.$.dialog.open);
    let policySubtitleContainer =
        mostVisited.$.dialog.querySelector<HTMLElement>(
            '#policySubtitleContainer');
    assertTrue(isVisible(policySubtitleContainer));
    let urlInput = $$<CrInputElement>(mostVisited, '#dialogInputUrl');
    assertTrue(urlInput.readonly);
    let nameInput = $$<CrInputElement>(mostVisited, '#dialogInputName');
    assertTrue(nameInput.readonly);

    // Close the dialog.
    const saveButton =
        mostVisited.$.dialog.querySelector<CrButtonElement>('.action-button')!;
    saveButton.click();
    await microtasksFinished();
    assertFalse(mostVisited.$.dialog.open);

    // Open the add shortcut dialog.
    mostVisited.$.addShortcut.click();
    await microtasksFinished();

    // Verify dialog state for adding a new shortcut.
    assertTrue(mostVisited.$.dialog.open);
    policySubtitleContainer = mostVisited.$.dialog.querySelector<HTMLElement>(
        '#policySubtitleContainer');
    assertFalse(isVisible(policySubtitleContainer));
    urlInput = $$<CrInputElement>(mostVisited, '#dialogInputUrl');
    assertFalse(urlInput.readonly);
    nameInput = $$<CrInputElement>(mostVisited, '#dialogInputName');
    assertFalse(nameInput.readonly);
  });

  test('action menu enabled/disabled based on permissions', async () => {
    const tiles = [
      createEnterpriseShortcut(
          0, /*allowUserEdit=*/ true, /*allowUserDelete=*/ true),
      createEnterpriseShortcut(
          1, /*allowUserEdit=*/ true, /*allowUserDelete=*/ false),
      createEnterpriseShortcut(
          2, /*allowUserEdit=*/ false, /*allowUserDelete=*/ true),
      createEnterpriseShortcut(
          3, /*allowUserEdit=*/ false, /*allowUserDelete=*/ false),
    ];
    await addTiles(
        tiles, /*customLinksEnabled=*/ false, /*visible=*/ true,
        /*enterpriseShortcutsEnabled=*/ true);

    const tileElements = queryTiles();
    assertEquals(4, tileElements.length);

    // Case 1: edit and delete allowed.
    let tile = tileElements[0]!;
    tile.querySelector<HTMLElement>('#actionMenuButton')!.click();
    await microtasksFinished();
    assertTrue(mostVisited.$.actionMenu.open);
    let viewOrEditButton =
        $$<HTMLButtonElement>(mostVisited, '#actionMenuViewOrEdit');
    assertFalse(viewOrEditButton.disabled);
    assertEquals('Edit shortcut', viewOrEditButton.textContent.trim());
    assertFalse(
        $$<HTMLButtonElement>(mostVisited, '#actionMenuRemove').disabled);
    mostVisited.$.actionMenu.close();
    await microtasksFinished();

    // Case 2: only edit allowed.
    tile = tileElements[1]!;
    tile.querySelector<HTMLElement>('#actionMenuButton')!.click();
    await microtasksFinished();
    assertTrue(mostVisited.$.actionMenu.open);
    viewOrEditButton =
        $$<HTMLButtonElement>(mostVisited, '#actionMenuViewOrEdit');
    assertFalse(viewOrEditButton.disabled);
    assertEquals('Edit shortcut', viewOrEditButton.textContent.trim());
    assertTrue(
        $$<HTMLButtonElement>(mostVisited, '#actionMenuRemove').disabled);
    mostVisited.$.actionMenu.close();
    await microtasksFinished();

    // Case 3: only delete allowed.
    tile = tileElements[2]!;
    tile.querySelector<HTMLElement>('#actionMenuButton')!.click();
    await microtasksFinished();
    assertTrue(mostVisited.$.actionMenu.open);
    viewOrEditButton =
        $$<HTMLButtonElement>(mostVisited, '#actionMenuViewOrEdit');
    assertFalse(viewOrEditButton.disabled);
    assertEquals('Details', viewOrEditButton.textContent.trim());
    assertFalse(
        $$<HTMLButtonElement>(mostVisited, '#actionMenuRemove').disabled);
    mostVisited.$.actionMenu.close();
    await microtasksFinished();

    // Case 4: neither allowed.
    tile = tileElements[3]!;
    tile.querySelector<HTMLElement>('#actionMenuButton')!.click();
    await microtasksFinished();
    assertTrue(mostVisited.$.actionMenu.open);
    viewOrEditButton =
        $$<HTMLButtonElement>(mostVisited, '#actionMenuViewOrEdit');
    assertFalse(viewOrEditButton.disabled);
    assertEquals('Details', viewOrEditButton.textContent.trim());
    assertTrue(
        $$<HTMLButtonElement>(mostVisited, '#actionMenuRemove').disabled);
  });
});

suite('ShortcutsAutoRemovalToast', () => {
  setup(async () => {
    await setUpTest();
  });

  test('toast shown with Undo auto removal button', async () => {
    // TODO(crbug.com/467437715): Call this via the callback router once the
    // browser side is ready.
    mostVisited.autoRemovalToast();
    await microtasksFinished();
    assertTrue(mostVisited.$.toastManager.isToastOpen);

    // Check undo auto removal button is visible.
    const undoAutoRemovalButton =
        mostVisited.shadowRoot.querySelector<HTMLElement>('#undoAutoRemoval');
    assertTrue(!!undoAutoRemovalButton);
    assertFalse(undoAutoRemovalButton.hidden);

    // Check other buttons are removed from the DOM.
    const undoButton =
        mostVisited.shadowRoot.querySelector<HTMLElement>('#undo');
    assertEquals(null, undoButton);
    const restoreButton =
        mostVisited.shadowRoot.querySelector<HTMLElement>('#restore');
    assertEquals(null, restoreButton);
  });

  test('toast undo auto removal button', async () => {
    // Check toast is not open initially.
    assertFalse(mostVisited.$.toastManager.isToastOpen);

    // Check toast is open after auto removal event.
    // TODO(crbug.com/467437715): Call this via the callback router once the
    // browser side is ready.
    mostVisited.autoRemovalToast();
    await microtasksFinished();
    assertTrue(mostVisited.$.toastManager.isToastOpen);

    // Check undo auto removal button is visible.
    const undoAutoRemovalButton =
        mostVisited.shadowRoot.querySelector<HTMLElement>('#undoAutoRemoval');
    assertTrue(!!undoAutoRemovalButton);
    assertFalse(undoAutoRemovalButton.hidden);

    // Check undo auto removal button click calls the handler and closes the
    // toast.
    const wait = handler.whenCalled('undoMostVisitedAutoRemoval');
    undoAutoRemovalButton.click();
    await wait;
    assertFalse(mostVisited.$.toastManager.isToastOpen);
  });
});
