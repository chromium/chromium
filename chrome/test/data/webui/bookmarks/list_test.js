// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl, Command, MenuSource} from 'chrome://bookmarks/bookmarks.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBookmarksBrowserProxy} from './test_browser_proxy.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, customClick, getAllFoldersOpenState, normalizeIterable, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-list>', function() {
  let list;
  let store;

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
    const items = list.root.querySelectorAll('bookmarks-item');
    const ids = Array.from(items).map((item) => item.itemId);

    assertDeepEquals(['1', '3', '5', '7'], ids);
  });

  test('shift-selects multiple items', function() {
    const items = list.root.querySelectorAll('bookmarks-item');

    customClick(items[0]);

    assertEquals('select-items', store.lastAction.name);
    assertTrue(store.lastAction.clear);
    assertEquals('1', store.lastAction.anchor);
    assertDeepEquals(['1'], store.lastAction.items);

    store.data.selection.anchor = '1';
    customClick(items[2], {shiftKey: true, ctrlKey: true});

    assertEquals('select-items', store.lastAction.name);
    assertFalse(store.lastAction.clear);
    assertEquals('1', store.lastAction.anchor);
    assertDeepEquals(['1', '3', '5'], store.lastAction.items);
  });

  test('deselects items on click outside of card', function() {
    customClick(list);
    assertEquals('deselect-items', store.lastAction.name);
  });

  test('adds, deletes, and moves update displayedList_', function() {
    list.displayedIds_ = ['1', '7', '3', '5'];
    assertDeepEquals(list.displayedIds_, list.displayedList_.map(n => n.id));

    list.displayedIds_ = ['1', '3', '5'];
    assertDeepEquals(list.displayedIds_, list.displayedList_.map(n => n.id));

    list.displayedIds_ = ['1', '3', '7', '5'];
    assertDeepEquals(list.displayedIds_, list.displayedList_.map(n => n.id));
  });

  test('selects all valid IDs on highlight-items', function() {
    list.dispatchEvent(new CustomEvent(
        'highlight-items',
        {bubbles: true, composed: true, detail: ['10', '1', '3', '9']}));
    assertEquals('select-items', store.lastAction.name);
    assertEquals('1', store.lastAction.anchor);
    assertDeepEquals(['1', '3'], store.lastAction.items);
  });
});

suite('<bookmarks-list> integration test', function() {
  let list;
  let store;
  let items;

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

    items = list.root.querySelectorAll('bookmarks-item');
  });

  test('shift-selects multiple items', function() {
    customClick(items[1]);
    assertDeepEquals(['3'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('3', store.data.selection.anchor);

    customClick(items[3], {shiftKey: true});
    assertDeepEquals(
        ['3', '5', '7'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('3', store.data.selection.anchor);

    customClick(items[0], {shiftKey: true});
    assertDeepEquals(['1', '3'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('3', store.data.selection.anchor);
  });

  test('ctrl toggles multiple items', function() {
    customClick(items[1]);
    assertDeepEquals(['3'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('3', store.data.selection.anchor);

    customClick(items[3], {ctrlKey: true});
    assertDeepEquals(['3', '7'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('7', store.data.selection.anchor);

    customClick(items[1], {ctrlKey: true});
    assertDeepEquals(['7'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('3', store.data.selection.anchor);
  });

  test('ctrl+shift adds ranges to selection', function() {
    customClick(items[0]);
    assertDeepEquals(['1'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('1', store.data.selection.anchor);

    customClick(items[2], {ctrlKey: true});
    assertDeepEquals(['1', '5'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('5', store.data.selection.anchor);

    customClick(items[4], {ctrlKey: true, shiftKey: true});
    assertDeepEquals(
        ['1', '5', '7', '9'], normalizeIterable(store.data.selection.items));
    assertDeepEquals('5', store.data.selection.anchor);

    customClick(items[0], {ctrlKey: true, shiftKey: true});
    assertDeepEquals(
        ['1', '3', '5', '7', '9'],
        normalizeIterable(store.data.selection.items));
    assertDeepEquals('5', store.data.selection.anchor);
  });
});

suite('<bookmarks-list> command manager integration test', function() {
  let app;
  let store;
  let proxy;

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
        app.shadowRoot.querySelector('bookmarks-command-manager');
    proxy.resetResolver('recordInHistogram');
    const list = app.shadowRoot.querySelector('bookmarks-list');
    list.dispatchEvent(new CustomEvent(
        'contextmenu',
        {bubbles: true, composed: true, detail: {clientX: 0, clientY: 0}}));

    await proxy.whenCalled('recordInHistogram');

    assertEquals(MenuSource.LIST, commandManager.menuSource_);
    assertDeepEquals(
        [Command.ADD_BOOKMARK, Command.ADD_FOLDER],
        commandManager.menuCommands_);
  });
});
