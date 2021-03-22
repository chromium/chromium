// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {$$, NewTabPageProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {assertNotStyle, assertStyle, keydown} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

suite('NewTabPageMostVisitedTest', () => {
  /** @type {!MostVisitedElement} */
  let mostVisited;

  /**
   * @implements {WindowProxy}
   * @extends {TestBrowserProxy}
   */
  let windowProxy;

  /**
   * @implements {newTabPage.mojom.PageHandlerRemote}
   * @extends {TestBrowserProxy}
   */
  let handler;

  /** @type {newTabPage.mojom.PageHandlerRemote} */
  let callbackRouterRemote;

  /** @type {!MediaListenerList} */
  let mediaListenerWideWidth;

  /** @type {!MediaListenerList} */
  let mediaListenerMediumWidth;

  /** @type {!Function} */
  let mediaListener;

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
   * @param {number|!Array} n
   * @param {boolean=} customLinksEnabled
   * @param {boolean=} visible
   * @return {!Promise}
   * @private
   */
  async function addTiles(n, customLinksEnabled = true, visible = true) {
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
    callbackRouterRemote.setMostVisitedInfo({
      customLinksEnabled: customLinksEnabled,
      tiles: tiles,
      visible: visible,
    });
    await callbackRouterRemote.$.flushForTesting();
    await tilesRendered;
  }

  /** @private */
  function assertAddShortcutHidden() {
    assertTrue(mostVisited.$.addShortcut.hidden);
  }

  /** @private */
  function assertAddShortcutShown() {
    assertFalse(mostVisited.$.addShortcut.hidden);
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

  function leaveUrlInput() {
    mostVisited.$.dialogInputUrl.dispatchEvent(new Event('blur'));
  }

  suiteSetup(() => {
    loadTimeData.overrideValues({
      linkRemovedMsg: '',
    });
  });

  setup(() => {
    PolymerTest.clearBody();

    windowProxy = TestBrowserProxy.fromClass(WindowProxy);
    handler = TestBrowserProxy.fromClass(newTabPage.mojom.PageHandlerRemote);
    handler.setResultFor('addMostVisitedTile', Promise.resolve({
      success: true,
    }));
    handler.setResultFor('updateMostVisitedTile', Promise.resolve({
      success: true,
    }));
    windowProxy.setResultMapperFor('matchMedia', query => {
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
    WindowProxy.setInstance(windowProxy);
    const callbackRouter = new newTabPage.mojom.PageCallbackRouter();
    NewTabPageProxy.setInstance(handler, callbackRouter);
    callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();
    mostVisited = document.createElement('ntp-most-visited');
    document.body.appendChild(mostVisited);
    assertEquals(2, windowProxy.getCallCount('matchMedia'));
    wide();
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

  test('four tiles fit on one line with addShortcut', async () => {
    await addTiles(4);
    assertEquals(4, queryTiles().length);
    assertAddShortcutShown();
    const tops = queryAll('a, #addShortcut').map(({offsetTop}) => offsetTop);
    assertEquals(5, tops.length);
    tops.forEach(top => {
      assertEquals(tops[0], top);
    });
  });

  test('five tiles are displayed on two rows with addShortcut', async () => {
    await addTiles(5);
    assertEquals(5, queryTiles().length);
    assertAddShortcutShown();
    const tops = queryAll('a, #addShortcut').map(({offsetTop}) => offsetTop);
    assertEquals(6, tops.length);
    const firstRowTop = tops[0];
    const secondRowTop = tops[3];
    assertNotEquals(firstRowTop, secondRowTop);
    tops.slice(0, 3).forEach(top => {
      assertEquals(firstRowTop, top);
    });
    tops.slice(3).forEach(top => {
      assertEquals(secondRowTop, top);
    });
  });

  test('nine tiles are displayed on two rows with addShortcut', async () => {
    await addTiles(9);
    assertEquals(9, queryTiles().length);
    assertAddShortcutShown();
    const tops = queryAll('a, #addShortcut').map(({offsetTop}) => offsetTop);
    assertEquals(10, tops.length);
    const firstRowTop = tops[0];
    const secondRowTop = tops[5];
    assertNotEquals(firstRowTop, secondRowTop);
    tops.slice(0, 5).forEach(top => {
      assertEquals(firstRowTop, top);
    });
    tops.slice(5).forEach(top => {
      assertEquals(secondRowTop, top);
    });
  });

  test('ten tiles are displayed on two rows without addShortcut', async () => {
    await addTiles(10);
    assertEquals(10, queryTiles().length);
    assertAddShortcutHidden();
    const tops = queryAll('a:not([hidden])').map(a => a.offsetTop);
    assertEquals(10, tops.length);
    const firstRowTop = tops[0];
    const secondRowTop = tops[5];
    assertNotEquals(firstRowTop, secondRowTop);
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
    assertTrue(mostVisited.visible_);
    assertFalse(mostVisited.$.container.hidden);
    await addTiles(1, /* customLinksEnabled */ true, /* visible */ false);
    assertEquals(1, queryTiles().length);
    assertEquals(0, queryAll('.tile[hidden]').length);
    assertFalse(mostVisited.visible_);
    assertTrue(mostVisited.$.container.hidden);
    await addTiles(1, /* customLinksEnabled */ true, /* visible */ true);
    assertEquals(1, queryTiles().length);
    assertEquals(0, queryAll('.tile[hidden]').length);
    assertTrue(mostVisited.visible_);
    assertFalse(mostVisited.$.container.hidden);
  });

  test('dialog opens when add shortcut clicked', () => {
    const {dialog} = mostVisited.$;
    assertFalse(dialog.open);
    mostVisited.$.addShortcut.click();
    assertTrue(dialog.open);
  });

  suite('test various widths', () => {
    function medium() {
      updateScreenWidth(false, true);
    }

    function narrow() {
      updateScreenWidth(false, false);
    }

    test('hide add shortcut if on third row (narrow)', async () => {
      await addTiles(6);
      medium();
      assertAddShortcutShown();
      narrow();
      assertAddShortcutHidden();
      medium();
      assertAddShortcutShown();
    });

    test('hide add shortcut if on third row (medium)', async () => {
      await addTiles(8);
      wide();
      assertAddShortcutShown();
      medium();
      assertAddShortcutHidden();
      wide();
      assertAddShortcutShown();
    });

    test('hide add shortcut if on third row (medium)', async () => {
      await addTiles(9);
      wide();
      assertAddShortcutShown();
      await addTiles(10);
      assertAddShortcutHidden();
    });
  });

  suite('add dialog', () => {
    /** @private {CrDialogElement} */
    let dialog;
    /** @private {CrInputElement} */
    let inputName;
    /** @private {CrInputElement} */
    let inputUrl;
    /** @private {CrButtonElement} */
    let saveButton;
    /** @private {CrButtonElement} */
    let cancelButton;

    setup(() => {
      dialog = mostVisited.$.dialog;
      inputName = mostVisited.$.dialogInputName;
      inputUrl = mostVisited.$.dialogInputUrl;
      saveButton = dialog.querySelector('.action-button');
      cancelButton = dialog.querySelector('.cancel-button');
      mostVisited.$.addShortcut.click();
    });

    test('inputs are initially empty', () => {
      assertEquals('', inputName.value);
      assertEquals('', inputUrl.value);
    });

    test('saveButton is enabled with URL is not empty', () => {
      assertTrue(saveButton.disabled);

      inputName.value = 'name';
      assertTrue(saveButton.disabled);

      inputUrl.value = 'url';
      assertFalse(saveButton.disabled);

      inputUrl.value = '';
      assertTrue(saveButton.disabled);

      inputUrl.value = 'url';
      assertFalse(saveButton.disabled);

      inputUrl.value = '                                \n\n\n        ';
      assertTrue(saveButton.disabled);
    });

    test('cancel closes dialog', () => {
      assertTrue(dialog.open);
      cancelButton.click();
      assertFalse(dialog.open);
    });

    test('inputs are clear after dialog reuse', () => {
      inputName.value = 'name';
      inputUrl.value = 'url';
      cancelButton.click();
      mostVisited.$.addShortcut.click();
      assertEquals('', inputName.value);
      assertEquals('', inputUrl.value);
    });

    test('use URL input for title when title empty', async () => {
      inputUrl.value = 'url';
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      const [url, title] = await addCalled;
      assertEquals('url', title);
    });

    test('toast shown on save', async () => {
      inputUrl.value = 'url';
      assertFalse(mostVisited.$.toast.open);
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      await addCalled;
      assertTrue(mostVisited.$.toast.open);
    });

    test('toast has undo buttons when action successful', async () => {
      handler.setResultFor('addMostVisitedTile', Promise.resolve({
        success: true,
      }));
      inputUrl.value = 'url';
      saveButton.click();
      await handler.whenCalled('addMostVisitedTile');
      await flushTasks();
      assertFalse($$(mostVisited, '#undo').hidden);
    });

    test('toast has no undo buttons when action successful', async () => {
      handler.setResultFor('addMostVisitedTile', Promise.resolve({
        success: false,
      }));
      inputUrl.value = 'url';
      saveButton.click();
      await handler.whenCalled('addMostVisitedTile');
      await flushTasks();
      assertFalse(!!$$(mostVisited, '#undo'));
    });

    test('save name and URL', async () => {
      inputName.value = 'name';
      inputUrl.value = 'https://url/';
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      const [{url}, title] = await addCalled;
      assertEquals('name', title);
      assertEquals('https://url/', url);
    });

    test('dialog closes on save', () => {
      inputUrl.value = 'url';
      assertTrue(dialog.open);
      saveButton.click();
      assertFalse(dialog.open);
    });

    test('https:// is added if no scheme is used', async () => {
      inputUrl.value = 'url';
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      const [{url}, title] = await addCalled;
      assertEquals('https://url/', url);
    });

    test('http is a valid scheme', async () => {
      assertTrue(saveButton.disabled);
      inputUrl.value = 'http://url';
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      await addCalled;
      assertFalse(saveButton.disabled);
    });

    test('https is a valid scheme', async () => {
      inputUrl.value = 'https://url';
      const addCalled = handler.whenCalled('addMostVisitedTile');
      saveButton.click();
      await addCalled;
    });

    test('chrome is not a valid scheme', () => {
      assertTrue(saveButton.disabled);
      inputUrl.value = 'chrome://url';
      assertFalse(inputUrl.invalid);
      leaveUrlInput();
      assertTrue(inputUrl.invalid);
      assertTrue(saveButton.disabled);
    });

    test('invalid cleared when text entered', () => {
      inputUrl.value = '%';
      assertFalse(inputUrl.invalid);
      leaveUrlInput();
      assertTrue(inputUrl.invalid);
      assertEquals('Type a valid URL', inputUrl.errorMessage);
      inputUrl.value = '';
      assertFalse(inputUrl.invalid);
    });

    test('shortcut already exists', async () => {
      await addTiles(2);
      inputUrl.value = 'b';
      assertFalse(inputUrl.invalid);
      leaveUrlInput();
      assertTrue(inputUrl.invalid);
      assertEquals('Shortcut already exists', inputUrl.errorMessage);
      inputUrl.value = 'c';
      assertFalse(inputUrl.invalid);
      leaveUrlInput();
      assertFalse(inputUrl.invalid);
      inputUrl.value = '%';
      assertFalse(inputUrl.invalid);
      leaveUrlInput();
      assertTrue(inputUrl.invalid);
      assertEquals('Type a valid URL', inputUrl.errorMessage);
    });
  });

  test('open edit dialog', async () => {
    await addTiles(2);
    const {actionMenu, dialog} = mostVisited.$;
    assertFalse(actionMenu.open);
    queryTiles()[0].querySelector('#actionMenuButton').click();
    assertTrue(actionMenu.open);
    assertFalse(dialog.open);
    mostVisited.$.actionMenuEdit.click();
    assertFalse(actionMenu.open);
    assertTrue(dialog.open);
  });

  suite('edit dialog', () => {
    /** @private {CrActionMenuElement} */
    let actionMenu;
    /** @private {CrIconButtonElement} */
    let actionMenuButton;
    /** @private {CrDialogElement} */
    let dialog;
    /** @private {CrInputElement} */
    let inputName;
    /** @private {CrInputElement} */
    let inputUrl;
    /** @private {CrButtonElement} */
    let saveButton;
    /** @private {CrButtonElement} */
    let cancelButton;
    /** @private {HTMLElement} */
    let tile;

    setup(async () => {
      actionMenu = mostVisited.$.actionMenu;
      dialog = mostVisited.$.dialog;
      inputName = mostVisited.$.dialogInputName;
      inputUrl = mostVisited.$.dialogInputUrl;
      saveButton = dialog.querySelector('.action-button');
      cancelButton = dialog.querySelector('.cancel-button');
      await addTiles(2);
      tile = queryTiles()[1];
      actionMenuButton = tile.querySelector('#actionMenuButton');
      actionMenuButton.click();
      mostVisited.$.actionMenuEdit.click();
    });

    test('edit a tile URL', async () => {
      assertEquals('https://b/', inputUrl.value);
      const updateCalled = handler.whenCalled('updateMostVisitedTile');
      inputUrl.value = 'updated-url';
      saveButton.click();
      const [url, newUrl, newTitle] = await updateCalled;
      assertEquals('https://updated-url/', newUrl.url);
    });

    test('toast shown when tile editted', async () => {
      inputUrl.value = 'updated-url';
      assertFalse(mostVisited.$.toast.open);
      saveButton.click();
      await handler.whenCalled('updateMostVisitedTile');
      assertTrue(mostVisited.$.toast.open);
    });

    test('no toast when not editted', async () => {
      assertFalse(mostVisited.$.toast.open);
      saveButton.click();
      await flushTasks();
      assertFalse(mostVisited.$.toast.open);
    });

    test('edit a tile title', async () => {
      assertEquals('b', inputName.value);
      const updateCalled = handler.whenCalled('updateMostVisitedTile');
      inputName.value = 'updated name';
      saveButton.click();
      const [url, newUrl, newTitle] = await updateCalled;
      assertEquals('updated name', newTitle);
    });

    test('update not called when name and URL not changed', async () => {
      // |updateMostVisitedTile| will be called only after either the title or
      // url has changed.
      const updateCalled = handler.whenCalled('updateMostVisitedTile');
      saveButton.click();

      // Reopen dialog and edit URL.
      actionMenuButton.click();
      mostVisited.$.actionMenuEdit.click();
      inputUrl.value = 'updated-url';
      saveButton.click();

      const [url, newUrl, newTitle] = await updateCalled;
      assertEquals('https://updated-url/', newUrl.url);
    });

    test('shortcut already exists', async () => {
      inputUrl.value = 'a';
      assertFalse(inputUrl.invalid);
      leaveUrlInput();
      assertTrue(inputUrl.invalid);
      assertEquals('Shortcut already exists', inputUrl.errorMessage);
      // The shortcut being editted has a URL of https://b/. Entering the same
      // URL is not an error.
      inputUrl.value = 'b';
      assertFalse(inputUrl.invalid);
      leaveUrlInput();
      assertFalse(inputUrl.invalid);
    });
  });

  test('remove with action menu', async () => {
    const {actionMenu, actionMenuRemove: removeButton} = mostVisited.$;
    await addTiles(2);
    const secondTile = queryTiles()[1];
    const actionMenuButton = secondTile.querySelector('#actionMenuButton');

    assertFalse(actionMenu.open);
    actionMenuButton.click();
    assertTrue(actionMenu.open);
    const deleteCalled = handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toast.open);
    removeButton.click();
    assertFalse(actionMenu.open);
    assertEquals('https://b/', (await deleteCalled).url);
    assertTrue(mostVisited.$.toast.open);
    // Toast buttons are visible.
    assertTrue(!!$$(mostVisited, '#undo'));
    assertTrue(!!$$(mostVisited, '#restore'));
  });

  test('remove query with action menu', async () => {
    const {actionMenu, actionMenuRemove: removeButton} = mostVisited.$;
    await addTiles([{
      title: 'title',
      titleDirection: mojoBase.mojom.TextDirection.LEFT_TO_RIGHT,
      url: {url: 'https://search-url/'},
      source: 0,
      titleSource: 0,
      isQueryTile: true,
      dataGenerationTime: {internalValue: BigInt(0)},
    }]);
    const actionMenuButton = queryTiles()[0].querySelector('#actionMenuButton');

    assertFalse(actionMenu.open);
    actionMenuButton.click();
    assertTrue(actionMenu.open);
    const deleteCalled = handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toast.open);
    removeButton.click();
    assertEquals('https://search-url/', (await deleteCalled).url);
    assertTrue(mostVisited.$.toast.open);
    // Toast buttons are visible.
    assertTrue(!!$$(mostVisited, '#undo'));
    assertTrue(!!$$(mostVisited, '#restore'));
  });

  test('remove with icon button (customLinksEnabled=false)', async () => {
    await addTiles(1, /* customLinksEnabled */ false);
    const removeButton = queryTiles()[0].querySelector('#removeButton');
    const deleteCalled = handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toast.open);
    removeButton.click();
    assertEquals('https://a/', (await deleteCalled).url);
    assertTrue(mostVisited.$.toast.open);
    // Toast buttons are visible.
    assertTrue(!!$$(mostVisited, '#undo'));
    assertTrue(!!$$(mostVisited, '#restore'));
  });

  test('remove query with icon button (customLinksEnabled=false)', async () => {
    await addTiles(
        [{
          title: 'title',
          titleDirection: mojoBase.mojom.TextDirection.LEFT_TO_RIGHT,
          url: {url: 'https://search-url/'},
          source: 0,
          titleSource: 0,
          isQueryTile: true,
          dataGenerationTime: {internalValue: BigInt(0)},
        }],
        /* customLinksEnabled */ false);
    const removeButton = queryTiles()[0].querySelector('#removeButton');
    const deleteCalled = handler.whenCalled('deleteMostVisitedTile');
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
    const deleteCalled = handler.whenCalled('deleteMostVisitedTile');
    assertFalse(mostVisited.$.toast.open);
    keydown(tile, 'Delete');
    assertEquals('https://a/', (await deleteCalled).url);
    assertTrue(mostVisited.$.toast.open);
  });

  test('ctrl+z triggers undo and hides toast', async () => {
    const {toast} = mostVisited.$;
    assertFalse(toast.open);
    mostVisited.toast_('linkRemovedMsg', /* showButtons= */ true);
    await flushTasks();
    assertTrue(toast.open);
    const undoCalled = handler.whenCalled('undoMostVisitedTileAction');
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
    mostVisited.toast_('linkRemovedMsg', /* showButtons= */ false);
    await flushTasks();
    assertTrue(toast.open);
    mostVisited.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      ctrlKey: !isMac,
      key: 'z',
      metaKey: isMac,
    }));
    assertEquals(0, handler.getCallCount('undoMostVisitedTileAction'));
    assertTrue(toast.open);
  });

  test('toast restore defaults button', async () => {
    const wait = handler.whenCalled('restoreMostVisitedDefaults');
    const {toast} = mostVisited.$;
    assertFalse(toast.open);
    mostVisited.toast_('linkRemovedMsg', /* showButtons= */ true);
    await flushTasks();
    assertTrue(toast.open);
    toast.querySelector('#restore').click();
    await wait;
    assertFalse(toast.open);
  });

  test('toast undo button', async () => {
    const wait = handler.whenCalled('undoMostVisitedTileAction');
    const {toast} = mostVisited.$;
    assertFalse(toast.open);
    mostVisited.toast_('linkRemovedMsg', /* showButtons= */ true);
    await flushTasks();
    assertTrue(toast.open);
    toast.querySelector('#undo').click();
    await wait;
    assertFalse(toast.open);
  });

  test('drag first tile to second position', async () => {
    await addTiles(2);
    const [first, second] = queryTiles();
    assertEquals('https://a/', first.href);
    assertTrue(first.draggable);
    assertEquals('https://b/', second.href);
    assertTrue(second.draggable);
    const firstRect = first.getBoundingClientRect();
    const secondRect = second.getBoundingClientRect();
    first.dispatchEvent(new DragEvent('dragstart', {
      clientX: firstRect.x + firstRect.width / 2,
      clientY: firstRect.y + firstRect.height / 2,
    }));
    await flushTasks();
    const reorderCalled = handler.whenCalled('reorderMostVisitedTile');
    document.dispatchEvent(new DragEvent('dragend', {
      clientX: secondRect.x + 1,
      clientY: secondRect.y + 1,
    }));
    const [url, newPos] = await reorderCalled;
    assertEquals('https://a/', url.url);
    assertEquals(1, newPos);
    const [newFirst, newSecond] = queryTiles();
    assertEquals('https://b/', newFirst.href);
    assertEquals('https://a/', newSecond.href);
  });

  test('drag second tile to first position', async () => {
    await addTiles(2);
    const [first, second] = queryTiles();
    assertEquals('https://a/', first.href);
    assertTrue(first.draggable);
    assertEquals('https://b/', second.href);
    assertTrue(second.draggable);
    const firstRect = first.getBoundingClientRect();
    const secondRect = second.getBoundingClientRect();
    second.dispatchEvent(new DragEvent('dragstart', {
      clientX: secondRect.x + secondRect.width / 2,
      clientY: secondRect.y + secondRect.height / 2,
    }));
    await flushTasks();
    const reorderCalled = handler.whenCalled('reorderMostVisitedTile');
    document.dispatchEvent(new DragEvent('dragend', {
      clientX: firstRect.x + 1,
      clientY: firstRect.y + 1,
    }));
    const [url, newPos] = await reorderCalled;
    assertEquals('https://b/', url.url);
    assertEquals(0, newPos);
    const [newFirst, newSecond] = queryTiles();
    assertEquals('https://b/', newFirst.href);
    assertEquals('https://a/', newSecond.href);
  });

  test('most visited tiles cannot be reordered', async () => {
    await addTiles(2, /* customLinksEnabled= */ false);
    const [first, second] = queryTiles();
    assertEquals('https://a/', first.href);
    assertTrue(first.draggable);
    assertEquals('https://b/', second.href);
    assertTrue(second.draggable);

    const firstRect = first.getBoundingClientRect();
    const secondRect = second.getBoundingClientRect();
    first.dispatchEvent(new DragEvent('dragstart', {
      clientX: firstRect.x + firstRect.width / 2,
      clientY: firstRect.y + firstRect.height / 2,
    }));
    document.dispatchEvent(new DragEvent('dragend', {
      clientX: secondRect.x + 1,
      clientY: secondRect.y + 1,
    }));
    await flushTasks();
    assertEquals(0, handler.getCallCount('reorderMostVisitedTile'));
    const [newFirst, newSecond] = queryTiles();
    assertEquals('https://a/', newFirst.href);
    assertEquals('https://b/', newSecond.href);
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

  test('add shortcut white', () => {
    assertStyle(
        mostVisited.$.addShortcutIcon, 'background-color', 'rgb(32, 33, 36)');
    mostVisited.toggleAttribute('use-white-add-icon', true);
    assertStyle(
        mostVisited.$.addShortcutIcon, 'background-color',
        'rgb(255, 255, 255)');
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
        await handler.whenCalled('onMostVisitedTileNavigation');
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
    handler.resetResolver('updateMostVisitedInfo');

    // Act.
    document.dispatchEvent(new Event('visibilitychange'));

    // Assert.
    assertEquals(1, handler.getCallCount('updateMostVisitedInfo'));
  });
});
