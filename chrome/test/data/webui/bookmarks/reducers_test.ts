// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksPageState, FolderOpenState, NodeMap, SelectFolderAction, SelectionState, SelectItemsAction} from 'chrome://bookmarks/bookmarks.js';
import {changeFolderOpen, clearSearch, createBookmark, createEmptyState, deselectItems, editBookmark, getDisplayedList, isShowingSearch, moveBookmark, reduceAction, removeBookmark, reorderChildren, selectFolder, setSearchResults, setSearchTerm, updateAnchor, updateFolderOpenState, updateNodes, updateSelectedFolder, updateSelection} from 'chrome://bookmarks/bookmarks.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createFolder, createItem, normalizeIterable, testTree} from './test_util.js';

suite('selection state', function() {
  let selection: SelectionState;
  let action;

  function select(
      items: string[], anchor: string, clear: boolean,
      toggle: boolean): SelectItemsAction {
    return {
      name: 'select-items',
      clear: clear,
      anchor: anchor,
      items: items,
      toggle: toggle,
    };
  }

  setup(function() {
    selection = {
      anchor: null,
      items: new Set(),
    };
  });

  test('can select an item', function() {
    action = select(['1'], '1', true, false);
    selection = updateSelection(selection, action);

    assertDeepEquals(['1'], normalizeIterable(selection.items));
    assertEquals('1', selection.anchor);

    // Replace current selection.
    action = select(['2'], '2', true, false);
    selection = updateSelection(selection, action);
    assertDeepEquals(['2'], normalizeIterable(selection.items));
    assertEquals('2', selection.anchor);

    // Add to current selection.
    action = select(['3'], '3', false, false);
    selection = updateSelection(selection, action);
    assertDeepEquals(['2', '3'], normalizeIterable(selection.items));
    assertEquals('3', selection.anchor);
  });

  test('can select multiple items', function() {
    action = select(['1', '2', '3'], '3', true, false);
    selection = updateSelection(selection, action);
    assertDeepEquals(['1', '2', '3'], normalizeIterable(selection.items));

    action = select(['3', '4'], '4', false, false);
    selection = updateSelection(selection, action);
    assertDeepEquals(['1', '2', '3', '4'], normalizeIterable(selection.items));
  });

  test('is cleared when selected folder changes', function() {
    action = select(['1', '2', '3'], '3', true, false);
    selection = updateSelection(selection, action);

    action = selectFolder('2');
    selection = updateSelection(selection, action!);
    assertDeepEquals(new Set(), selection.items);
  });

  test('is cleared when search finished', function() {
    action = select(['1', '2', '3'], '3', true, false);
    selection = updateSelection(selection, action);

    action = setSearchResults(['2']);
    selection = updateSelection(selection, action);
    assertDeepEquals(new Set(), selection.items);
  });

  test('is cleared when search cleared', function() {
    action = select(['1', '2', '3'], '3', true, false);
    selection = updateSelection(selection, action);

    action = clearSearch();
    selection = updateSelection(selection, action);
    assertDeepEquals(new Set(), selection.items);
  });

  test('deselect items', function() {
    action = select(['1', '2', '3'], '3', true, false);
    selection = updateSelection(selection, action);

    action = deselectItems();
    selection = updateSelection(selection, action);
    assertDeepEquals(new Set(), selection.items);
  });

  test('toggle an item', function() {
    action = select(['1', '2', '3'], '3', true, false);
    selection = updateSelection(selection, action);

    action = select(['1'], '3', false, true);
    selection = updateSelection(selection, action);
    assertDeepEquals(['2', '3'], normalizeIterable(selection.items));
  });

  test('update anchor', function() {
    action = updateAnchor('3');
    selection = updateSelection(selection, action);

    assertEquals('3', selection.anchor);
  });

  test('deselects items when they are deleted', function() {
    const nodeMap = testTree(
        createFolder(
            '1',
            [
              createItem('2'),
              createItem('3'),
              createItem('4'),
            ]),
        createItem('5'));

    action = select(['2', '4', '5'], '4', true, false);
    selection = updateSelection(selection, action);

    action = removeBookmark('1', '0', 0, nodeMap);
    selection = updateSelection(selection, action);

    assertDeepEquals(['5'], normalizeIterable(selection.items));
    assertEquals(null, selection.anchor);
  });

  test('deselects items when they are moved to a different folder', function() {
    action = select(['2', '3'], '2', true, false);
    selection = updateSelection(selection, action);

    // Move item '2' from the 1st item in '0' to the 0th item in '1'.
    action = moveBookmark('2', '1', 0, '0', 1);
    selection = updateSelection(selection, action);

    assertDeepEquals(['3'], normalizeIterable(selection.items));
    assertEquals(null, selection.anchor);
  });
});

