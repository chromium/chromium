// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksFolderNodeElement, SelectFolderAction} from 'chrome://bookmarks/bookmarks.js';
import {BookmarkManagerApiProxyImpl, changeFolderOpen, Command, selectFolder} from 'chrome://bookmarks/bookmarks.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';

import {TestBookmarkManagerApiProxy} from './test_bookmark_manager_api_proxy.js';
import {TestCommandManager} from './test_command_manager.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, findFolderNode, getAllFoldersOpenState, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-folder-node>', function() {
  let rootNode: BookmarksFolderNodeElement;
  let store: TestStore;
  let bookmarkManagerProxy: TestBookmarkManagerApiProxy;

  function getFolderNode(id: string) {
    return findFolderNode(rootNode, id) as BookmarksFolderNodeElement;
  }

  function assertHasFocusAndNotSelected(id: string) {
    const node = getFolderNode(id);
    const activeElement = node.shadowRoot!.activeElement;
    assertTrue(
        activeElement != null && activeElement === getDeepActiveElement());
    const badAction = selectFolder(id);
    assertTrue(!!badAction);
    if (store.lastAction != null && badAction.name === store.lastAction.name) {
      assertNotEquals(
          badAction.id, (store.lastAction as SelectFolderAction).id);
    }
  }

  function keydown(id: string, key: string) {
    keyDownOn(getFolderNode(id).$.container, 0, [], key);
  }

  setup(function() {
    const nodes = testTree(
        createFolder(
            '1',
            [
              createFolder(
                  '2',
                  [
                    createFolder('3', []),
                    createFolder('4', []),
                  ]),
              createItem('5'),
            ]),
        createFolder('7', []));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: '1',
    });
    store.setReducersEnabled(true);
    store.replaceSingleton();

    bookmarkManagerProxy = new TestBookmarkManagerApiProxy();
    BookmarkManagerApiProxyImpl.setInstance(bookmarkManagerProxy);

    rootNode = document.createElement('bookmarks-folder-node');
    rootNode.itemId = '0';
    rootNode.depth = -1;
    replaceBody(rootNode);
    flush();
  });

  test('keyboard selection', function() {
    function assertFocused(oldFocus: string, newFocus: string) {
      assertEquals(
          '-1', getFolderNode(oldFocus).$.container.getAttribute('tabindex'));
      assertEquals(
          '0', getFolderNode(newFocus).$.container.getAttribute('tabindex'));
      assertEquals(
          getFolderNode(newFocus).$.container,
          getFolderNode(newFocus).shadowRoot!.activeElement);
    }

    store.data.folderOpenState.set('2', false);
    store.notifyObservers();

    // The selected folder is focus enabled on attach.
    assertEquals('0', getFolderNode('1').$.container.getAttribute('tabindex'));

    // Only the selected folder should be keyboard focusable.
    assertEquals('-1', getFolderNode('2').$.container.getAttribute('tabindex'));

    store.data.search.term = 'asdf';

    // The selected folder is focus enabled even with a search term.
    assertEquals('0', getFolderNode('1').$.container.getAttribute('tabindex'));

    store.data.search.term = '';

    // Give keyboard focus to the first item.
    getFolderNode('1').$.container.focus();

    // Move down into child.
    keydown('1', 'ArrowDown');
    assertHasFocusAndNotSelected('2');
    keydown('2', ' ');

    assertDeepEquals(selectFolder('2'), store.lastAction);
    store.data.selectedFolder = '2';
    store.notifyObservers();

    assertEquals('-1', getFolderNode('1').$.container.getAttribute('tabindex'));
    assertEquals('0', getFolderNode('2').$.container.getAttribute('tabindex'));
    assertFocused('1', '2');

    // Move down past closed folders.
    keydown('2', 'ArrowDown');
    assertHasFocusAndNotSelected('7');
    keydown('7', ' ');

    assertDeepEquals(selectFolder('7'), store.lastAction);
    assertFocused('2', '7');

    // Move down past end of list.
    store.resetLastAction();
    keydown('7', 'ArrowDown');
    assertDeepEquals(null, store.lastAction);

    // Move up past closed folders.
    keydown('7', 'ArrowUp');
    assertHasFocusAndNotSelected('2');
    keydown('2', ' ');

    assertDeepEquals(selectFolder('2'), store.lastAction);
    assertFocused('7', '2');

    // Move up into parent.
    keydown('2', 'ArrowUp');
    assertHasFocusAndNotSelected('1');
    keydown('1', ' ');
    assertDeepEquals(selectFolder('1'), store.lastAction);
    assertFocused('2', '1');

    // Move up past start of list.
    store.resetLastAction();
    keydown('1', 'ArrowUp');
    assertDeepEquals(null, store.lastAction);
  });

  test('keyboard left/right', function() {
    store.data.folderOpenState.set('2', false);
    store.notifyObservers();

    // Give keyboard focus to the first item.
    getFolderNode('1').$.container.focus();

    // Pressing right descends into first child.
    keydown('1', 'ArrowRight');
    assertHasFocusAndNotSelected('2');
    keydown('2', ' ');

    assertDeepEquals(selectFolder('2'), store.lastAction);

    // Pressing right on a closed folder opens that folder
    keydown('2', 'ArrowRight');
    assertDeepEquals(changeFolderOpen('2', true), store.lastAction);

    // Pressing right again descends into first child.
    keydown('2', 'ArrowRight');
    assertHasFocusAndNotSelected('3');
    keydown('3', ' ');
    assertDeepEquals(selectFolder('3'), store.lastAction);

    // Pressing right on a folder with no children does nothing.
    store.resetLastAction();
    keydown('3', 'ArrowRight');
    assertHasFocusAndNotSelected('3');
    assertDeepEquals(null, store.lastAction);

    // Pressing left on a folder with no children ascends to parent.
    keydown('3', 'ArrowDown');
    keydown('4', 'ArrowLeft');
    assertHasFocusAndNotSelected('2');
    keydown('2', ' ');
    assertDeepEquals(selectFolder('2'), store.lastAction);

    // Pressing left again closes the parent.
    keydown('2', 'ArrowLeft');
    assertDeepEquals(changeFolderOpen('2', false), store.lastAction);

    // RTL does not flip left and right.
    document.body.style.direction = 'rtl';
    keydown('2', 'ArrowRight');
    assertDeepEquals(changeFolderOpen('2', true), store.lastAction);

    keydown('2', 'ArrowLeft');
    assertDeepEquals(changeFolderOpen('2', false), store.lastAction);

    document.body.style.direction = 'ltr';
  });

  test('keyboard commands are passed to command manager', async function() {
    const testCommandManager = new TestCommandManager();
    document.body.appendChild(testCommandManager.getCommandManager());

    const toastManager = document.createElement('cr-toast-manager');
    document.body.appendChild(toastManager);

    store.data.selection.items = new Set(['3', '4']);
    store.data.selectedFolder = '2';
    store.notifyObservers();

    getFolderNode('2').$.container.focus();
    keydown('2', 'Delete');
    await bookmarkManagerProxy.whenCalled('removeTrees');

    testCommandManager.assertLastCommand(Command.DELETE, ['2']);
  });
});
