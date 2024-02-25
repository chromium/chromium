// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for action creators that depend on the page state
 * and/or have non-trivial logic.
 */

import type {SelectItemsAction} from 'chrome://bookmarks/bookmarks.js';
import {ROOT_NODE_ID, selectFolder, selectItem} from 'chrome://bookmarks/bookmarks.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestStore} from './test_store.js';
import {createFolder, createItem, testTree} from './test_util.js';

suite('selectItem', function() {
  let store: TestStore;
  let action: SelectItemsAction;

  setup(function() {
    store = new TestStore({
      nodes: testTree(createFolder(
          '1',
          [
            createItem('2'),
            createItem('8'),
            createFolder('4', []),
            createItem('6'),
          ])),
      selectedFolder: '1',
    });
  });

  test('can select single item', function() {
    action = selectItem('2', store.data, {
      clear: false,
      range: false,
      toggle: false,
    });
    const expected = {
      name: 'select-items',
      items: ['2'],
      clear: false,
      toggle: false,
      anchor: '2',
    };
    assertDeepEquals(expected, action);
  });

  test('can shift-select in regular list', function() {
    store.data.selection.anchor = '2';
    action = selectItem('4', store.data, {
      clear: true,
      range: true,
      toggle: false,
    });

    assertDeepEquals(['2', '8', '4'], action.items);
    // Shift-selection doesn't change anchor.
    assertDeepEquals('2', action.anchor);
  });

  test('can shift-select in search results', function() {
    store.data.selectedFolder = '';
    store.data.search = {
      term: 'test',
      results: ['1', '4', '8'],
      inProgress: false,
    };
    store.data.selection.anchor = '8';

    action = selectItem('4', store.data, {
      clear: true,
      range: true,
      toggle: false,
    });

    assertDeepEquals(['4', '8'], action.items);
  });

  test('selects the item when the anchor is missing', function() {
    // Anchor hasn't been set yet.
    store.data.selection.anchor = null;

    action = selectItem('4', store.data, {
      clear: false,
      range: true,
      toggle: false,
    });
    assertEquals('4', action.anchor);
    assertDeepEquals(['4'], action.items);

    // Anchor set to an item which doesn't exist.
    store.data.selection.anchor = '42';

    action = selectItem('8', store.data, {
      clear: false,
      range: true,
      toggle: false,
    });
    assertEquals('8', action.anchor);
    assertDeepEquals(['8'], action.items);
  });
});

test('selectFolder prevents selecting invalid nodes', function() {
  const nodes = testTree(createFolder('1', [
    createItem('2'),
  ]));

  let action = selectFolder(ROOT_NODE_ID, nodes);
  assertEquals(null, action);

  action = selectFolder('2', nodes);
  assertEquals(null, action);

  action = selectFolder('42', nodes);
  assertEquals(null, action);

  action = selectFolder('1', nodes);
  assertEquals('select-folder', action!.name);
  assertEquals('1', action!.id);
});
