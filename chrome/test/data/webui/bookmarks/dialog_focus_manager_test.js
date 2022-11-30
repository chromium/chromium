// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogFocusManager, MenuSource} from 'chrome://bookmarks/bookmarks.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestCommandManager} from './test_command_manager.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, getAllFoldersOpenState, replaceBody, testTree} from './test_util.js';

suite('DialogFocusManager', function() {
  let list;
  let store;
  let items;
  let commandManager;
  let dialogFocusManager;

  function keydown(el, key) {
    keyDownOn(el, '', '', key);
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

    commandManager = new TestCommandManager().getCommandManager();
    document.body.appendChild(commandManager);

    dialogFocusManager = new DialogFocusManager();
    DialogFocusManager.setInstance(dialogFocusManager);
  });

  test('restores focus on dialog dismissal', async function() {
    const focusedItem = items[0];
    focusedItem.focus();
    assertEquals(focusedItem, dialogFocusManager.getFocusedElement_());

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    const dropdown = commandManager.$.dropdown.getIfExists();

    assertTrue(dropdown.open);
    assertNotEquals(focusedItem, dialogFocusManager.getFocusedElement_());

    keydown(dropdown, 'Escape');
    assertFalse(dropdown.open);

    await Promise.all([
      eventToPromise('close', dropdown),
      eventToPromise('focus', focusedItem),
    ]);

    assertEquals(focusedItem, dialogFocusManager.getFocusedElement_());
  });

  test('restores focus after stacked dialogs', async () => {
    const focusedItem = items[0];
    focusedItem.focus();
    assertEquals(focusedItem, dialogFocusManager.getFocusedElement_());

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    const dropdown = commandManager.$.dropdown.getIfExists();
    dropdown.close();
    assertNotEquals(focusedItem, dialogFocusManager.getFocusedElement_());

    const editDialog = commandManager.$.editDialog.get();
    editDialog.showEditDialog(store.data.nodes['2']);

    await eventToPromise('close', dropdown);
    editDialog.onCancelButtonTap_();
    assertNotEquals(focusedItem, dialogFocusManager.getFocusedElement_());

    await eventToPromise('close', editDialog);
    assertEquals(focusedItem, dialogFocusManager.getFocusedElement_());
  });

  test('restores focus after multiple shows of same dialog', async () => {
    let focusedItem = items[0];
    focusedItem.focus();
    assertEquals(focusedItem, dialogFocusManager.getFocusedElement_());

    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);
    assertNotEquals(focusedItem, dialogFocusManager.getFocusedElement_());
    const dropdown = commandManager.$.dropdown.getIfExists();
    dropdown.close();

    focusedItem = items[3];
    focusedItem.focus();
    commandManager.openCommandMenuAtPosition(0, 0, MenuSource.ITEM);

    await eventToPromise('close', dropdown);
    assertTrue(dropdown.open);
    dropdown.close();
    assertNotEquals(focusedItem, dialogFocusManager.getFocusedElement_());

    await eventToPromise('close', dropdown);
    assertEquals(focusedItem, dialogFocusManager.getFocusedElement_());
  });
});
