// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksAppElement, BookmarksItemElement, BookmarksListElement, SelectItemsAction} from 'chrome://bookmarks/bookmarks.js';
import {BrowserProxyImpl, Command, MenuSource, removeBookmark} from 'chrome://bookmarks/bookmarks.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestBookmarksBrowserProxy} from './test_browser_proxy.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, customClick, getAllFoldersOpenState, normalizeIterable, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-list>', function() {
  let list: BookmarksListElement;
  let store: TestStore;

  setup(function() {
    const nodes = testTree(createFolder('10', [
      createItem('1'),
      createFolder('3', []),
      createItem('5'),
      createItem('7'),
    ]));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: '10',
    });
    store.replaceSingleton();

    list = document.createElement('bookmarks-list');
    list.style.height = '100%';
    list.style.width = '100%';
    list.style.position = 'absolute';

    replaceBody(list);
    flush();
  });

  test('renders correct <bookmark-item> elements', function() {
    const items = list.shadowRoot!.querySelectorAll('bookmarks-item');
    const ids = Array.from(items).map((item) => item.itemId);

    assertDeepEquals(['1', '3', '5', '7'], ids);
  });

  test('shift-selects multiple items', function() {
    const items = list.shadowRoot!.querySelectorAll('bookmarks-item');

    customClick(items[0]!);

    let lastAction = store.lastAction as SelectItemsAction;

    assertEquals('select-items', lastAction.name);
    assertTrue(lastAction.clear);
    assertEquals('1', lastAction.anchor);
    assertDeepEquals(['1'], lastAction.items);

    store.data.selection.anchor = '1';
    customClick(items[2]!, {shiftKey: true, ctrlKey: true});
    lastAction = store.lastAction as SelectItemsAction;

    assertEquals('select-items', lastAction.name);
    assertFalse(lastAction.clear);
    assertEquals('1', lastAction.anchor);
    assertDeepEquals(['1', '3', '5'], lastAction.items);
  });

  test('deselects items on click outside of card', function() {
    customClick(list);
    const lastAction = store.lastAction as SelectItemsAction;
    assertEquals('deselect-items', lastAction.name);
  });

  test('adds, deletes, and moves update displayedList_', function() {
    list.setDisplayedIdsForTesting(['1', '7', '3', '5']);
    flush();
    let items = list.shadowRoot!.querySelectorAll('bookmarks-item');
    assertDeepEquals(
        ['1', '7', '3', '5'],
        Array.from(items).filter(i => !i.hidden).map(i => i.itemId));

    list.setDisplayedIdsForTesting(['1', '3', '5']);
    flush();
    items = list.shadowRoot!.querySelectorAll('bookmarks-item');
    assertDeepEquals(
        ['1', '3', '5'],
        Array.from(items).filter(i => !i.hidden).map(i => i.itemId));

    list.setDisplayedIdsForTesting(['1', '3', '7', '5']);
    flush();
    items = list.shadowRoot!.querySelectorAll('bookmarks-item');
    assertDeepEquals(
        ['1', '3', '7', '5'],
        Array.from(items).filter(i => !i.hidden).map(i => i.itemId));
  });

  test('selects all valid IDs on highlight-items', function() {
    list.dispatchEvent(new CustomEvent(
        'highlight-items',
        {bubbles: true, composed: true, detail: ['10', '1', '3', '9']}));
    const lastAction = store.lastAction as SelectItemsAction;
    assertEquals('select-items', lastAction.name);
    assertEquals('1', lastAction.anchor);
    assertDeepEquals(['1', '3'], lastAction.items);
  });
});

