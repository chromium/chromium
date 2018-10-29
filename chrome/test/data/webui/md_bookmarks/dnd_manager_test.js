// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('drag and drop', function() {
  let app;
  let list;
  let rootFolderNode;
  let store;
  let dndManager;

  const DRAG_STYLE = {
    NONE: 0,
    ON: 1,
    ABOVE: 2,
    BELOW: 3,
  };

  function getFolderNode(id) {
    return findFolderNode(rootFolderNode, id);
  }

  function getListItem(id) {
    const items = list.root.querySelectorAll('bookmarks-item');
    for (let i = 0; i < items.length; i++) {
      if (items[i].itemId == id)
        return items[i];
    }
  }

  function dispatchDragEvent(type, node, xy) {
    xy = xy || MockInteractions.middleOfNode(node);
    const props = {
      bubbles: true,
      cancelable: true,
      composed: true,
      clientX: xy.x,
      clientY: xy.y,
      // Make this a primary input.
      buttons: 1,
    };
    const e = new DragEvent(type, props);
    node.dispatchEvent(e);
  }

  function assertDragStyle(bookmarkElement, style) {
    const dragStyles = {};
    dragStyles[DRAG_STYLE.ON] = 'drag-on';
    dragStyles[DRAG_STYLE.ABOVE] = 'drag-above';
    dragStyles[DRAG_STYLE.BELOW] = 'drag-below';

    const classList = bookmarkElement.getDropTarget().classList;
    Object.keys(dragStyles).forEach(dragStyle => {
      assertEquals(
          dragStyle == style, classList.contains(dragStyles[dragStyle]),
          dragStyles[dragStyle] + (dragStyle == style ? ' missing' : ' found') +
              ' in classList ' + classList);
    });
  }

  function createDragData(ids, sameProfile) {
    return {
      elements: ids.map(id => store.data.nodes[id]),
      sameProfile: sameProfile == undefined ? true : sameProfile,
    };
  }

  function simulateDragStart(dragElement) {
    dispatchDragEvent('dragstart', dragElement);
    move(dragElement, MockInteractions.topLeftOfNode(dragElement));
  }

  function move(target, dest) {
    dispatchDragEvent(
        'dragover', target, dest || MockInteractions.middleOfNode(target));
  }

  function getDragIds() {
    return dndManager.dragInfo_.dragData.elements.map((x) => x.id);
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
    store = new bookmarks.TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: '1',
    });
    store.replaceSingleton();

    chrome.bookmarks.move = function(id, details) {};
    chrome.bookmarkManagerPrivate.startDrag = function(
        idList, dragNodeIndex, isFromTouch) {
      dndManager.dragInfo_.setNativeDragData(createDragData(idList));
    };

    app = document.createElement('bookmarks-app');
    replaceBody(app);
    list = app.$$('bookmarks-list');
    rootFolderNode = app.$$('bookmarks-folder-node');
    dndManager = app.dndManager_;
    dndManager.setTimerProxyForTesting(new bookmarks.TestTimerProxy());
    Polymer.dom.flush();
  });

  test('dragInfo isDraggingFolderToDescendant', function() {
    const dragInfo = new bookmarks.DragInfo();
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

  test('drag in list', function() {
    const dragElement = getListItem('13');
    let dragTarget = getListItem('12');

    simulateDragStart(dragElement);

    assertDeepEquals(['13'], getDragIds());

    // Bookmark items cannot be dragged onto other items.
    move(dragTarget, MockInteractions.topLeftOfNode(dragTarget));
    assertEquals(
        DropPosition.ABOVE,
        dndManager.calculateValidDropPositions_(dragTarget));
    assertDragStyle(dragTarget, DRAG_STYLE.ABOVE);

    move(document.body);
    assertDragStyle(dragTarget, DRAG_STYLE.NONE);

    // Bookmark items can be dragged onto folders.
    dragTarget = getListItem('11');
    move(dragTarget);
    assertEquals(
        DropPosition.ON | DropPosition.ABOVE | DropPosition.BELOW,
        dndManager.calculateValidDropPositions_(dragTarget));
    assertDragStyle(dragTarget, DRAG_STYLE.ON);

    // There are no valid drop locations for dragging an item onto itself.
    assertEquals(
        DropPosition.NONE,
        dndManager.calculateValidDropPositions_(dragElement));
    move(dragElement);

    assertDragStyle(dragTarget, DRAG_STYLE.NONE);
    assertDragStyle(dragElement, DRAG_STYLE.NONE);
  });

  test('reorder folder nodes', function() {
    const dragElement = getFolderNode('112');
    const dragTarget = getFolderNode('111');

    simulateDragStart(dragElement);

    assertEquals(
        DropPosition.ON | DropPosition.ABOVE,
        dndManager.calculateValidDropPositions_(dragTarget));

    move(dragTarget, MockInteractions.topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DRAG_STYLE.ABOVE);
  });

  test('drag an item into a sidebar folder', function() {
    const dragElement = getListItem('13');
    let dragTarget = getFolderNode('2');
    simulateDragStart(dragElement);

    // Items can only be dragged onto sidebar folders, not above or below.
    assertEquals(
        DropPosition.ON, dndManager.calculateValidDropPositions_(dragTarget));

    move(dragTarget);
    assertDragStyle(dragTarget, DRAG_STYLE.ON);

    // Items cannot be dragged onto their parent folders.
    dragTarget = getFolderNode('1');
    move(dragTarget);
    assertDragStyle(dragTarget, DRAG_STYLE.NONE);
  });

  test('drag a folder into a descendant', function() {
    const dragElement = getFolderNode('11');
    const dragTarget = getFolderNode('112');

    // Folders cannot be dragged into their descendants.
    simulateDragStart(dragElement);
    assertEquals(
        DropPosition.NONE, dndManager.calculateValidDropPositions_(dragTarget));

    move(dragTarget);

    assertDragStyle(dragTarget, DRAG_STYLE.NONE);
  });

  test('drag item into sidebar folder with descendants', function() {
    const dragElement = getFolderNode('15');
    const dragTarget = getFolderNode('11');

    simulateDragStart(dragElement);

    // Drags below an open folder are not allowed.
    assertEquals(
        DropPosition.ON | DropPosition.ABOVE,
        dndManager.calculateValidDropPositions_(dragTarget));

    move(dragTarget);

    assertDragStyle(dragTarget, DRAG_STYLE.ON);

    dispatchDragEvent('dragend', dragElement);
    assertDragStyle(dragTarget, DRAG_STYLE.NONE);

    store.data.folderOpenState.set('11', false);
    store.notifyObservers();

    simulateDragStart(dragElement);

    // Drags below a closed folder are allowed.
    assertEquals(
        DropPosition.ON | DropPosition.ABOVE | DropPosition.BELOW,
        dndManager.calculateValidDropPositions_(dragTarget));
  });

  test('drag multiple list items', function() {
    // Dragging multiple items.
    store.data.selection.items = new Set(['13', '15']);
    let dragElement = getListItem('13');
    simulateDragStart(dragElement);
    assertDeepEquals(['13', '15'], getDragIds());

    // The dragged items should not be allowed to be dragged around any selected
    // item.
    assertEquals(
        DropPosition.NONE,
        dndManager.calculateValidDropPositions_(getListItem('13')));
    assertEquals(
        DropPosition.ON,
        dndManager.calculateValidDropPositions_(getListItem('14')));
    assertEquals(
        DropPosition.NONE,
        dndManager.calculateValidDropPositions_(getListItem('15')));
    dispatchDragEvent('dragend', dragElement);

    // Dragging an unselected item should only drag the unselected item.
    dragElement = getListItem('14');
    simulateDragStart(dragElement);
    assertDeepEquals(['14'], getDragIds());
    dispatchDragEvent('dragend', dragElement);

    // Dragging a folder node should only drag the node.
    dragElement = getListItem('11');
    simulateDragStart(dragElement);
    assertDeepEquals(['11'], getDragIds());
  });

  test('bookmarks from different profiles', function() {
    dndManager.handleChromeDragEnter_(createDragData(['11'], false));

    // All positions should be allowed even with the same bookmark id if the
    // drag element is from a different profile.
    assertEquals(
        DropPosition.ON | DropPosition.ABOVE | DropPosition.BELOW,
        dndManager.calculateValidDropPositions_(getListItem('11')));

    // Folders from other profiles should be able to be dragged into
    // descendants in this profile.
    assertEquals(
        DropPosition.ON | DropPosition.ABOVE | DropPosition.BELOW,
        dndManager.calculateValidDropPositions_(getFolderNode('112')));
  });

  test('drag from sidebar to list', function() {
    const dragElement = getFolderNode('112');
    const dragTarget = getListItem('13');

    // Drag a folder onto the list.
    simulateDragStart(dragElement);
    assertDeepEquals(['112'], getDragIds());

    move(dragTarget, MockInteractions.topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DRAG_STYLE.ABOVE);

    dispatchDragEvent('dragend', dragTarget);

    // Folders should not be able to dragged onto themselves in the list.
    dndManager.handleChromeDragEnter_(createDragData(['11']));
    assertEquals(
        DropPosition.NONE,
        dndManager.calculateValidDropPositions_(getListItem('11')));

    // Ancestors should not be able to be dragged onto descendant
    // displayed lists.
    store.data.selectedFolder = '111';
    store.notifyObservers();
    Polymer.dom.flush();

    dndManager.handleChromeDragEnter_(createDragData(['11']));
    assertEquals(
        DropPosition.NONE,
        dndManager.calculateValidDropPositions_(getListItem('1111')));
  });

  test('drags with search', function() {
    store.data.search.term = 'Asgore';
    store.data.search.results = ['11', '13', '2'];
    store.data.selectedFolder = null;
    store.notifyObservers();

    // Search results should not be able to be dragged onto, but can be dragged
    // from.
    dndManager.handleChromeDragEnter_(createDragData(['2']));
    assertEquals(
        DropPosition.NONE,
        dndManager.calculateValidDropPositions_(getListItem('13')));

    // Drags onto folders should work as per usual.
    assertEquals(
        DropPosition.ON | DropPosition.ABOVE | DropPosition.BELOW,
        dndManager.calculateValidDropPositions_(getFolderNode('112')));

    // Drags onto an empty search list do nothing.
    store.data.search.results = [];
    store.notifyObservers();
    assertEquals(
        DropPosition.NONE, dndManager.calculateValidDropPositions_(list));
  });

  test('calculateDropInfo_', function() {
    function assertDropInfo(parentId, index, element, position) {
      assertDeepEquals(
          {parentId: parentId, index: index},
          dndManager.calculateDropInfo_(
              {element: element, position: position}));
    }

    // Drops onto the list.
    assertDropInfo('1', 0, getListItem('11'), DropPosition.ABOVE);
    assertDropInfo('1', 2, getListItem('12'), DropPosition.BELOW);
    assertDropInfo('13', -1, getListItem('13'), DropPosition.ON);

    // Drops onto the sidebar.
    assertDropInfo('1', 4, getFolderNode('15'), DropPosition.ABOVE);
    assertDropInfo('1', 5, getFolderNode('15'), DropPosition.BELOW);
    assertDropInfo('111', -1, getFolderNode('111'), DropPosition.ON);
  });

  test('simple native drop end to end', function() {
    let dropParentId;
    let dropIndex;
    chrome.bookmarkManagerPrivate.drop = function(parentId, index) {
      dropParentId = parentId;
      dropIndex = index;
    };

    const dragElement = getListItem('13');
    const dragTarget = getListItem('12');

    simulateDragStart(dragElement);
    assertDeepEquals(['13'], getDragIds());

    dispatchDragEvent(
        'dragover', dragTarget, MockInteractions.topLeftOfNode(dragTarget));
    assertDragStyle(dragTarget, DRAG_STYLE.ABOVE);

    dispatchDragEvent('drop', dragTarget);
    assertEquals('1', dropParentId);
    assertEquals(1, dropIndex);

    dispatchDragEvent('dragend', dragTarget);
    assertDragStyle(dragTarget, DRAG_STYLE.NONE);
  });

  test('auto expander', function() {
    const timerProxy = new bookmarks.TestTimerProxy();
    timerProxy.immediatelyResolveTimeouts = false;

    const autoExpander = dndManager.autoExpander_;
    autoExpander.debouncer_.timerProxy_ = timerProxy;

    store.data.folderOpenState.set('11', false);
    store.notifyObservers();
    Polymer.dom.flush();

    const dragElement = getFolderNode('14');
    let dragTarget = getFolderNode('15');

    simulateDragStart(dragElement);

    // Dragging onto folders without children doesn't update the auto expander.
    move(dragTarget);
    assertEquals(null, autoExpander.lastElement_);

    // Dragging onto open folders doesn't update the auto expander.
    dragTarget = getFolderNode('1');
    move(dragTarget);
    assertEquals(null, autoExpander.lastElement_);

    // Dragging onto a closed folder with children updates the auto expander.
    dragTarget = getFolderNode('11');
    move(dragTarget);
    assertEquals(dragTarget, autoExpander.lastElement_);

    // Dragging onto another item resets the auto expander.
    dragTarget = getFolderNode('1');
    move(dragTarget);
    assertEquals(null, autoExpander.lastElement_);

    // Dragging onto the list resets the auto expander.
    dragTarget = getFolderNode('11');
    move(dragTarget);
    assertEquals(dragTarget, autoExpander.lastElement_);

    dragTarget = list;
    move(dragTarget);
    assertEquals(null, autoExpander.lastElement_);

    // Moving the mouse resets the delay.
    dragTarget = getFolderNode('11');
    move(dragTarget);
    assertEquals(dragTarget, autoExpander.lastElement_);
    const oldTimer = autoExpander.debouncer_.timer_;

    move(dragTarget);
    assertNotEquals(oldTimer, autoExpander.debouncer_.timer_);

    // Auto expands after expand delay.
    timerProxy.runTimeoutFn(autoExpander.debouncer_.timer_);
    assertDeepEquals(
        bookmarks.actions.changeFolderOpen('11', true), store.lastAction);
    assertEquals(null, autoExpander.lastElement_);
  });

  test('drag onto empty list', function() {
    store.data.selectedFolder = '14';
    store.notifyObservers();

    const dragElement = getFolderNode('15');
    const dragTarget = list;

    // Dragging onto an empty list.
    simulateDragStart(dragElement);

    move(dragTarget);
    assertEquals(
        DropPosition.ON, dndManager.calculateValidDropPositions_(dragTarget));
    assertDragStyle(dragTarget, DRAG_STYLE.ON);

    dispatchDragEvent('dragend', dragTarget);

    // Dragging onto a non-empty list.
    store.data.selectedFolder = '11';
    store.notifyObservers();

    simulateDragStart(dragElement);

    move(dragTarget);
    assertEquals(
        DropPosition.NONE, dndManager.calculateValidDropPositions_(dragTarget));
    assertDragStyle(dragTarget, DRAG_STYLE.NONE);
  });

  test('drag item selects/deselects items', function() {
    store.setReducersEnabled(true);

    store.data.selection.items = new Set(['13', '15']);
    store.notifyObservers();

    // Dragging an item not in the selection selects the dragged item and
    // deselects the previous selection.
    let dragElement = getListItem('14');
    simulateDragStart(dragElement);
    assertDeepEquals(['14'], normalizeIterable(store.data.selection.items));
    dispatchDragEvent('dragend', dragElement);

    // Dragging a folder node deselects any selected items in the bookmark list.
    dragElement = getFolderNode('15');
    simulateDragStart(dragElement);
    assertDeepEquals([], normalizeIterable(store.data.selection.items));
    dispatchDragEvent('dragend', dragElement);
  });

  test('cannot drag items when editing is disabled', function() {
    store.data.prefs.canEdit = false;
    store.notifyObservers();

    const dragElement = getFolderNode('11');
    simulateDragStart(dragElement);
    assertFalse(dndManager.dragInfo_.isDragValid());
  });

  test('cannot start dragging unmodifiable items', function() {
    store.data.nodes['2'].unmodifiable = 'managed';
    store.notifyObservers();

    let dragElement = getFolderNode('1');
    simulateDragStart(dragElement);
    assertFalse(dndManager.dragInfo_.isDragValid());

    dragElement = getFolderNode('2');
    simulateDragStart(dragElement);
    assertFalse(dndManager.dragInfo_.isDragValid());
  });

  test('cannot drag onto folders with unmodifiable children', function() {
    store.data.nodes['2'].unmodifiable = 'managed';
    store.notifyObservers();

    const dragElement = getListItem('12');
    simulateDragStart(dragElement);

    // Can't drag onto the unmodifiable node.
    const dragTarget = getFolderNode('2');
    move(dragTarget);
    assertEquals(
        DropPosition.NONE, dndManager.calculateValidDropPositions_(dragTarget));
  });
});
