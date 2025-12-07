// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksCommandManagerElement, BookmarksItemElement, BookmarksListElement} from 'chrome://bookmarks/bookmarks.js';
import {Command, DialogFocusManager, MenuSource} from 'chrome://bookmarks/bookmarks.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestCommandManager} from './test_command_manager.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, replaceBody, testTree} from './test_util.js';


suite('DialogFocusManager', function() {
  let list: BookmarksListElement;
  let store: TestStore;
  let items: NodeListOf<BookmarksItemElement>;
  let commandManager: BookmarksCommandManagerElement;
  let dialogFocusManager: DialogFocusManager;

  function keydown(el: HTMLElement, key: string) {
    keyDownOn(el, 0, [], key);
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

    list = document.createElement('bookmarks-list');
    list.style.height = '100%';
    list.style.width = '100%';
    list.style.position = 'absolute';
    replaceBody(list);
    await eventToPromise('viewport-filled', list.$.list);
    await microtasksFinished();
    items = list.shadowRoot.querySelectorAll('bookmarks-item');

    commandManager = new TestCommandManager().getCommandManager();
    document.body.appendChild(commandManager);

    dialogFocusManager = new DialogFocusManager();
    DialogFocusManager.setInstance(dialogFocusManager);
  });

  test('restores focus on dialog dismissal', async function() {
    const focusedItem = items[0]!;
    focusedItem.focus();
    assertEquals(focusedItem, getDeepActiveElement());

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    await microtasksFinished();
    const dropdown = commandManager.$.dropdown.getIfExists()!;

    assertTrue(dropdown.open);
    assertNotEquals(focusedItem, getDeepActiveElement());

    keydown(dropdown, 'Escape');
    assertFalse(dropdown.open);

    await eventToPromise('close', dropdown);

    assertEquals(focusedItem, getDeepActiveElement());
  });

  test('restores focus after stacked dialogs', async () => {
    const focusedItem = items[0]!;
    focusedItem.focus();
    assertEquals(focusedItem, getDeepActiveElement());

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    await microtasksFinished();
    const dropdown = commandManager.$.dropdown.getIfExists();
    assertTrue(!!dropdown);

    commandManager.handle(Command.EDIT, new Set(['2']));
    await microtasksFinished();
    const editDialog =
        commandManager.shadowRoot.querySelector('bookmarks-edit-dialog');
    assertTrue(!!editDialog);
    dropdown.close();
    assertNotEquals(focusedItem, getDeepActiveElement());

    await eventToPromise('close', dropdown);
    editDialog.$.dialog.cancel();
    assertNotEquals(focusedItem, getDeepActiveElement());

    await eventToPromise('close', editDialog);
    assertEquals(focusedItem, getDeepActiveElement());
  });

  test('restores focus after multiple shows of same dialog', async () => {
    let focusedItem = items[0]!;
    focusedItem.focus();
    assertEquals(focusedItem, getDeepActiveElement());

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    await microtasksFinished();
    assertNotEquals(focusedItem, getDeepActiveElement());
    const dropdown = commandManager.$.dropdown.getIfExists()!;
    dropdown.close();

    focusedItem = items[3]!;
    focusedItem.focus();
    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);

    await eventToPromise('close', dropdown);
    assertTrue(dropdown.open);
    assertNotEquals(focusedItem, getDeepActiveElement());
    dropdown.close();

    await eventToPromise('close', dropdown);
    assertEquals(focusedItem, getDeepActiveElement());
  });
});
