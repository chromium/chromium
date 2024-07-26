// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarkElement, BookmarksAppElement, BookmarksFolderNodeElement, BookmarksItemElement, BookmarksListElement, DndManager} from 'chrome://bookmarks/bookmarks.js';
import {BookmarkManagerApiProxyImpl, BrowserProxyImpl, DragInfo, overrideFolderOpenerTimeoutDelay, setDebouncerForTesting} from 'chrome://bookmarks/bookmarks.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {middleOfNode, topLeftOfNode} from 'chrome://webui-test/mouse_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBookmarkManagerApiProxy} from './test_bookmark_manager_api_proxy.js';
import {TestBookmarksBrowserProxy} from './test_browser_proxy.js';
import {TestStore} from './test_store.js';
import {TestTimerProxy} from './test_timer_proxy.js';
import {createFolder, createItem, findFolderNode, getAllFoldersOpenState, normalizeIterable, replaceBody, testTree} from './test_util.js';

suite('drag and drop', function() {
  let app: BookmarksAppElement;
  let list: BookmarksListElement;
  let rootFolderNode: BookmarksFolderNodeElement;
  let store: TestStore;
  let dndManager: DndManager;
  let bookmarkManagerApi: TestBookmarkManagerApiProxy;

  enum DragStyle {
    NONE = 0,
    ON = 1,
    ABOVE = 2,
    BELOW = 3,
  }

  function getFolderNode(id: string) {
    return findFolderNode(rootFolderNode, id) as BookmarksFolderNodeElement;
  }

  function getListItem(id: string) {
    const items = list.root!.querySelectorAll('bookmarks-item');
    for (let i = 0; i < items.length; i++) {
      if (items[i]!.itemId === id) {
        return items[i] as BookmarksItemElement;
      }
    }
    assertNotReached();
  }

  function dispatchDragEvent(
      type: string, node: HTMLElement, xy?: {x: number, y: number}) {
    xy = xy || middleOfNode(node);
    const props = {
      bubbles: true,
      cancelable: true,
      composed: true,
      clientX: xy!.x,
      clientY: xy!.y,
      // Make this a primary input.
      buttons: 1,
    };
    const e = new DragEvent(type, props);
    node.dispatchEvent(e);
  }

  function bottomRightOfNode(target: HTMLElement) {
    const rect = target.getBoundingClientRect();
    return {y: rect.top + rect.height, x: rect.left + rect.width};
  }

  function assertDragStyle(bookmarkElement: BookmarkElement, style: DragStyle) {
    const dragStyles: {[key: string]: string} = {};
    dragStyles[DragStyle.ON] = 'drag-on';
    dragStyles[DragStyle.ABOVE] = 'drag-above';
    dragStyles[DragStyle.BELOW] = 'drag-below';

    const classList = bookmarkElement.getDropTarget()!.classList;
    Object.entries(dragStyles).forEach(([dragStyle, value]) => {
      assertEquals(
          dragStyle === style.toString(), classList.contains(value),
          value + (dragStyle === style.toString() ? ' missing' : ' found') +
              ' in classList ' + classList);
    });
  }

  function createDragData(ids: string[], sameProfile: boolean = true) {
    return {
      elements: ids.map(
          id => store.data.nodes[id] as chrome.bookmarks.BookmarkTreeNode),
      sameProfile: sameProfile,
    };
  }

  async function simulateDragStart(dragElement: HTMLElement) {
    dispatchDragEvent('dragstart', dragElement);
    const idList = await bookmarkManagerApi.whenCalled('startDrag');
    bookmarkManagerApi.resetResolver('startDrag');
    dndManager.getDragInfoForTesting()!.setNativeDragData(
        createDragData(idList));
    move(dragElement, topLeftOfNode(dragElement));
  }

  function move(target: HTMLElement, dest?: {x: number, y: number}) {
    dispatchDragEvent('dragover', target, dest || middleOfNode(target));
  }

  function getDragIds() {
    return dndManager.getDragInfoForTesting()!.dragData!.elements.map(
        (x) => x.id);
  }

  setup(function() {
    const nodes = testTree(
        createFolder(
            '1',
            [
              createFolder(
                  '11',
                  [
                    createFolder(
                        '111',
                        [
                          createItem('1111'),
                        ]),
                    createFolder('112', []),
                  ]),
              createItem('12'),
              createItem('13'),
              createFolder('14', []),
              createFolder('15', []),
            ]),
        createFolder('2', []));
    store = new TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: '1',
    });
    store.replaceSingleton();

    bookmarkManagerApi = new TestBookmarkManagerApiProxy();
    BookmarkManagerApiProxyImpl.setInstance(bookmarkManagerApi);

    const testBrowserProxy = new TestBookmarksBrowserProxy();
    BrowserProxyImpl.setInstance(testBrowserProxy);
    app = document.createElement('bookmarks-app');
    replaceBody(app);
    list =
        app.shadowRoot!.querySelector<BookmarksListElement>('bookmarks-list')!;
    rootFolderNode = app.shadowRoot!.querySelector<BookmarksFolderNodeElement>(
        'bookmarks-folder-node')!;
    dndManager = app.getDndManagerForTesting() as DndManager;
    dndManager!.setTimerProxyForTesting(new TestTimerProxy());

    // Wait for the API listener to call the browser proxy, since this
    // indicates initialization is done.
    return testBrowserProxy.whenCalled('getIncognitoAvailability').then(() => {
      flush();
    });
  });

  test('dragInfo isDraggingFolderToDescendant', function() {
    const dragInfo = new DragInfo();
    const nodes = store.data.nodes;
    dragInfo.setNativeDragData(createDragData(['11']));
    assertTrue(dragInfo.isDraggingFolderToDescendant('111', nodes));
    assertFalse(dragInfo.isDraggingFolderToDescendant('1', nodes));
    assertFalse(dragInfo.isDraggingFolderToDescendant('2', nodes));

    dragInfo.setNativeDragData(createDragData(['1']));
    assertTrue(dragInfo.isDraggingFolderToDescendant('14', nodes));
    assertTrue(dragInfo.isDraggingFolderToDescendant('111', nodes));
    assertFalse(dragInfo.isDraggingFolderToDescendant('2', nodes));
  });

  test('drag in list', async function() {
    const dragElement = getListItem('13');
    let dragTarget = getListItem('12');

    await simulateDragStart(dragElement);

    assertDeepEquals(['13'], getDragIds());

    // Bookmark items cannot be dragged onto other items.
    move(dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ABOVE);

    move(document.body);
    assertDragStyle(dragTarget, DragStyle.NONE);

    // Bookmark items can be dragged onto folders.
    dragTarget = getListItem('11');
    move(dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ABOVE);

    move(dragTarget, middleOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ON);

    move(dragTarget, bottomRightOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.BELOW);

    // There are no valid drop locations for dragging an item onto itself.
    move(dragElement);

    assertDragStyle(dragTarget, DragStyle.NONE);
    assertDragStyle(dragElement, DragStyle.NONE);
  });

  test('reorder folder nodes', async function() {
    const dragElement = getFolderNode('112');
    const dragTarget = getFolderNode('111');

    await simulateDragStart(dragElement);

    move(dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ABOVE);

    move(dragTarget, bottomRightOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ON);
  });

  test('drag an item into a sidebar folder', async function() {
    const dragElement = getListItem('13');
    let dragTarget = getFolderNode('2');
    await simulateDragStart(dragElement);

    // Items can only be dragged onto sidebar folders, not above or below.
    move(dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ON);

    move(dragTarget, middleOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ON);

    move(dragTarget, bottomRightOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ON);

    // Items cannot be dragged onto their parent folders.
    dragTarget = getFolderNode('1');
    move(dragTarget);
    assertDragStyle(dragTarget, DragStyle.NONE);
  });

  test('drag a folder into a descendant', async function() {
    const dragElement = getFolderNode('11');
    const dragTarget = getFolderNode('112');

    // Folders cannot be dragged into their descendants.
    await simulateDragStart(dragElement);

    move(dragTarget);

    assertDragStyle(dragTarget, DragStyle.NONE);
  });

  test('drag item into sidebar folder with descendants', async function() {
    const dragElement = getFolderNode('15');
    const dragTarget = getFolderNode('11');

    await simulateDragStart(dragElement);

    // Drags below an open folder are not allowed.
    move(dragTarget, middleOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ON);

    move(dragTarget, bottomRightOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ON);

    move(dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ABOVE);

    dispatchDragEvent('dragend', dragElement);
    assertDragStyle(dragTarget, DragStyle.NONE);

    store.data.folderOpenState.set('11', false);
    store.notifyObservers();

    await simulateDragStart(dragElement);

    // Drags below a closed folder are allowed.
    move(dragTarget, bottomRightOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.BELOW);
  });

  test('drag multiple list items', async function() {
    // Dragging multiple items.
    store.data.selection.items = new Set(['13', '15']);
    let dragElement = getListItem('13');
    await simulateDragStart(dragElement);
    assertDeepEquals(['13', '15'], getDragIds());

    // The dragged items should not be allowed to be dragged around any selected
    // item.
    let dragTarget = getListItem('13');
    move(dragTarget);
    assertDragStyle(dragTarget, DragStyle.NONE);

    dragTarget = getListItem('14');
    move(dragTarget);
    assertDragStyle(dragTarget, DragStyle.ON);

    dragTarget = getListItem('15');
    move(dragTarget);
    assertDragStyle(dragTarget, DragStyle.NONE);

    dispatchDragEvent('dragend', dragElement);

    // Dragging an unselected item should only drag the unselected item.
    dragElement = getListItem('14');
    await simulateDragStart(dragElement);
    assertDeepEquals(['14'], getDragIds());
    dispatchDragEvent('dragend', dragElement);

    // Dragging a folder node should only drag the node.
    dragElement = getListItem('11');
    await simulateDragStart(dragElement);
    assertDeepEquals(['11'], getDragIds());
  });

  test('drag multiple list items preserve displaying order', async function() {
    // Dragging multiple items with different selection order.
    store.data.selection.items = new Set(['15', '13']);
    const dragElement = getListItem('13');
    await simulateDragStart(dragElement);
    assertDeepEquals(['13', '15'], getDragIds());
  });

  test('bookmarks from different profiles', function() {
    bookmarkManagerApi.onDragEnter.callListeners(createDragData(['11'], false));

    // All positions should be allowed even with the same bookmark id if the
    // drag element is from a different profile.
    let dragTarget: BookmarkElement = getListItem('11');
    move(dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ABOVE);

    move(dragTarget, middleOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ON);

    move(dragTarget, bottomRightOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.BELOW);

    // Folders from other profiles should be able to be dragged into
    // descendants in this profile.
    dragTarget = getFolderNode('112');
    move(dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ABOVE);

    move(dragTarget, middleOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ON);

    move(dragTarget, bottomRightOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.BELOW);
  });

  test('drag from sidebar to list', async function() {
    let dragElement: BookmarkElement = getFolderNode('112');
    let dragTarget = getListItem('13');

    // Drag a folder onto the list.
    await simulateDragStart(dragElement);
    assertDeepEquals(['112'], getDragIds());

    move(dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ABOVE);

    dispatchDragEvent('dragend', dragTarget);

    // Folders should not be able to dragged onto themselves in the list.
    bookmarkManagerApi.onDragEnter.callListeners(createDragData(['11']));
    dragElement = getListItem('11');
    move(dragElement);
    assertDragStyle(dragElement, DragStyle.NONE);

    // Ancestors should not be able to be dragged onto descendant
    // displayed lists.
    store.data.selectedFolder = '111';
    store.notifyObservers();
    flush();

    bookmarkManagerApi.onDragEnter.callListeners(createDragData(['11']));
    dragTarget = getListItem('1111');
    move(dragTarget);
    assertDragStyle(dragTarget, DragStyle.NONE);
  });

  test('drags with search', function() {
    store.data.search.term = 'Asgore';
    store.data.search.results = ['11', '13', '2'];
    store.data.selectedFolder = '';
    store.notifyObservers();

    // Search results should not be able to be dragged onto, but can be dragged
    // from.
    bookmarkManagerApi.onDragEnter.callListeners(createDragData(['2']));
    let dragTarget: BookmarkElement = getListItem('13');
    move(dragTarget);
    assertDragStyle(dragTarget, DragStyle.NONE);

    // Drags onto folders should work as per usual.
    dragTarget = getFolderNode('112');
    move(dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ABOVE);

    move(dragTarget, middleOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ON);

    move(dragTarget, bottomRightOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.BELOW);
  });

  // This is a regression test for https://crbug.com/974525.
  test(
      'drag bookmark that is not in selected folder but in search result',
      async function() {
        store.data.search.term = 'Asgore';
        store.data.search.results = ['11', '13', '2'];
        store.data.selectedFolder = '';
        store.notifyObservers();

        await simulateDragStart(getListItem('13'));

        assertDeepEquals(['13'], getDragIds());
      });

  test('simple native drop end to end', async function() {
    const dragElement = getListItem('13');
    const dragTarget = getListItem('12');

    await simulateDragStart(dragElement);
    assertDeepEquals(['13'], getDragIds());

    dispatchDragEvent('dragover', dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ABOVE);

    setDebouncerForTesting();
    dispatchDragEvent('drop', dragTarget);

    const [dropParentId, dropIndex] =
        await bookmarkManagerApi.whenCalled('drop');

    assertEquals('1', dropParentId);
    assertEquals(1, dropIndex);

    dispatchDragEvent('dragend', dragTarget);
    assertDragStyle(dragTarget, DragStyle.NONE);
  });

  test('auto expander', async function() {
    overrideFolderOpenerTimeoutDelay(0);
    store.setReducersEnabled(true);

    store.data.folderOpenState.set('11', false);
    store.data.folderOpenState.set('14', false);
    store.data.folderOpenState.set('15', false);
    store.notifyObservers();
    flush();

    const dragElement = getFolderNode('15');
    await simulateDragStart(dragElement);

    // Dragging onto folders without children doesn't open the folder.
    let dragTarget = getFolderNode('14');
    move(dragTarget);
    await flushTasks();
    assertFalse(dragTarget.isOpen);

    // Dragging onto itself doesn't open the folder.
    move(dragElement);
    await flushTasks();
    assertFalse(dragElement.isOpen);

    // Dragging onto an open folder doesn't affect the folder.
    dragTarget = getFolderNode('1');
    assertTrue(dragTarget.isOpen);
    move(dragTarget);
    await flushTasks();
    assertTrue(dragTarget.isOpen);

    dragTarget = getFolderNode('11');

    // Dragging off of a closed folder doesn't open it.
    move(dragTarget);
    move(list);
    await flushTasks();
    assertFalse(dragTarget.isOpen);

    // Dragging onto a folder with DragStyle.BELOW doesn't open it.
    move(dragTarget, bottomRightOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.BELOW);
    await flushTasks();
    assertFalse(dragTarget.isOpen);

    // Dragging onto a folder with DragStyle.ABOVE doesn't open it.
    move(dragTarget, topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DragStyle.ABOVE);
    await flushTasks();
    assertFalse(dragTarget.isOpen);

    // Dragging onto a closed folder with children opens it.
    move(dragTarget);
    assertDragStyle(dragTarget, DragStyle.ON);
    await flushTasks();
    assertTrue(dragTarget.isOpen);
  });

  test('drag item selects/deselects items', async function() {
    store.setReducersEnabled(true);

    store.data.selection.items = new Set(['13', '15']);
    store.notifyObservers();

    // Dragging an item not in the selection selects the dragged item and
    // deselects the previous selection.
    let dragElement: BookmarkElement = getListItem('14');
    await simulateDragStart(dragElement);
    assertDeepEquals(['14'], normalizeIterable(store.data.selection.items));
    dispatchDragEvent('dragend', dragElement);

    // Dragging a folder node deselects any selected items in the bookmark list.
    dragElement = getFolderNode('15');
    await simulateDragStart(dragElement);
    assertDeepEquals([], normalizeIterable(store.data.selection.items));
    dispatchDragEvent('dragend', dragElement);
  });

  test('cannot drag items when editing is disabled', async function() {
    store.data.prefs.canEdit = false;
    store.notifyObservers();

    const dragElement = getFolderNode('11');

    dispatchDragEvent('dragstart', dragElement);
    assertFalse(dndManager.getDragInfoForTesting()!.isDragValid());
  });

  test('cannot start dragging unmodifiable items', async function() {
    store.data.nodes['2']!.unmodifiable = 'managed';
    store.notifyObservers();

    let dragElement = getFolderNode('1');
    dispatchDragEvent('dragstart', dragElement);
    assertFalse(dndManager.getDragInfoForTesting()!.isDragValid());

    dragElement = getFolderNode('2');
    dispatchDragEvent('dragstart', dragElement);
    assertFalse(dndManager.getDragInfoForTesting()!.isDragValid());
  });

  test('cannot drag onto folders with unmodifiable children', async function() {
    store.data.nodes['2']!.unmodifiable = 'managed';
    store.notifyObservers();

    const dragElement = getListItem('12');
    await simulateDragStart(dragElement);

    // Can't drag onto the unmodifiable node.
    const dragTarget = getFolderNode('2');
    move(dragTarget);
    assertDragStyle(dragTarget, DragStyle.NONE);
  });
});