suite('folder open state', function() {
  let nodes: NodeMap;
  let folderOpenState: FolderOpenState;
  let action;

  setup(function() {
    nodes = testTree(
        createFolder(
            '1',
            [
              createFolder('2', []),
              createItem('3'),
            ]),
        createFolder('4', []));
    folderOpenState = new Map();
  });

  test('close folder', function() {
    action = changeFolderOpen('2', false);
    folderOpenState = updateFolderOpenState(folderOpenState, action, nodes);
    assertFalse(folderOpenState.has('1'));
    assertFalse(folderOpenState.get('2')!);
  });

  test('select folder with closed parent', function() {
    // Close '1'
    action = changeFolderOpen('1', false);
    folderOpenState = updateFolderOpenState(folderOpenState, action, nodes);
    assertFalse(folderOpenState.get('1')!);
    assertFalse(folderOpenState.has('2'));

    // Should re-open when '2' is selected.
    action = selectFolder('2');
    folderOpenState = updateFolderOpenState(folderOpenState, action!, nodes);
    assertTrue(folderOpenState.get('1')!);
    assertFalse(folderOpenState.has('2'));

    // The parent should be set to permanently open, even if it wasn't
    // explicitly closed.
    folderOpenState = new Map();
    action = selectFolder('2');
    folderOpenState = updateFolderOpenState(folderOpenState, action!, nodes);
    assertTrue(folderOpenState.get('1')!);
    assertFalse(folderOpenState.has('2'));
  });

  test('move nodes in a closed folder', function() {
    // Moving bookmark items should not open folders.
    folderOpenState = new Map([['1', false]]);
    action = moveBookmark('3', '1', 1, '1', 0);
    folderOpenState = updateFolderOpenState(folderOpenState, action, nodes);

    assertFalse(folderOpenState.get('1')!);

    // Moving folders should open their parents.
    folderOpenState = new Map([['1', false], ['2', false]]);
    action = moveBookmark('4', '2', 0, '0', 1);
    folderOpenState = updateFolderOpenState(folderOpenState, action, nodes);
    assertTrue(folderOpenState.get('1')!);
    assertTrue(folderOpenState.get('2')!);
  });
});

suite('selected folder', function() {
  let nodes: NodeMap;
  let selectedFolder: string;
  let action: SelectFolderAction;

  setup(function() {
    nodes = testTree(createFolder('1', [
      createFolder(
          '2',
          [
            createFolder('3', []),
            createFolder('4', []),
          ]),
    ]));

    selectedFolder = '1';
  });

  test('updates from selectFolder action', function() {
    action = selectFolder('2')!;
    selectedFolder = updateSelectedFolder(selectedFolder, action, nodes);
    assertEquals('2', selectedFolder);
  });

  test('updates when parent of selected folder is closed', function() {
    action = selectFolder('2')!;
    selectedFolder = updateSelectedFolder(selectedFolder, action, nodes);

    action = changeFolderOpen('1', false);
    selectedFolder = updateSelectedFolder(selectedFolder, action, nodes);
    assertEquals('1', selectedFolder);
  });

  test('selects ancestor when selected folder is deleted', function() {
    action = selectFolder('3')!;
    selectedFolder = updateSelectedFolder(selectedFolder, action, nodes);

    // Delete the selected folder:
    action = removeBookmark('3', '2', 0, nodes);
    selectedFolder = updateSelectedFolder(selectedFolder, action, nodes);

    assertEquals('2', selectedFolder);

    action = selectFolder('4')!;
    selectedFolder = updateSelectedFolder(selectedFolder, action, nodes);

    // Delete an ancestor of the selected folder:
    action = removeBookmark('2', '1', 0, nodes);
    selectedFolder = updateSelectedFolder(selectedFolder, action, nodes);

    assertEquals('1', selectedFolder);
  });
});

