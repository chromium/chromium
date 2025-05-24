// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ACCOUNT_HEADING_NODE_ID, canEditNode, canReorderChildren, getDescendants, isRootNode, isRootOrChildOfRoot, LOCAL_HEADING_NODE_ID, removeIdsFromObject, removeIdsFromSet, ROOT_NODE_ID} from 'chrome://bookmarks/bookmarks.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestStore} from './test_store.js';
import {createFolder, createItem, normalizeIterable, testTree} from './test_util.js';

suite('util', function() {
  test('getDescendants collects all children', function() {
    const nodes = testTree(createFolder('1', []), createFolder('2', [
                             createItem('3'),
                             createFolder(
                                 '4',
                                 [
                                   createItem('6'),
                                   createFolder('7', []),
                                 ]),
                             createItem('5'),
                           ]));

    let descendants = getDescendants(nodes, '1');
    assertDeepEquals(['1'], normalizeIterable(descendants));

    descendants = getDescendants(nodes, '4');
    assertDeepEquals(['4', '6', '7'], normalizeIterable(descendants));

    descendants = getDescendants(nodes, '2');
    assertDeepEquals(
        ['2', '3', '4', '5', '6', '7'], normalizeIterable(descendants));

    descendants = getDescendants(nodes, '42');
    assertDeepEquals([], normalizeIterable(descendants));
  });

  test('removeIdsFromObject', function() {
    const obj = {
      '1': true,
      '2': false,
      '4': true,
    };

    const nodes = new Set(['2', '3', '4']);

    const newMap = removeIdsFromObject(obj, nodes);

    assertEquals(undefined, newMap['2']);
    assertEquals(undefined, newMap['4']);
    assertTrue(newMap['1']!);

    // Should not have changed the input object.
    assertFalse(obj['2']);
  });

  test('removeIdsFromSet', function() {
    const set = new Set(['1', '3', '5']);
    const toRemove = new Set(['1', '2', '3']);

    const newSet = removeIdsFromSet(set, toRemove);
    assertDeepEquals(['5'], normalizeIterable(newSet));
  });

  test('canEditNode and canReorderChildren', function() {
    const store = new TestStore({
      nodes: testTree(
          createFolder(
              '1',
              [
                createItem('11', {syncing: true}),
              ],
              {syncing: true}),
          createFolder(
              '4',
              [
                createItem('41', {
                  syncing: false,
                  unmodifiable:
                      chrome.bookmarks.BookmarkTreeNodeUnmodifiable.MANAGED,
                }),
              ],
              {
                syncing: false,
                unmodifiable:
                    chrome.bookmarks.BookmarkTreeNodeUnmodifiable.MANAGED,
              })),
    });

    // The heading nodes are unmodifiable, and their children cannot be
    // reordered.
    assertFalse(canEditNode(store.data, ACCOUNT_HEADING_NODE_ID));
    assertFalse(canReorderChildren(store.data, ACCOUNT_HEADING_NODE_ID));
    assertFalse(canEditNode(store.data, LOCAL_HEADING_NODE_ID));
    assertFalse(canReorderChildren(store.data, LOCAL_HEADING_NODE_ID));

    // Top-level folders are unmodifiable, but their children can be changed.
    assertFalse(canEditNode(store.data, '1'));
    assertTrue(canReorderChildren(store.data, '1'));

    // Managed folders are entirely unmodifiable.
    assertFalse(canEditNode(store.data, '4'));
    assertFalse(canReorderChildren(store.data, '4'));
    assertFalse(canEditNode(store.data, '41'));
    assertFalse(canReorderChildren(store.data, '41'));

    // Regular nodes are modifiable.
    assertTrue(canEditNode(store.data, '11'));
    assertTrue(canReorderChildren(store.data, '11'));

    // When editing is disabled globally, everything is unmodifiable.
    store.data.prefs.canEdit = false;

    assertFalse(canEditNode(store.data, '1'));
    assertFalse(canReorderChildren(store.data, '1'));

    assertFalse(canEditNode(store.data, '41'));
    assertFalse(canReorderChildren(store.data, '41'));

    assertFalse(canEditNode(store.data, '11'));
    assertFalse(canReorderChildren(store.data, '11'));
  });

  test('getDescendants no heading node when only local folders', function() {
    const nodes = testTree(
        createFolder('1', [], {syncing: false}),
        createFolder('2', [], {syncing: false}));

    const descendants = getDescendants(nodes, ROOT_NODE_ID);
    assertDeepEquals([ROOT_NODE_ID, '1', '2'], normalizeIterable(descendants));
    assertDeepEquals(nodes[ROOT_NODE_ID]!.children!, ['1', '2']);
  });

  test('getDescendants no heading node when only account folders', function() {
    const nodes = testTree(
        createFolder('1', [], {syncing: true}),
        createFolder('2', [], {syncing: true}));

    const descendants = getDescendants(nodes, ROOT_NODE_ID);
    assertDeepEquals([ROOT_NODE_ID, '1', '2'], normalizeIterable(descendants));
    assertDeepEquals(nodes[ROOT_NODE_ID]!.children!, ['1', '2']);
  });

  test(
      'getDescendants no heading nodes for syncing user with managed bookmarks',
      function() {
        const nodes = testTree(
            createFolder('1', [], {
              folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
              syncing: true,
            }),
            createFolder('2', [], {
              folderType: chrome.bookmarks.FolderType.MANAGED,
              syncing: false,
            }));

        const descendants = getDescendants(nodes, ROOT_NODE_ID);
        assertDeepEquals(
            [ROOT_NODE_ID, '1', '2'], normalizeIterable(descendants));
        assertDeepEquals(nodes[ROOT_NODE_ID]!.children!, ['1', '2']);
      });

  test(
      'getDescendants heading nodes when both local and account folders',
      function() {
        const nodes = testTree(
            createFolder('1', [], {
              folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
              syncing: true,
            }),
            createFolder('2', [], {
              folderType: chrome.bookmarks.FolderType.OTHER,
              syncing: true,
            }),
            createFolder('3', [], {
              folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
              syncing: false,
            }));

        const descendants = getDescendants(nodes, ROOT_NODE_ID);
        assertDeepEquals(
            [
              ROOT_NODE_ID,
              '1',
              '2',
              '3',
              ACCOUNT_HEADING_NODE_ID,
              LOCAL_HEADING_NODE_ID,
            ],
            normalizeIterable(descendants));
        assertDeepEquals(
            nodes[ROOT_NODE_ID]!.children!,
            [ACCOUNT_HEADING_NODE_ID, LOCAL_HEADING_NODE_ID]);
        assertDeepEquals(nodes[ACCOUNT_HEADING_NODE_ID]!.children!, ['1', '2']);
        assertDeepEquals(nodes[LOCAL_HEADING_NODE_ID]!.children!, ['3']);
      });

  test('isRootNode and isRootOrChildOfRoot when no heading nodes', function() {
    const store = new TestStore({
      nodes: testTree(createFolder(
          '1', [createItem('11', {syncing: false})], {syncing: false})),
    });

    assertTrue(isRootNode(ROOT_NODE_ID));
    assertFalse(isRootNode('1'));
    assertFalse(isRootNode('11'));

    assertTrue(isRootOrChildOfRoot(store.data, ROOT_NODE_ID));
    assertTrue(isRootOrChildOfRoot(store.data, '1'));
    assertFalse(isRootOrChildOfRoot(store.data, '11'));

    // Non-existent nodes return false.
    assertFalse(isRootNode('123456'));
    assertFalse(isRootOrChildOfRoot(store.data, '123456'));
  });

  test('isRootNode and isRootOrChildOfRoot when heading nodes', function() {
    const store = new TestStore({
      nodes: testTree(
          createFolder(
              '1', [createItem('11', {syncing: false})], {syncing: false}),
          createFolder(
              '2', [createItem('21', {syncing: true})], {syncing: true})),
    });

    assertTrue(isRootNode(ROOT_NODE_ID));
    assertTrue(isRootNode(ACCOUNT_HEADING_NODE_ID));
    assertTrue(isRootNode(LOCAL_HEADING_NODE_ID));
    assertFalse(isRootNode('1'));
    assertFalse(isRootNode('11'));
    assertFalse(isRootNode('2'));
    assertFalse(isRootNode('21'));

    assertTrue(isRootOrChildOfRoot(store.data, ROOT_NODE_ID));
    assertTrue(isRootOrChildOfRoot(store.data, ACCOUNT_HEADING_NODE_ID));
    assertTrue(isRootOrChildOfRoot(store.data, LOCAL_HEADING_NODE_ID));
    assertTrue(isRootOrChildOfRoot(store.data, '1'));
    assertTrue(isRootOrChildOfRoot(store.data, '2'));
    assertFalse(isRootOrChildOfRoot(store.data, '11'));
    assertFalse(isRootOrChildOfRoot(store.data, '21'));

    // Non-existent nodes return false.
    assertFalse(isRootNode('123456'));
    assertFalse(isRootOrChildOfRoot(store.data, '123456'));
  });
});