suite('<bookmarks-list> integration test', function() {
  let list: BookmarksListElement;
  let store: TestStore;
  let items: NodeListOf<BookmarksItemElement>;

  setup(function() {
    store = new TestStore({
      nodes: testTree(createFolder(
          '10',
          [
            createItem('1'),
            createFolder('3', []),
            createItem('5'),
            createItem('7'),
            createItem('9'),
          ])),
      selectedFolder: '10',
    });
    store.replaceSingleton();
    store.setReducersEnabled(true);

    list = document.createElement('bookmarks-list');
    list.style.height = '100%';
    list.style.width = '100%';
    list.style.position = 'absolute';

    replaceBody(list);
    flush();

    items = list.shadowRoot!.querySelectorAll('bookmarks-item');
  });

  test('shift-selects multiple items', function() {
    customClick(items[1]!);
    assertDeepEquals(['3'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('3', store.data.selection.anchor);

    customClick(items[3]!, {shiftKey: true});
    assertDeepEquals(
        ['3', '5', '7'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('3', store.data.selection.anchor);

    customClick(items[0]!, {shiftKey: true});
    assertDeepEquals(['1', '3'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('3', store.data.selection.anchor);
  });

  test('ctrl toggles multiple items', function() {
    customClick(items[1]!);
    assertDeepEquals(['3'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('3', store.data.selection.anchor);

    customClick(items[3]!, {ctrlKey: true});
    assertDeepEquals(['3', '7'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('7', store.data.selection.anchor);

    customClick(items[1]!, {ctrlKey: true});
    assertDeepEquals(['7'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('3', store.data.selection.anchor);
  });

  test('ctrl+shift adds ranges to selection', function() {
    customClick(items[0]!);
    assertDeepEquals(['1'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('1', store.data.selection.anchor);

    customClick(items[2]!, {ctrlKey: true});
    assertDeepEquals(['1', '5'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('5', store.data.selection.anchor);

    customClick(items[4]!, {ctrlKey: true, shiftKey: true});
    assertDeepEquals(
        ['1', '5', '7', '9'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('5', store.data.selection.anchor);

    customClick(items[0]!, {ctrlKey: true, shiftKey: true});
    assertDeepEquals(
        ['1', '3', '5', '7', '9'],
        normalizeIterable(store.data.selection.items));
    assertDeepEquals('5', store.data.selection.anchor);
  });

  // TODO(b/343974530) disable for flaky / unpredictable failures.
  test.skip('delete restores focus on item after anchor', async function() {
    customClick(items[2]!);
    customClick(items[4]!, {ctrlKey: true});
    assertDeepEquals(['5', '9'], normalizeIterable(store.data.selection.items));
    assertEquals('9', store.data.selection.anchor);

    // customClick does not set focus like a real click does.
    list.$.list.focusItem(4);
    await waitAfterNextRender(list);
    assertEquals(
        '9',
        (list.shadowRoot?.activeElement as BookmarksItemElement | null)
            ?.itemId);

    // Simulate user deleting items. Remove actions come in rapidly one at a
    // time via `api_listener.ts` but they are batched together.
    store.beginBatchUpdate();
    for (const item of store.data.selection.items) {
      const parentId = store.data.nodes[item]?.parentId!;
      store.dispatch(removeBookmark(
          item, parentId, store.data.nodes[parentId]?.children?.indexOf(item)!,
          store.data.nodes));
    }
    store.endBatchUpdate();

    // Let `list` update its dom.
    await waitAfterNextRender(list);

    // `list` internally uses setTimeout to trigger focus after deletion. Using
    // `flushTasks` here should force assertions to run after focus has been
    // updated.
    await flushTasks();

    // The element immediately preceding the deleted '9' should now be focused.
    assertEquals(
        '7',
        (list.shadowRoot?.activeElement as BookmarksItemElement | null)
            ?.itemId);
  });
});

suite('<bookmarks-list> command manager integration test', function() {
  let app: BookmarksAppElement;
  let store: TestStore;
  let proxy: TestBookmarksBrowserProxy;

  setup(function() {
    store = new TestStore({
      nodes: testTree(createFolder('1', [])),
      selectedFolder: '1',
    });
    store.replaceSingleton();
    store.setReducersEnabled(true);

    proxy = new TestBookmarksBrowserProxy();
    BrowserProxyImpl.setInstance(proxy);

    app = document.createElement('bookmarks-app');
    app.style.height = '100%';
    app.style.width = '100%';
    app.style.position = 'absolute';

    replaceBody(app);

    flush();
  });

  test('show context menu', async () => {
    const commandManager =
        app.shadowRoot!.querySelector('bookmarks-command-manager')!;
    const list = app.shadowRoot!.querySelector('bookmarks-list')!;
    list.dispatchEvent(new CustomEvent(
        'contextmenu',
        {bubbles: true, composed: true, detail: {clientX: 0, clientY: 0}}));

    assertEquals(MenuSource.LIST, commandManager.getMenuSourceForTesting());
    const menuCommands =
        commandManager.shadowRoot!.querySelectorAll('.dropdown-item');
    assertDeepEquals(
        [Command.ADD_BOOKMARK.toString(), Command.ADD_FOLDER.toString()],
        Array.from(menuCommands).map(el => el.getAttribute('command')));
  });
});
