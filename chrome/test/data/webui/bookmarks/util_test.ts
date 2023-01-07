// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {canEditNode, canReorderChildren, getDescendants, removeIdsFromObject, removeIdsFromSet} from 'chrome://bookmarks/bookmarks.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestStore} from './test_store.js';
import {createFolder, createItem, normalizeIterable, testTree} from './test_util.js';

suite('util', function() {
  test('getDescendants collects all children', function() {
    const nodes = testTree(createFolder('0', [
      createFolder('1', []),
      createFolder(
          '2',
          [
            createItem('3'),
            createFolder(
                '4',
                [
                  createItem('6'),
                  createFolder('7', []),
                ]),
            createItem('5'),
          ]),
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
                createItem('11'),
              ]),
          createFolder(
              '4',
              [
                createItem('41', {
                  unmodifiable:
                      chrome.bookmarks.BookmarkTreeNodeUnmodifiable.MANAGED,
                }),
              ],
              {
                unmodifiable:
                    chrome.bookmarks.BookmarkTreeNodeUnmodifiable.MANAGED,
              })),
    });

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
});
