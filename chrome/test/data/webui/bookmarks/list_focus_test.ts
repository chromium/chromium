// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksItemElement, BookmarksListElement} from 'chrome://bookmarks/bookmarks.js';
import {BookmarkManagerApiProxyImpl, Command} from 'chrome://bookmarks/bookmarks.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import type {ModifiersParam} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBookmarkManagerApiProxy} from './test_bookmark_manager_api_proxy.js';
import {TestCommandManager} from './test_command_manager.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, normalizeIterable, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-list>', function() {
  let list: BookmarksListElement;
  let store: TestStore;
  let items: NodeListOf<BookmarksItemElement>;
  let testCommandManager: TestCommandManager;
  const multiKey = isMac ? 'meta' : 'ctrl';

  function keydown(item: HTMLElement, key: string, modifiers?: ModifiersParam) {
    keyDownOn(item, 0, modifiers, key);
  }

  function getItem(id: string): BookmarksItemElement {
    const item = Array.from(items).find(({itemId}) => itemId === id);
    assertTrue(!!item, `Item ${id} does not exist in items.`);
    return item;
  }

  async function selectAndFocus(id: string) {
    getItem(id).focus();
    await microtasksFinished();
    store.data.selection.items = new Set([id]);
    store.notifyObservers();
    return microtasksFinished();
  }

  async function updateIds(ids: string[]) {
    store.data.nodes[store.data.selectedFolder]!.children = ids;
    store.notifyObservers();
    await eventToPromise('items-rendered', list.$.list);
    await microtasksFinished();
  }

  function checkMenuButtonFocus(id: string) {
    assertEquals(getItem(id).$.menuButton, getDeepActiveElement());
  }

  setup(async function() {
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

    const proxy = new TestBookmarkManagerApiProxy();
    BookmarkManagerApiProxyImpl.setInstance(proxy);

    list = document.createElement('bookmarks-list');
    list.style.height = '100%';
    list.style.width = '100%';
    list.style.position = 'absolute';
    replaceBody(list);
    await eventToPromise('viewport-filled', list.$.list);
    await microtasksFinished();
    items = list.shadowRoot.querySelectorAll('bookmarks-item');

    testCommandManager = new TestCommandManager();
    document.body.appendChild(testCommandManager.getCommandManager());

    const toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);

    return microtasksFinished();
  });

  test('simple keyboard selection', async () => {
    assertEquals(6, items.length);

    let focusedItem = items[0]!;
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertEquals(0, focusedItem.$.menuButton.tabIndex);
    focusedItem.focus();

    keydown(focusedItem, 'ArrowDown');
    await microtasksFinished();
    items = list.shadowRoot.querySelectorAll('bookmarks-item');
    focusedItem = items[1]!;
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertEquals(0, focusedItem.$.menuButton.tabIndex);
    assertDeepEquals(['3'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowUp');
    await microtasksFinished();
    focusedItem = items[0]!;
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowRight');
    await microtasksFinished();
    focusedItem = items[0]!;
    assertEquals(
        focusedItem, document.activeElement!.shadowRoot!.activeElement);
    assertEquals(focusedItem.$.menuButton, items[0]!.shadowRoot.activeElement);

    keydown(focusedItem, 'ArrowLeft');
    await microtasksFinished();
    focusedItem = items[0]!;
    assertEquals(
        focusedItem, document.activeElement!.shadowRoot!.activeElement);
    assertEquals(null, items[0]!.shadowRoot.activeElement);

    keydown(focusedItem, 'End');
    await microtasksFinished();
    focusedItem = items[5]!;
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertDeepEquals(['7'], normalizeIterable(store.data.selection.items));

    // Moving past the end of the list is a no-op.
    keydown(focusedItem, 'ArrowDown');
    await microtasksFinished();
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertDeepEquals(['7'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'Home');
    await microtasksFinished();
    focusedItem = items[0]!;
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    // Moving past the start of the list is a no-op.
    keydown(focusedItem, 'ArrowUp');
    await microtasksFinished();
    assertEquals('0', focusedItem.getAttribute('tabindex'));
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'Escape');
    await microtasksFinished();
    assertDeepEquals([], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'a', multiKey);
    await microtasksFinished();
    assertDeepEquals(
        ['2', '3', '4', '5', '6', '7'],
        normalizeIterable(store.data.selection.items));
  });

  test('shift selection', async () => {
    assertEquals(6, items.length);

    let focusedItem = items[0]!;
    focusedItem.focus();

    keydown(focusedItem, 'ArrowDown', 'shift');
    await microtasksFinished();
    focusedItem = items[1]!;
    assertDeepEquals(['2', '3'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'Escape');
    await microtasksFinished();
    focusedItem = items[1]!;
    assertDeepEquals([], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowUp', 'shift');
    await microtasksFinished();
    focusedItem = items[0]!;
    assertDeepEquals(['2', '3'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', 'shift');
    await microtasksFinished();
    focusedItem = items[1]!;
    assertDeepEquals(['3'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', 'shift');
    await microtasksFinished();
    focusedItem = items[2]!;
    assertDeepEquals(['3', '4'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'End', 'shift');
    await microtasksFinished();
    focusedItem = items[2]!;
    assertDeepEquals(
        ['3', '4', '5', '6', '7'],
        normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'Home', 'shift');
    await microtasksFinished();
    focusedItem = items[2]!;
    assertDeepEquals(['2', '3'], normalizeIterable(store.data.selection.items));
  });

  test('ctrl selection', async () => {
    assertEquals(6, items.length);

    let focusedItem = items[0]!;
    focusedItem.focus();
    await microtasksFinished();

    keydown(focusedItem, ' ');
    await microtasksFinished();
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', multiKey);
    await microtasksFinished();
    focusedItem = items[1]!;
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));
    assertEquals('3', store.data.selection.anchor);

    keydown(focusedItem, 'ArrowDown', multiKey);
    await microtasksFinished();
    focusedItem = items[2]!;
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    // Space toggles selection with ctrl key held down.
    keydown(focusedItem, ' ', multiKey);
    await microtasksFinished();
    assertDeepEquals(['2', '4'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, ' ', multiKey);
    await microtasksFinished();
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    // Space toggles selection without ctrl key held down.
    keydown(focusedItem, ' ');
    await microtasksFinished();
    assertDeepEquals(['2', '4'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, ' ');
    await microtasksFinished();
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));
  });

  test('ctrl+shift selection', async () => {
    assertEquals(6, items.length);

    let focusedItem = items[0]!;
    focusedItem.focus();
    await microtasksFinished();

    keydown(focusedItem, ' ');
    await microtasksFinished();
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', multiKey);
    await microtasksFinished();
    focusedItem = items[1]!;
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', multiKey);
    await microtasksFinished();
    focusedItem = items[2]!;
    assertDeepEquals(['2'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', [multiKey, 'shift']);
    await microtasksFinished();
    focusedItem = items[3]!;
    assertDeepEquals(
        ['2', '4', '5'], normalizeIterable(store.data.selection.items));

    keydown(focusedItem, 'ArrowDown', [multiKey, 'shift']);
    await microtasksFinished();
    focusedItem = items[3]!;
    assertDeepEquals(
        ['2', '4', '5', '6'], normalizeIterable(store.data.selection.items));
  });

  test('keyboard commands are passed to command manager', async () => {
    store.data.selection.items = new Set(['2', '3']);
    store.notifyObservers();
    await microtasksFinished();

    const focusedItem = items[4]!;
    focusedItem.focus();
    await microtasksFinished();

    keydown(focusedItem, 'Delete');
    await microtasksFinished();
    // Commands should take affect on the selection, even if something else is
    // focused.
    testCommandManager.assertLastCommand(Command.DELETE, ['2', '3']);
  });

  test('cr-lazy-list does not steal focus on enter', async () => {
    assertTrue(!!items[0]);

    // Iron-list attempts to focus the whole <bookmarks-item> when pressing
    // enter on the menu button. This checks that we block this behavior
    // during keydown on <bookmarks-list>.
    const button = items[0].$.menuButton;
    button.focus();
    keydown(button, 'Enter');
    await microtasksFinished();
    testCommandManager.getCommandManager().closeCommandMenu();
    assertEquals(button, items[0].shadowRoot.activeElement);
  });

  test('remove first item, focus on first item', async () => {
    await selectAndFocus('2');
    await updateIds(['3', '4', '5', '6', '7']);
    checkMenuButtonFocus('3');
  });

  test('remove last item, focus on last item', async () => {
    await selectAndFocus('7');
    await updateIds(['2', '3', '4', '5', '6']);
    checkMenuButtonFocus('6');
  });

  test('remove middle item, focus on item with same index', async () => {
    await selectAndFocus('3');
    await updateIds(['2', '4', '5', '6', '7']);
    checkMenuButtonFocus('4');
  });

  test('reorder items, focus does not change', async () => {
    await selectAndFocus('3');
    await updateIds(['2', '4', '5', '6', '3', '7']);
    assertEquals(document.body, getDeepActiveElement());
  });
});
