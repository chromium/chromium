// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksFolderNodeElement, SelectFolderAction} from 'chrome://bookmarks/bookmarks.js';
import {ACCOUNT_HEADING_NODE_ID, LOCAL_HEADING_NODE_ID, ROOT_NODE_ID, selectFolder} from 'chrome://bookmarks/bookmarks.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestCommandManager} from './test_command_manager.js';
import {TestStore} from './test_store.js';
import {createFolder, createItem, findFolderNode, getAllFoldersOpenState, replaceBody, testTree} from './test_util.js';

suite('<bookmarks-folder-node>', function() {
  let rootNode: BookmarksFolderNodeElement;
  let store: TestStore;

  function getFolderNode(id: string): BookmarksFolderNodeElement|undefined {
    return findFolderNode(rootNode, id);
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
    store.replaceSingleton();

    rootNode = document.createElement('bookmarks-folder-node');
    rootNode.itemId = ROOT_NODE_ID;
    rootNode.depth = -1;
    replaceBody(rootNode);
    return microtasksFinished();
  });

  test('selecting and deselecting folders dispatches action', async () => {
    const rootFolders =
        rootNode.shadowRoot.querySelectorAll('bookmarks-folder-node');
    const firstGen = rootFolders[0]!.$['descendants'].querySelectorAll(
        'bookmarks-folder-node');
    const secondGen =
        firstGen[0]!.$['descendants'].querySelectorAll('bookmarks-folder-node');

    // Select nested folder.
    secondGen[0]!.$['container'].click();
    assertTrue(!!store.lastAction);
    assertEquals('select-folder', store.lastAction.name);
    assertEquals(
        secondGen[0]!.itemId, (store.lastAction as SelectFolderAction).id);

    // Select folder in a separate subtree.
    rootFolders[1]!.$['container'].click();
    assertEquals('select-folder', store.lastAction.name);
    assertEquals(
        rootFolders[1]!.itemId, (store.lastAction as SelectFolderAction).id);

    // Doesn't re-select if the folder is already selected.
    store.data.selectedFolder = '7';
    store.notifyObservers();
    await microtasksFinished();
    store.resetLastAction();

    rootFolders[1]!.$['container'].click();
    assertEquals(null, store.lastAction);
  });

  test('depth calculation', function() {
    const rootFolders =
        rootNode.shadowRoot.querySelectorAll('bookmarks-folder-node');
    const firstGen = rootFolders[0]!.$['descendants'].querySelectorAll(
        'bookmarks-folder-node');
    const secondGen =
        firstGen[0]!.$['descendants'].querySelectorAll('bookmarks-folder-node');

    Array.prototype.forEach.call(rootFolders, function(f) {
      assertEquals(0, f.depth);
      assertEquals('0', f.style.getPropertyValue('--node-depth'));
    });
    Array.prototype.forEach.call(firstGen, function(f) {
      assertEquals(1, f.depth);
      assertEquals('1', f.style.getPropertyValue('--node-depth'));
    });
    Array.prototype.forEach.call(secondGen, function(f) {
      assertEquals(2, f.depth);
      assertEquals('2', f.style.getPropertyValue('--node-depth'));
    });
  });

  test('doesn\'t highlight selected folder while searching', async () => {
    const rootFolders =
        rootNode.shadowRoot.querySelectorAll('bookmarks-folder-node');

    assertEquals('1', rootFolders[0]!.itemId);
    assertTrue(rootFolders[0]!.$.container.hasAttribute('selected'));

    store.data.search = {
      term: 'test',
      inProgress: false,
      results: ['3'],
    };
    store.notifyObservers();
    await microtasksFinished();

    assertFalse(rootFolders[0]!.$.container.hasAttribute('selected'));
  });

  test('last visible descendant', async () => {
    assertEquals('7', rootNode.getLastVisibleDescendant().itemId);
    assertEquals('4', getFolderNode('1')!.getLastVisibleDescendant().itemId);

    store.data.folderOpenState.set('2', false);
    store.notifyObservers();
    await microtasksFinished();

    assertEquals('2', getFolderNode('1')!.getLastVisibleDescendant().itemId);
  });

  test('non-permanent folders are hidden by default', async () => {
    store.data.folderOpenState = new Map();
    store.notifyObservers();
    await microtasksFinished();

    assertTrue(getFolderNode('0')!.isOpen);
    assertFalse(getFolderNode('1')!.isOpen);
    assertFalse(getFolderNode('7')!.isOpen);
  });

  test(
      'local folders are collapsed by default with account and local nodes',
      async () => {
        // This creates the following structure:
        //
        // Root node
        // -- Account Heading
        // ---- Account Bookmarks Bar ('5')
        // ------ Folder ('7')
        // ------ Folder ('8')
        // ---- Account Other Node ('6')
        // -- Local Heading
        // ---- Bookmarks Bar ('1')
        // ------ Folder ('3')
        // ------ Folder ('4')
        // ---- Other Node ('2')

        store = new TestStore({
          nodes: testTree(
              createFolder(
                  ACCOUNT_HEADING_NODE_ID,
                  [
                    createFolder(
                        '5',
                        [
                          createFolder('7', [], {syncing: true}),
                          createFolder('8', [], {syncing: true}),
                        ],
                        {
                          folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
                          syncing: true,
                        }),
                    createFolder('6', [], {
                      folderType: chrome.bookmarks.FolderType.OTHER,
                      syncing: true,
                    }),
                  ],
                  {syncing: true}),
              createFolder(
                  LOCAL_HEADING_NODE_ID,
                  [
                    createFolder(
                        '1',
                        [
                          createFolder('3', []),
                          createFolder('4', []),
                        ],
                        {
                          folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
                        }),
                    createFolder(
                        '2', [],
                        {folderType: chrome.bookmarks.FolderType.OTHER}),
                  ],
                  )),
        });

        store.replaceSingleton();
        replaceBody(rootNode);
        await microtasksFinished();

        assertTrue(getFolderNode(ROOT_NODE_ID)!.isOpen);
        assertTrue(getFolderNode(ACCOUNT_HEADING_NODE_ID)!.isOpen);
        assertFalse(getFolderNode('5')!.isOpen);
        assertFalse(getFolderNode('6')!.isOpen);
        assertFalse(getFolderNode(LOCAL_HEADING_NODE_ID)!.isOpen);
      });

  test('get node parent', function() {
    assertEquals(
        getFolderNode(ROOT_NODE_ID), getFolderNode('1')!.getParentFolderNode());
    assertEquals(getFolderNode('2'), getFolderNode('4')!.getParentFolderNode());
    assertEquals(null, getFolderNode(ROOT_NODE_ID)!.getParentFolderNode());
  });

  test('next/previous folder nodes', async () => {
    function getNextChild(parentId: string, targetId: string, reverse: boolean):
        BookmarksFolderNodeElement|null {
      return getFolderNode(parentId)!.getNextChild(
          reverse, getFolderNode(targetId)!);
    }

    // Initially open the tree up to two levels.
    store.data.folderOpenState.set('1', true);
    store.data.folderOpenState.set('2', true);
    store.notifyObservers();
    await microtasksFinished();

    // Forwards.
    assertEquals('4', getNextChild('2', '3', false)!.itemId);
    assertEquals(null, getNextChild('2', '4', false));

    // Backwards.
    assertEquals(null, getNextChild('1', '2', true));
    assertEquals('3', getNextChild('2', '4', true)!.itemId);
    assertEquals('4', getNextChild(ROOT_NODE_ID, '7', true)!.itemId);

    // Skips closed folders.
    store.data.folderOpenState.set('2', false);
    store.notifyObservers();
    await microtasksFinished();

    assertEquals(null, getNextChild('1', '2', false));
    assertEquals('2', getNextChild(ROOT_NODE_ID, '7', true)!.itemId);
  });

  test('right click opens context menu', function() {
    const testCommandManager = new TestCommandManager();
    document.body.appendChild(testCommandManager.getCommandManager());

    const node = getFolderNode('2')!;
    node.$.container.dispatchEvent(new MouseEvent('contextmenu'));

    assertDeepEquals(selectFolder('2'), store.lastAction);
    testCommandManager.assertMenuOpenForIds(['2']);
  });
});
