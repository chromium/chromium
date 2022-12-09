// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Command} from 'chrome://bookmarks/bookmarks.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {getDeepActiveElement} from 'chrome://resources/js/util_ts.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestCommandManager} from './test_command_manager.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, normalizeIterable, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-list>', function() {
  let list;
  let store;
  let items;
  let testCommandManager;
  const multiKey = isMac ? 'meta' : 'ctrl';

  function keydown(item, key, modifiers) {
    keyDownOn(item, '', modifiers, key);
  }

  /**
   * @param {string} id
   * @return {!HTMLElement}
   */
  function getItem(id) {
    const item = Array.from(items).find(({itemId}) => itemId === id);
    assert(item, `Item ${id} does not exist in items.`);
    return item;
  }

  /** @param {string} id */
  function selectAndFocus(id) {
    getItem(id).focus();
    store.data.selection.items = new Set([id]);
    store.notifyObservers();
  }

  /** @param {!Array<string>} ids */
  function updateIds(ids) {
    store.data.nodes[store.data.selectedFolder].children = ids;
    store.notifyObservers();
  }

  /**
   * @param {!function} fn
   * @return {!Promise}
   */
  async function doAndWait(fn) {
    fn();
    await flushTasks();
    // Focus is done asynchronously.
    await flushTasks();
  }

  /** @param {string} id */
  function checkMenuButtonFocus(id) {
    assertEquals(getItem(id).$.menuButton, getDeepActiveElement());
  }

  setup(function() {
    const nodes = testTree(createFolder('1', [
      createItem('2'),
      createItem('3'),
      createItem('4'),
      createItem('5'),
      createItem('6'),
      createFolder('7', []),
    ]));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: '1',
    });
    store.setReducersEnabled(true);
    store.replaceSingleton();

    list = document.createElement('bookmarks-list');
    list.style.height = '100%';
    list.style.width = '100%';
    list.style.position = 'absolute';
    replaceBody(list);
    flush();
    items = list.root.querySelectorAll('bookmarks-item');

    testCommandManager = new TestCommandManager();
    document.body.appendChild(testCommandManager.getCommandManager());
  });

  test('simple keyboard selection', function() {
    let focusedItem = items[0];
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertEquals(0, focusedItem.$.menuButton.tabIndex);
    focusedItem.focus();

    keydown(focusedItem, 'ArrowDown');
    focusedItem = items[1];
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertEquals(0, focusedItem.$.menuButton.tabIndex);
    assertDeepEquals(['3'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowUp');
    focusedItem = items[0];
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowRight');
    focusedItem = items[0];
    assertEquals(items[0], document.activeElement.root.activeElement);
    assertEquals(items[0].$.menuButton, items[0].root.activeElement);

    keydown(focusedItem, 'ArrowLeft');
    focusedItem = items[0];
    assertEquals(items[0], document.activeElement.root.activeElement);
    assertEquals(null, items[0].root.activeElement);

    keydown(focusedItem, 'End');
    focusedItem = items[5];
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertDeepEquals(['7'], normalizeIterable(store.data.selection.items));

    // Moving past the end of the list is a no-op.
    keydown(focusedItem, 'ArrowDown');
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertDeepEquals(['7'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'Home');
    focusedItem = items[0];
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    // Moving past the start of the list is a no-op.
    keydown(focusedItem, 'ArrowUp');
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'Escape');
    assertDeepEquals([], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'a', multiKey);
    assertDeepEquals(
        ['2', '3', '4', '5', '6', '7'],
        normalizeIterable(store.data.selection.items));
  });

  test('shift selection', function() {
    let focusedItem = items[0];
    focusedItem.focus();

    keydown(focusedItem, 'ArrowDown', 'shift');
    focusedItem = items[1];
    assertDeepEquals(['2', '3'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'Escape');
    focusedItem = items[1];
    assertDeepEquals([], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowUp', 'shift');
    focusedItem = items[0];
    assertDeepEquals(['2', '3'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', 'shift');
    focusedItem = items[1];
    assertDeepEquals(['3'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', 'shift');
    focusedItem = items[2];
    assertDeepEquals(['3', '4'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'End', 'shift');
    focusedItem = items[2];
    assertDeepEquals(
        ['3', '4', '5', '6', '7'],
        normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'Home', 'shift');
    focusedItem = items[2];
    assertDeepEquals(['2', '3'], normalizeIterable(store.data.selection.items));
  });

  test('ctrl selection', function() {
    let focusedItem = items[0];
    focusedItem.focus();

    keydown(focusedItem, ' ', multiKey);
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', multiKey);
    focusedItem = items[1];
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));
    assertEquals('3', store.data.selection.anchor);

    keydown(focusedItem, 'ArrowDown', multiKey);
    focusedItem = items[2];
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, ' ', multiKey);
    assertDeepEquals(['2', '4'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, ' ', multiKey);
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));
  });

  test('ctrl+shift selection', function() {
    let focusedItem = items[0];
    focusedItem.focus();

    keydown(focusedItem, ' ', multiKey);
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', multiKey);
    focusedItem = items[1];
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', multiKey);
    focusedItem = items[2];
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', [multiKey, 'shift']);
    focusedItem = items[3];
    assertDeepEquals(
        ['2', '4', '5'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', [multiKey, 'shift']);
    focusedItem = items[3];
    assertDeepEquals(
        ['2', '4', '5', '6'], normalizeIterable(store.data.selection.items));
  });

  test('keyboard commands are passed to command manager', function() {
    chrome.bookmarkManagerPrivate.removeTrees = function() {};

    store.data.selection.items = new Set(['2', '3']);
    store.notifyObservers();

    const focusedItem = items[4];
    focusedItem.focus();

    keydown(focusedItem, 'Delete');
    // Commands should take affect on the selection, even if something else is
    // focused.
    testCommandManager.assertLastCommand(Command.DELETE, ['2', '3']);
  });

  test('iron-list does not steal focus on enter', async () => {
    // Iron-list attempts to focus the whole <bookmarks-item> when pressing
    // enter on the menu button. This checks that we block this behavior
    // during keydown on <bookmarks-list>.
    const button = items[0].$.menuButton;
    button.focus();
    keydown(button, 'Enter');
    testCommandManager.getCommandManager().closeCommandMenu();
    assertEquals(button, items[0].root.activeElement);
  });

  test('remove first item, focus on first item', async () => {
    await doAndWait(() => {
      selectAndFocus('2');
      updateIds(['3', '4', '5', '6', '7']);
    });
    checkMenuButtonFocus('3');
  });

  test('remove last item, focus on last item', async () => {
    await doAndWait(() => {
      selectAndFocus('7');
      updateIds(['2', '3', '4', '5', '6']);
    });
    checkMenuButtonFocus('6');
  });

  test('remove middle item, focus on item with same index', async () => {
    await doAndWait(() => {
      selectAndFocus('3');
      updateIds(['2', '4', '5', '6', '7']);
    });
    checkMenuButtonFocus('4');
  });

  test('reorder items, focus does not change', async () => {
    await doAndWait(() => {
      selectAndFocus('3');
      updateIds(['2', '4', '5', '6', '3', '7']);
    });
    assertEquals(document.body, getDeepActiveElement());
  });
});
