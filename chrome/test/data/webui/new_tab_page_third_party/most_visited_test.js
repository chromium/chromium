// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome://new-tab-page-third-party/new_tab_page_third_party.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {assertNotStyle, assertStyle, createTestProxy, keydown} from 'chrome://test/new_tab_page_third_party/test_support.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageThirdPartyMostVisitedTest', () => {
  /** @type {!MostVisitedElement} */
  let mostVisited;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  /** @type {!MediaListenerList} */
  let mediaListenerWideWidth;

  /** @type {!MediaListenerList} */
  let mediaListenerMediumWidth;

  /** @type {!Function} */
  let mediaListener;

  /**
   * @param {!HTMLElement} element
   * @param {string} query
   * @return {HTMLElement}
   * @private
   */
  const $$ = (element, query) => element.shadowRoot.querySelector(query);

  /**
   * @param {string} q
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
   * @return {!Array<!HTMLElement>}
   * @private
   */
  function queryHiddenTiles() {
    return queryAll('.tile[hidden]');
  }

  /**
   * @param {number} length
   * @private
   */
  function assertTileLength(length) {
    assertEquals(length, queryTiles().length);
  }

  /**
   * @param {number} length
   * @private
   */
  function assertHiddenTileLength(length) {
    assertEquals(length, queryHiddenTiles().length);
  }

  /**
   * @param {number|!Array} n
   * @return {!Promise}
   * @private
   */
  async function addTiles(n) {
    const tiles = Array.isArray(n) ? n : Array(n).fill(0).map((x, i) => {
      const char = String.fromCharCode(i + /* 'a' */ 97);
      return {
        title: char,
        titleDirection: mojoBase.mojom.TextDirection.LEFT_TO_RIGHT,
        url: {url: `https://${char}/`},
        source: i,
        titleSource: i,
        isQueryTile: false,
        dataGenerationTime: {internalValue: BigInt(0)},
      };
    });
    const tilesRendered = eventToPromise('dom-change', mostVisited.$.tiles);
    testProxy.callbackRouterRemote.setMostVisitedTiles(tiles);
    await testProxy.callbackRouterRemote.$.flushForTesting();
    await tilesRendered;
  }

  /**
   * @return {!Promise}
   * @private
   */
  async function addAndDeleteTile() {
    await addTiles(1);
    keydown(queryTiles()[0], 'Delete');
  }

  /**
   * @return {!Promise}
   * @private
   */
  async function addAndDeleteQueryTile() {
    await addTiles([{
      title: 'title',
      titleDirection: mojoBase.mojom.TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://search-url/'},
      source: 0,
      titleSource: 0,
      isQueryTile: true,
      dataGenerationTime: {internalValue: BigInt(0)},
    }]);
    keydown(queryTiles()[0], 'Delete');
  }

  /**
   * @param {boolean} isWide
   * @param {boolean} isMedium
   */
  function updateScreenWidth(isWide, isMedium) {
    assertTrue(!!mediaListenerWideWidth);
    assertTrue(!!mediaListenerMediumWidth);
    mediaListenerWideWidth.matches = isWide;
    mediaListenerMediumWidth.matches = isMedium;
    mediaListener();
  }

  function wide() {
    updateScreenWidth(true, true);
  }

  suiteSetup(() => {
    loadTimeData.overrideValues({
      linkRemovedMsg: '',
    });
  });

  setup(() => {
    PolymerTest.clearBody();
    testProxy = createTestProxy();
    testProxy.setResultMapperFor('matchMedia', query => {
      const mediaListenerList = {
        matches: false,  // Used to determine the screen width.
        media: query,
        addListener(listener) {
          mediaListener = listener;
        },
        removeListener() {},
      };
      if (query === '(min-width: 672px)') {
        mediaListenerWideWidth = mediaListenerList;
      } else if (query === '(min-width: 560px)') {
        mediaListenerMediumWidth = mediaListenerList;
      } else {
        assertTrue(false);
      }
      return mediaListenerList;
    });
    BrowserProxy.instance_ = testProxy;
    mostVisited = document.createElement('ntp3p-most-visited');
    document.body.appendChild(mostVisited);
    assertEquals(2, testProxy.getCallCount('matchMedia'));
    wide();
  });

  /**
   * @param {number} topRowCount
   * @param {number} tileCount
   * @return {!Promise}
   * @private
   */
  const assertTilesOnCorrectRows = async (topRowCount, tileCount) => {
    await addTiles(tileCount);
    assertTileLength(tileCount);
    const tops = queryTiles().map(({offsetTop}) => offsetTop);
    assertEquals(tileCount, tops.length);
    const firstRowTop = tops[0];
    tops.slice(0, topRowCount).forEach(top => {
      assertEquals(firstRowTop, top);
    });
    if (topRowCount < tileCount) {
      const secondRowTop = tops[topRowCount];
      assertNotEquals(firstRowTop, secondRowTop);
      tops.slice(topRowCount).forEach(top => {
        assertEquals(secondRowTop, top);
      });
    }
  };

  test('1 to 4 tiles fit on one line', async () => {
    await assertTilesOnCorrectRows(1, 1);
    await assertTilesOnCorrectRows(2, 2);
    await assertTilesOnCorrectRows(3, 3);
    await assertTilesOnCorrectRows(4, 4);
  });

  test('5 and 6 tiles are displayed on two row', async () => {
    await assertTilesOnCorrectRows(3, 5);
    await assertTilesOnCorrectRows(3, 6);
  });

  test('7 and 8 tiles are displayed on two rows', async () => {
    await assertTilesOnCorrectRows(4, 7);
    await assertTilesOnCorrectRows(4, 8);
  });

  test('eight tiles is the max tiles displayed', async () => {
    assertTileLength(0);
    await addTiles(11);
    assertTileLength(8);
  });

  suite('test various widths', () => {
    function medium() {
      updateScreenWidth(false, true);
    }

    function narrow() {
      updateScreenWidth(false, false);
    }

    test('six is max for narrow', async () => {
      await addTiles(7);
      medium();
      assertTileLength(7);
      assertHiddenTileLength(0);
      narrow();
      assertTileLength(7);
      assertHiddenTileLength(1);
      medium();
      assertTileLength(7);
      assertHiddenTileLength(0);
    });

    test('eight is max for medium', async () => {
      await addTiles(8);
      narrow();
      assertTileLength(8);
      assertHiddenTileLength(2);
      medium();
      assertTileLength(8);
      assertHiddenTileLength(0);
      narrow();
      assertTileLength(8);
      assertHiddenTileLength(2);
    });

    test('eight is max for wide', async () => {
      await addTiles(8);
      narrow();
      assertTileLength(8);
      assertHiddenTileLength(2);
      wide();
      assertTileLength(8);
      assertHiddenTileLength(0);
      narrow();
      assertTileLength(8);
      assertHiddenTileLength(2);
    });
  });

  test('remove with icon button', async () => {
    await addTiles(1);
    const removeButton = queryTiles()[0].querySelector('#removeButton');
    const deleteCalled = testProxy.handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toast.open);
    removeButton.click();
    assertEquals('https://a/', (await deleteCalled).url);
    assertTrue(mostVisited.$.toast.open);
    // Toast buttons are visible.
    assertTrue(!!$$(mostVisited, '#undo'));
    assertTrue(!!$$(mostVisited, '#restore'));
  });

  test('remove query with icon button', async () => {
    await addTiles([{
      title: 'title',
      titleDirection: mojoBase.mojom.TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://search-url/'},
      source: 0,
      titleSource: 0,
      isQueryTile: true,
      dataGenerationTime: {internalValue: BigInt(0)},
    }]);
    const removeButton = queryTiles()[0].querySelector('#removeButton');
    const deleteCalled = testProxy.handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toast.open);
    removeButton.click();
    assertEquals('https://search-url/', (await deleteCalled).url);
    assertTrue(mostVisited.$.toast.open);
    // Toast buttons are not visible.
    assertFalse(!!$$(mostVisited, '#undo'));
    assertFalse(!!$$(mostVisited, '#restore'));
  });

  test('tile url is set to href of <a>', async () => {
    await addTiles(1);
    const [tile] = queryTiles();
    assertEquals('https://a/', tile.href);
  });

  test('delete first tile', async () => {
    await addTiles(1);
    const [tile] = queryTiles();
    const deleteCalled = testProxy.handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toast.open);
    keydown(tile, 'Delete');
    assertEquals('https://a/', (await deleteCalled).url);
    assertTrue(mostVisited.$.toast.open);
  });

  test('ctrl+z triggers undo and hides toast', async () => {
    const {toast} = mostVisited.$;
    assertFalse(toast.open);
    await addAndDeleteTile();
    assertTrue(toast.open);
    const undoCalled =
        testProxy.handler.whenCalled('undoMostVisitedTileAction');
    mostVisited.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      ctrlKey: !isMac,
      key: 'z',
      metaKey: isMac,
    }));
    await undoCalled;
    assertFalse(toast.open);
  });

  test('ctrl+z does nothing if toast buttons are not showing', async () => {
    const {toast} = mostVisited.$;
    assertFalse(toast.open);
    await addAndDeleteQueryTile();
    assertTrue(toast.open);
    mostVisited.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      ctrlKey: !isMac,
      key: 'z',
      metaKey: isMac,
    }));
    assertEquals(
        0, testProxy.handler.getCallCount('undoMostVisitedTileAction'));
    assertTrue(toast.open);
  });

  test('toast restore defaults button', async () => {
    const wait = testProxy.handler.whenCalled('restoreMostVisitedDefaults');
    const {toast} = mostVisited.$;
    assertFalse(toast.open);
    await addAndDeleteTile();
    await flushTasks();
    assertTrue(toast.open);
    toast.querySelector('#restore').click();
    await wait;
    assertFalse(toast.open);
  });

  test('toast undo button', async () => {
    const wait = testProxy.handler.whenCalled('undoMostVisitedTileAction');
    const {toast} = mostVisited.$;
    assertFalse(toast.open);
    await addAndDeleteTile();
    await flushTasks();
    assertTrue(toast.open);
    toast.querySelector('#undo').click();
    await wait;
    assertFalse(toast.open);
  });

  test('RIGHT_TO_LEFT tile title text direction', async () => {
    await addTiles([{
      title: 'title',
      titleDirection: mojoBase.mojom.TextDirection.RIGHT_TO_LEFT,
      url: {url: 'https://url/'},
      source: 0,
      titleSource: 0,
      isQueryTile: false,
      dataGenerationTime: {internalValue: BigInt(0)},
    }]);
    const [tile] = queryTiles();
    const titleElement = tile.querySelector('.tile-title');
    assertEquals('rtl', window.getComputedStyle(titleElement).direction);
  });

  test('LEFT_TO_RIGHT tile title text direction', async () => {
    await addTiles([{
      title: 'title',
      titleDirection: mojoBase.mojom.TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://url/'},
      source: 0,
      titleSource: 0,
      isQueryTile: false,
      dataGenerationTime: {internalValue: BigInt(0)},
    }]);
    const [tile] = queryTiles();
    const titleElement = tile.querySelector('.tile-title');
    assertEquals('ltr', window.getComputedStyle(titleElement).direction);
  });

  test('setting color styles tile color', () => {
    // Act.
    mostVisited.style.setProperty('--ntp-theme-text-color', 'blue');
    mostVisited.style.setProperty(
        '--ntp-theme-shortcut-background-color', 'red');

    // Assert.
    queryAll('.tile-title').forEach(tile => {
      assertStyle(tile, 'color', 'rgb(0, 0, 255)');
    });
    queryAll('.tile-icon').forEach(tile => {
      assertStyle(tile, 'background-color', 'rgb(255, 0, 0)');
    });
  });

  test('add title pill', () => {
    mostVisited.style.setProperty('--ntp-theme-text-shadow', '1px 2px');
    queryAll('.tile-title').forEach(tile => {
      assertStyle(tile, 'background-color', 'rgba(0, 0, 0, 0)');
    });
    queryAll('.tile-title span').forEach(tile => {
      assertNotStyle(tile, 'text-shadow', 'none');
    });
    mostVisited.toggleAttribute('use-title-pill', true);
    queryAll('.tile-title').forEach(tile => {
      assertStyle(tile, 'background-color', 'rgb(255, 255, 255)');
    });
    queryAll('.tile-title span').forEach(tile => {
      assertStyle(tile, 'text-shadow', 'none');
      assertStyle(tile, 'color', 'rgb(60, 64, 67)');
    });
  });

  test('rendering tiles logs event', async () => {
    // Arrange.
    testProxy.setResultFor('now', 123);

    // Act.
    await addTiles(2);

    // Assert.
    const [tiles, time] =
        await testProxy.handler.whenCalled('onMostVisitedTilesRendered');
    assertEquals(time, 123);
    assertEquals(tiles.length, 2);
    assertDeepEquals(tiles[0], {
      title: 'a',
      titleDirection: mojoBase.mojom.TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://a/'},
      source: 0,
      titleSource: 0,
      isQueryTile: false,
      dataGenerationTime: {internalValue: BigInt(0)},
    });
    assertDeepEquals(tiles[1], {
      title: 'b',
      titleDirection: mojoBase.mojom.TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://b/'},
      source: 1,
      titleSource: 1,
      isQueryTile: false,
      dataGenerationTime: {internalValue: BigInt(0)},
    });
  });

  test('clicking tile logs event', async () => {
    // Arrange.
    await addTiles(1);

    // Act.
    const tileLink = queryTiles()[0];
    // Prevent triggering a navigation, which would break the test.
    tileLink.href = '#';
    tileLink.click();

    // Assert.
    const [tile, index] =
        await testProxy.handler.whenCalled('onMostVisitedTileNavigation');
    assertEquals(index, 0);
    assertDeepEquals(tile, {
      title: 'a',
      titleDirection: mojoBase.mojom.TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://a/'},
      source: 0,
      titleSource: 0,
      isQueryTile: false,
      dataGenerationTime: {internalValue: BigInt(0)},
    });
  });

  test('making tab visible refreshes most visited tiles', () => {
    // Arrange.
    testProxy.handler.resetResolver('updateMostVisitedTiles');

    // Act.
    document.dispatchEvent(new Event('visibilitychange'));

    // Assert.
    assertEquals(1, testProxy.handler.getCallCount('updateMostVisitedTiles'));
  });
});
