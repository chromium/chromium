// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ACCOUNT_HEADING_NODE_ID, canEditNode, canReorderChildren, flattenNodes, getDefaultSelectedFolder, getDescendants, isRootNode, isRootOrChildOfRoot, LOCAL_HEADING_NODE_ID, PermanentFolderType, removeIdsFromObject, removeIdsFromSet, ROOT_NODE_ID, searchBookmarks} from 'chrome://bookmarks/bookmarks.js';
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
    assertDeepEquals(
        normalizeIterable([ROOT_NODE_ID, '1', '2']),
        normalizeIterable(descendants));
    assertDeepEquals(nodes[ROOT_NODE_ID]!.children!, ['1', '2']);
  });

  test('getDescendants no heading node when only account folders', function() {
    const nodes = testTree(
        createFolder('1', [], {syncing: true}),
        createFolder('2', [], {syncing: true}));

    const descendants = getDescendants(nodes, ROOT_NODE_ID);
    assertDeepEquals(
        normalizeIterable([ROOT_NODE_ID, '1', '2']),
        normalizeIterable(descendants));
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
            normalizeIterable([ROOT_NODE_ID, '1', '2']),
            normalizeIterable(descendants));
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
            normalizeIterable([
              ROOT_NODE_ID,
              '1',
              '2',
              '3',
              ACCOUNT_HEADING_NODE_ID,
              LOCAL_HEADING_NODE_ID,
            ]),
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

  test('getDefaultSelectedFolder', function() {
    const nodes = testTree(
        createFolder('1', [], {
          syncing: true,
          folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
        }),
        createFolder(
            '2', [],
            {syncing: false, folderType: chrome.bookmarks.FolderType.OTHER}),
        createFolder('11', [], {
          syncing: false,
          folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
        }));

    // Test that the syncing bookmarks bar is favored when both are present.
    assertEquals('1', getDefaultSelectedFolder(nodes));

    const nodesNoAccount = testTree(
        createFolder(
            '2', [],
            {syncing: false, folderType: chrome.bookmarks.FolderType.OTHER}),
        createFolder('11', [], {
          syncing: false,
          folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
        }));

    // Test that the local bookmarks bar is picked if account bar is missing.
    assertEquals('11', getDefaultSelectedFolder(nodesNoAccount));
  });

  test('searchBookmarks', function() {
    const nodes = testTree(createFolder('1', [
      createItem('2', {title: 'Google', url: 'https://www.google.com'}),
      createItem('3', {title: 'Google Mail', url: 'https://mail.google.com'}),
      createItem('4', {title: 'Yahoo Mail', url: 'https://mail.yahoo.com'}),
      createFolder(
          '5',
          [
            createItem('6', {title: 'Gmail', url: 'https://gmail.com'}),
          ]),
      createFolder('7', [], {
        title: 'Google Bookmarks',
        permanentFolderType: PermanentFolderType.kBookmarkBar,
      }),
      createFolder('8', [], {
        title: 'Google Stuff',
      }),
    ]));

    // Empty search term returns empty results.
    assertDeepEquals([], searchBookmarks(nodes, ''));
    assertDeepEquals([], searchBookmarks(nodes, '   '));

    const assertMatchingNodes =
        (searchTerm: string, expected: string[], tree = nodes) => {
          assertDeepEquals(
              expected, searchBookmarks(tree, searchTerm).map(n => n.id));
        };

    // Single term matching title.
    assertMatchingNodes('Google', ['2', '3', '8']);
    assertMatchingNodes('Mail', ['3', '4', '6']);

    // Single term matching URL.
    assertMatchingNodes('google.com', ['2', '3']);
    assertMatchingNodes('gmail', ['6']);

    // Case insensitivity.
    assertMatchingNodes('gOoGlE', ['2', '3', '8']);

    // Tokenized search (multi-word).
    assertMatchingNodes('google mail', ['3']);
    assertMatchingNodes('mail google', ['3']);

    // "mail yahoo" should match "Yahoo Mail" ('4').
    assertMatchingNodes('mail yahoo', ['4']);

    // Token matching across title and URL.
    const nodes2 = testTree(createFolder('1', [
      createItem(
          '2',
          {title: 'Apple Pie Recipe', url: 'https://www.nytimes.com/food'}),
    ]));
    assertMatchingNodes('pie nytimes', ['2'], nodes2);
  });

  test('flattenNodes', function() {
    const nodes = testTree(
        createFolder(
            '1',
            [
              createItem('2', {title: 'Google', url: 'https://www.google.com'}),
              createFolder(
                  '3',
                  [
                    createItem('4', {title: 'Sub Item'}),
                  ],
                  {title: 'Sub Folder'}),
              createItem('5', {title: 'Item 5'}),
            ],
            {
              permanentFolderType: PermanentFolderType.kBookmarkBar,
            }),
        createFolder(
            '6',
            [
              createItem('7', {title: 'Other Item'}),
            ],
            {
              permanentFolderType: PermanentFolderType.kOther,
            }));

    const flat = flattenNodes(nodes);
    // Root node, bookmark bar ('1'), and other bookmarks ('6') should be
    // excluded. User folders ('3') and bookmark items ('2', '4', '5', '7')
    // should be included in pre-order.
    assertDeepEquals(['2', '3', '4', '5', '7'], flat.map(n => n.id));

    // Custom baseId for a subtree.
    const subtree = flattenNodes(nodes, '3');
    assertDeepEquals(['3', '4'], subtree.map(n => n.id));

    // Non-existent baseId returns empty array.
    assertDeepEquals([], flattenNodes(nodes, '999'));

    // Empty NodeMap returns empty array.
    assertDeepEquals([], flattenNodes({}));
  });

  test('flattenNodes with account and local heading nodes', function() {
    const nodes = testTree(
        createFolder('1', [createItem('11', {syncing: true})], {
          folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
          syncing: true,
        }),
        createFolder('2', [createItem('21', {syncing: false})], {
          folderType: chrome.bookmarks.FolderType.BOOKMARKS_BAR,
          syncing: false,
        }));

    const flat = flattenNodes(nodes);
    assertDeepEquals(['11', '21'], flat.map(n => n.id));
  });
});