suite('node state', function() {
  let nodes: NodeMap;
  let action;

  setup(function() {
    nodes = testTree(
        createFolder(
            '1',
            [
              createItem('2', {title: 'a', url: 'a.com'}),
              createItem('3'),
              createFolder('4', []),
            ]),
        createFolder('5', []));
  });

  test('updates when a node is edited', function() {
    action = editBookmark('2', {title: 'b'});
    nodes = updateNodes(nodes, action);

    assertEquals('b', nodes['2']!.title);
    assertEquals('a.com', nodes['2']!.url);

    action = editBookmark('2', {title: 'c', url: 'c.com'});
    nodes = updateNodes(nodes, action);

    assertEquals('c', nodes['2']!.title);
    assertEquals('c.com', nodes['2']!.url);

    action = editBookmark('4', {title: 'folder'});
    nodes = updateNodes(nodes, action);

    assertEquals('folder', nodes['4']!.title);
    assertEquals(undefined, nodes['4']!.url);

    // Cannot edit URL of a folder:
    action = editBookmark('4', {title: 'folder', url: 'folder.com'});
    nodes = updateNodes(nodes, action);

    assertEquals('folder', nodes['4']!.title);
    assertEquals(undefined, nodes['4']!.url);
  });

  test('updates when a node is created', function() {
    // Create a folder.
    const folder = {
      title: '',
      id: '6',
      parentId: '1',
      index: 2,
    };
    action = createBookmark(folder.id, folder);
    nodes = updateNodes(nodes, action);

    assertEquals('1', nodes['6']!.parentId);
    assertDeepEquals([], nodes['6']!.children);
    assertDeepEquals(['2', '3', '6', '4'], nodes['1']!.children);

    // Add a new item to that folder.
    const item = {
      title: '',
      id: '7',
      parentId: '6',
      index: 0,
      url: 'https://www.example.com',
    };

    action = createBookmark(item.id, item);
    nodes = updateNodes(nodes, action);

    assertEquals('6', nodes['7']!.parentId);
    assertEquals(undefined, nodes['7']!.children);
    assertDeepEquals(['7'], nodes['6']!.children);
  });

  test('updates when a node is deleted', function() {
    action = removeBookmark('3', '1', 1, nodes);
    nodes = updateNodes(nodes, action);

    assertDeepEquals(['2', '4'], nodes['1']!.children);

    assertDeepEquals(['2', '4'], nodes['1']!.children);
    assertEquals(undefined, nodes['3']);
  });

  test('removes all children of deleted nodes', function() {
    action = removeBookmark('1', '0', 0, nodes);
    nodes = updateNodes(nodes, action);

    assertDeepEquals(['0', '5'], Object.keys(nodes).sort());
  });

  test('updates when a node is moved', function() {
    // Move within the same folder backwards.
    action = moveBookmark('3', '1', 0, '1', 1);
    nodes = updateNodes(nodes, action);

    assertDeepEquals(['3', '2', '4'], nodes['1']!.children);

    // Move within the same folder forwards.
    action = moveBookmark('3', '1', 2, '1', 0);
    nodes = updateNodes(nodes, action);

    assertDeepEquals(['2', '4', '3'], nodes['1']!.children);

    // Move between different folders.
    action = moveBookmark('4', '5', 0, '1', 1);
    nodes = updateNodes(nodes, action);

    assertDeepEquals(['2', '3'], nodes['1']!.children);
    assertDeepEquals(['4'], nodes['5']!.children);
  });

  test('updates when children of a node are reordered', function() {
    action = reorderChildren('1', ['4', '2', '3']);
    nodes = updateNodes(nodes, action);

    assertDeepEquals(['4', '2', '3'], nodes['1']!.children);
  });
});

suite('search state', function() {
  let state: BookmarksPageState;

  setup(function() {
    // Search touches a few different things, so we test using the entire state.
    state = createEmptyState();
    state.nodes = testTree(createFolder('1', [
      createFolder(
          '2',
          [
            createItem('3'),
          ]),
    ]));
  });

  test('updates when search is started and finished', function() {
    let action;

    action = selectFolder('2');
    state = reduceAction(state, action!);

    action = setSearchTerm('test');
    state = reduceAction(state, action!);

    assertEquals('test', state.search.term);
    assertTrue(state.search.inProgress);

    // UI should not have changed yet.
    assertFalse(isShowingSearch(state));
    assertDeepEquals(['3'], getDisplayedList(state));

    action = setSearchResults(['2', '3']);
    const searchedState = reduceAction(state, action);

    assertFalse(searchedState.search.inProgress);

    // UI changes once search results arrive.
    assertTrue(isShowingSearch(searchedState));
    assertDeepEquals(['2', '3'], getDisplayedList(searchedState));

    // Case 1: Clear search by setting an empty search term.
    action = setSearchTerm('');
    const clearedState = reduceAction(searchedState, action);

    // Should go back to displaying the contents of '2', which was shown before
    // the search.
    assertEquals('2', clearedState.selectedFolder);
    assertFalse(isShowingSearch(clearedState));
    assertDeepEquals(['3'], getDisplayedList(clearedState));
    assertEquals('', clearedState.search.term);
    assertDeepEquals(null, clearedState.search.results);

    // Case 2: Clear search by selecting a new folder.
    action = selectFolder('1');
    const selectedState = reduceAction(searchedState, action!);

    assertEquals('1', selectedState.selectedFolder);
    assertFalse(isShowingSearch(selectedState));
    assertDeepEquals(['2'], getDisplayedList(selectedState));
    assertEquals('', selectedState.search.term);
    assertDeepEquals(null, selectedState.search.results);
  });

  test('results do not clear while performing a second search', function() {
    let action = setSearchTerm('te');
    state = reduceAction(state, action);

    assertFalse(isShowingSearch(state));

    action = setSearchResults(['2', '3']);
    state = reduceAction(state, action);

    assertFalse(state.search.inProgress);
    assertTrue(isShowingSearch(state));

    // Continuing the search should not clear the previous results, which should
    // continue to show until the new results arrive.
    action = setSearchTerm('test');
    state = reduceAction(state, action);

    assertTrue(state.search.inProgress);
    assertTrue(isShowingSearch(state));
    assertDeepEquals(['2', '3'], getDisplayedList(state));

    action = setSearchResults(['3']);
    state = reduceAction(state, action);

    assertFalse(state.search.inProgress);
    assertTrue(isShowingSearch(state));
    assertDeepEquals(['3'], getDisplayedList(state));
  });

  test('removes deleted nodes', function() {
    let action;

    action = setSearchTerm('test');
    state = reduceAction(state, action);

    action = setSearchResults(['1', '3', '2']);
    state = reduceAction(state, action);

    action = removeBookmark('2', '1', 0, state.nodes);
    state = reduceAction(state, action);

    // 2 and 3 should be removed, since 2 was deleted and 3 was a descendant of
    // 2.
    assertDeepEquals(['1'], state.search.results);
  });
});
