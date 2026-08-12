// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_app.js';

import type {BookmarksTreeNode} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {PowerBookmarkRowElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row.js';
import {DROP_POSITION_ATTR, DropPosition} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_drag_manager.js';
import type {PowerBookmarksListElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelPowerBookmarkDragManagerTest', () => {
  let delegate: PowerBookmarksListElement;
  let bookmarksApi: TestBookmarksApiProxy;
  let originalOnDragEnter: unknown;
  let originalOnDragLeave: unknown;
  let mockOnDragEnter: FakeChromeEvent;
  let mockOnDragLeave: FakeChromeEvent;

  const allBookmarks: BookmarksTreeNode[] = [
    {
      id: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
      parentId: 'SIDE_PANEL_ROOT_BOOKMARK_ID',
      index: 0,
      title: 'Other Bookmarks',
      url: null,
      dateAdded: null,
      dateLastUsed: null,
      unmodifiable: false,
      children: [
        {
          id: '3',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          index: 0,
          title: 'First child bookmark',
          url: 'http://child/bookmark/1/',
          dateAdded: 1,
          dateLastUsed: null,
          unmodifiable: false,
          children: null,
        },
        {
          id: '4',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          index: 1,
          title: 'Second child bookmark',
          url: 'http://child/bookmark/2/',
          dateAdded: 3,
          dateLastUsed: null,
          unmodifiable: false,
          children: null,
        },
        {
          id: '5',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          index: 2,
          title: 'Child folder',
          url: null,
          dateAdded: 2,
          dateLastUsed: null,
          unmodifiable: false,
          children: [
            {
              id: '6',
              parentId: '5',
              index: 0,
              title: 'Nested bookmark',
              url: 'http://nested/bookmark/',
              dateAdded: 4,
              dateLastUsed: null,
              unmodifiable: false,
              children: null,
            },
            {
              id: '10',
              parentId: '5',
              index: 1,
              title: 'Sub folder',
              url: null,
              dateAdded: 5,
              dateLastUsed: null,
              unmodifiable: false,
              children: [
                {
                  id: '11',
                  parentId: '10',
                  index: 0,
                  title: 'Deep nested bookmark',
                  url: 'http://deep/nested/bookmark/',
                  dateAdded: 6,
                  dateLastUsed: null,
                  unmodifiable: false,
                  children: null,
                },
              ],
            },
          ],
        },
        {
          id: '7',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          index: 3,
          title: 'Sibling folder',
          url: null,
          dateAdded: 7,
          dateLastUsed: null,
          unmodifiable: false,
          children: [
            {
              id: '8',
              parentId: '7',
              index: 0,
              title: 'Sibling nested bookmark',
              url: 'http://sibling/nested/bookmark/',
              dateAdded: 8,
              dateLastUsed: null,
              unmodifiable: false,
              children: null,
            },
          ],
        },
      ],
    },
  ];

  function getBookmarkRow(id: string) {
    const rows = delegate.shadowRoot.querySelectorAll('power-bookmark-row');
    for (const row of rows) {
      if (row instanceof PowerBookmarkRowElement && row.bookmark.id === id) {
        return row;
      }
    }
    return undefined;
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setAllBookmarks(allBookmarks);
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    loadTimeData.overrideValues({
      editBookmarksEnabled: true,
      viewType: 0,
    });

    originalOnDragEnter = chrome.bookmarkManagerPrivate.onDragEnter;
    originalOnDragLeave = chrome.bookmarkManagerPrivate.onDragLeave;
    mockOnDragEnter = new FakeChromeEvent();
    mockOnDragLeave = new FakeChromeEvent();
    (chrome.bookmarkManagerPrivate as Record<string, unknown>)['onDragEnter'] =
        mockOnDragEnter;
    (chrome.bookmarkManagerPrivate as Record<string, unknown>)['onDragLeave'] =
        mockOnDragLeave;

    document.body.style.height = '1000px';
    const app = document.createElement('power-bookmarks-app');
    app.style.height = '1000px';
    app.style.display = 'block';
    document.body.appendChild(app);
    delegate = app.$.bookmarksList;

    await bookmarksApi.whenCalled('getAllBookmarks');
    await microtasksFinished();
  });

  teardown(() => {
    (chrome.bookmarkManagerPrivate as Record<string, unknown>)['onDragEnter'] =
        originalOnDragEnter;
    (chrome.bookmarkManagerPrivate as Record<string, unknown>)['onDragLeave'] =
        originalOnDragLeave;
  });

  test('DragStartCallsAPI', () => {
    let calledIds;
    let calledIndex;
    let calledX;
    let calledY;
    let calledTouch = false;
    chrome.bookmarkManagerPrivate.startDrag =
        (ids: string[], index: number, touch: boolean, x: number,
         y: number) => {
          calledIds = ids;
          calledIndex = index;
          calledTouch = touch;
          calledX = x;
          calledY = y;
        };

    const draggableBookmark = getBookmarkRow('5')!;
    draggableBookmark.dispatchEvent(new DragEvent(
        'dragstart',
        {bubbles: true, composed: true, clientX: 100, clientY: 200}));

    assertDeepEquals(['5'], calledIds);
    assertEquals(0, calledIndex);
    assertFalse(calledTouch);
    assertEquals(100, calledX);
    assertEquals(200, calledY);
  });

  test('TouchInteractionPreventsDrag', () => {
    let calledIds;
    chrome.bookmarkManagerPrivate.startDrag = (ids: string[]) => {
      calledIds = ids;
    };

    const draggableBookmark = getBookmarkRow('5')!;
    // Simulate touch interaction
    draggableBookmark.dispatchEvent(new PointerEvent(
        'pointerdown', {bubbles: true, composed: true, pointerType: 'touch'}));

    draggableBookmark.dispatchEvent(new DragEvent(
        'dragstart',
        {bubbles: true, composed: true, clientX: 100, clientY: 200}));

    // startDrag should NOT have been called
    assertEquals(undefined, calledIds);
  });

  test('DragOverUpdatesAttributes', () => {
    chrome.bookmarkManagerPrivate.startDrag = () => {};
    const draggedBookmark = getBookmarkRow('4')!;
    draggedBookmark.dispatchEvent(new DragEvent(
        'dragstart', {bubbles: true, composed: true, clientX: 0, clientY: 0}));

    function assertDropPosition(
        dragOverElement: HTMLElement, dropPosition: DropPosition) {
      const dragOverRect = dragOverElement.getBoundingClientRect();
      dragOverElement.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * 0.5),
      }));
      assertEquals(
          dropPosition, dragOverElement.getAttribute(DROP_POSITION_ATTR));
    }

    const dragOverFolder = getBookmarkRow('5')!;
    assertDropPosition(dragOverFolder, DropPosition.INTO);
  });

  test('DropsIntoFolder', () => {
    chrome.bookmarkManagerPrivate.startDrag = () => {};

    const draggedBookmark = getBookmarkRow('4')!;
    draggedBookmark.dispatchEvent(new DragEvent(
        'dragstart', {bubbles: true, composed: true, clientX: 0, clientY: 0}));

    const dropFolder = getBookmarkRow('5')!;
    const dragOverRect = dropFolder.getBoundingClientRect();
    dropFolder.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: dragOverRect.left,
      clientY: dragOverRect.top + (dragOverRect.height * .5),
    }));
    dropFolder.dispatchEvent(
        new DragEvent('drop', {bubbles: true, composed: true}));

    assertEquals(1, bookmarksApi.getCallCount('dropBookmarks'));
    assertEquals('5', bookmarksApi.getArgs('dropBookmarks')[0]);
  });

  test('HasActiveDrag', () => {
    chrome.bookmarkManagerPrivate.startDrag = () => {};

    const draggedBookmark = getBookmarkRow('4')!;
    draggedBookmark.dispatchEvent(new DragEvent(
        'dragstart', {bubbles: true, composed: true, clientX: 0, clientY: 0}));

    assertTrue(delegate.getDragManagerForTesting().hasActiveDrag());

    const dropFolder = getBookmarkRow('5')!;
    const dragOverRect = dropFolder.getBoundingClientRect();
    dropFolder.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: dragOverRect.left,
      clientY: dragOverRect.top + (dragOverRect.height * .5),
    }));
    dropFolder.dispatchEvent(
        new DragEvent('drop', {bubbles: true, composed: true}));

    assertFalse(delegate.getDragManagerForTesting().hasActiveDrag());
  });

  suite('WithTreeView', () => {
    setup(() => {
    });

    test('CancelsDropOntoSelf', () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      const draggedFolder = getBookmarkRow('5')!;
      draggedFolder.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      const dropFolder = getBookmarkRow('5')!;
      const dragOverRect = dropFolder.getBoundingClientRect();
      dropFolder.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));
      assertEquals(
          DropPosition.INTO, dropFolder.getAttribute(DROP_POSITION_ATTR));

      dropFolder.dispatchEvent(
          new DragEvent('drop', {bubbles: true, composed: true}));

      // The drop is cancelled instead of using the fallback bookmark.
      assertEquals(0, bookmarksApi.getCallCount('dropBookmarks'));
      assertFalse(dropFolder.hasAttribute(DROP_POSITION_ATTR));
    });

    test('HighlightsAndCancelsDropOntoParent', async () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      // Expand a folder so we can render its children in the DOM.
      const folderToExpand = getBookmarkRow('5')!;
      const expandButton =
          folderToExpand.currentListItem_.shadowRoot.querySelector<HTMLElement>(
              '#expandButton')!;
      expandButton.click();
      await microtasksFinished();

      // Drag a child bookmark (ID: 6) which is nested inside Folder 5.
      const draggedBookmark = getBookmarkRow('6')!;
      draggedBookmark.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      // Hover over the parent folder (Folder 5) of the dragged bookmark.
      const dropFolder = getBookmarkRow('5')!;
      const dragOverRect = dropFolder.getBoundingClientRect();
      dropFolder.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      // Both the parent folder and child are highlighted to show the drop
      // group.
      assertEquals(
          DropPosition.INTO, dropFolder.getAttribute(DROP_POSITION_ATTR));
      assertEquals(
          DropPosition.INTO, draggedBookmark.getAttribute(DROP_POSITION_ATTR));

      // Drop the bookmark onto its parent.
      dropFolder.dispatchEvent(
          new DragEvent('drop', {bubbles: true, composed: true}));

      // The drop is cancelled because the item is already in this folder.
      // The highlight is cleared.
      assertEquals(0, bookmarksApi.getCallCount('dropBookmarks'));
      assertFalse(dropFolder.hasAttribute(DROP_POSITION_ATTR));
      assertFalse(draggedBookmark.hasAttribute(DROP_POSITION_ATTR));
    });

    test('DropsIntoParentOfHoveredBookmark', async () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      // Expand Folder 5 to render its children.
      const folderToExpand = getBookmarkRow('5')!;
      const expandButton =
          folderToExpand.currentListItem_.shadowRoot.querySelector<HTMLElement>(
              '#expandButton')!;
      expandButton.click();
      await microtasksFinished();

      // Drag a bookmark from the top level (ID: 4).
      const draggedBookmark = getBookmarkRow('4')!;
      draggedBookmark.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      // Hover over the child bookmark (ID: 6) inside Folder 5.
      const dropBookmark = getBookmarkRow('6')!;
      const dragOverRect = dropBookmark.getBoundingClientRect();
      dropBookmark.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      // The target resolves to the parent folder (Folder 5).
      // Both the parent and the child bookmark are highlighted.
      const dropFolder = getBookmarkRow('5')!;
      assertEquals(
          DropPosition.INTO, dropFolder.getAttribute(DROP_POSITION_ATTR));
      assertEquals(
          DropPosition.INTO, dropBookmark.getAttribute(DROP_POSITION_ATTR));

      // Drop the bookmark onto the child.
      dropBookmark.dispatchEvent(
          new DragEvent('drop', {bubbles: true, composed: true}));

      // The drop executes (1 API call) and moves the bookmark into the
      // parent folder (Folder 5). Highlights are cleared.
      assertEquals(1, bookmarksApi.getCallCount('dropBookmarks'));
      assertEquals('5', bookmarksApi.getArgs('dropBookmarks')[0]);
      assertFalse(dropFolder.hasAttribute(DROP_POSITION_ATTR));
      assertFalse(dropBookmark.hasAttribute(DROP_POSITION_ATTR));
    });

    test('UsesFallbackToDropOnTopLevelFolder', async () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      const folderToExpand = getBookmarkRow('5')!;
      const expandButton =
          folderToExpand.currentListItem_.shadowRoot.querySelector<HTMLElement>(
              '#expandButton')!;
      expandButton.click();
      await microtasksFinished();

      const draggedBookmark = getBookmarkRow('6')!;
      draggedBookmark.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      const dropBookmark = getBookmarkRow('3')!;
      const dragOverRect = dropBookmark.getBoundingClientRect();
      dropBookmark.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      assertEquals(
          DropPosition.INTO, delegate.getAttribute(DROP_POSITION_ATTR));

      dropBookmark.dispatchEvent(
          new DragEvent('drop', {bubbles: true, composed: true}));

      assertEquals(1, bookmarksApi.getCallCount('dropBookmarks'));
      assertEquals(
          'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          bookmarksApi.getArgs('dropBookmarks')[0]);
      assertFalse(delegate.hasAttribute(DROP_POSITION_ATTR));
    });

    test('HighlightsCorrectlyAfterLeavingAndReentering', async () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      // Expand Folder 5 to render its children.
      const folderToExpand = getBookmarkRow('5')!;
      const expandButton =
          folderToExpand.currentListItem_.shadowRoot.querySelector<HTMLElement>(
              '#expandButton')!;
      expandButton.click();
      await microtasksFinished();

      // Start dragging child bookmark 6.
      const draggedBookmark = getBookmarkRow('6')!;
      draggedBookmark.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      const dropBookmark = getBookmarkRow('3')!;
      const dragOverRect = dropBookmark.getBoundingClientRect();

      // Drag over a top-level URL bookmark.
      dropBookmark.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));
      // The root target is the active view, so the list container highlights.
      assertEquals(
          DropPosition.INTO, delegate.getAttribute(DROP_POSITION_ATTR));

      // Simulate dragging the mouse outside the list element DOM boundaries.
      delegate.dispatchEvent(
          new DragEvent('dragleave', {bubbles: true, composed: true}));
      assertFalse(delegate.hasAttribute(DROP_POSITION_ATTR));

      // Simulate dragging the mouse completely outside the Chrome window.
      mockOnDragLeave.callListeners();

      // Simulate dragging the mouse back into the Chrome window,
      // supplying mock drag data to restore the session.
      const fakeDragData = {
        elements: [{
          id: '6',
          parentId: '5',
          title: 'Nested bookmark',
          url: 'http://nested/bookmark/',
        }],
        sameProfile: true,
      };
      mockOnDragEnter.callListeners(fakeDragData);

      // Hover over the top-level URL bookmark again.
      dropBookmark.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      // The container highlight should be restored correctly.
      assertEquals(
          DropPosition.INTO, delegate.getAttribute(DROP_POSITION_ATTR));

      // Drop the bookmark.
      dropBookmark.dispatchEvent(
          new DragEvent('drop', {bubbles: true, composed: true}));
      assertFalse(delegate.hasAttribute(DROP_POSITION_ATTR));
    });

    test('DragFolderIntoFolderNotExpanded', () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      // Drag "Sibling folder" (ID: 7).
      const draggedFolder = getBookmarkRow('7')!;
      draggedFolder.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      // Hover over "Child folder" (ID: 5) which is currently closed.
      const dropFolder = getBookmarkRow('5')!;
      const dragOverRect = dropFolder.getBoundingClientRect();
      dropFolder.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      // Only the target folder row itself should show the highlight.
      assertEquals(
          DropPosition.INTO, dropFolder.getAttribute(DROP_POSITION_ATTR));

      // Since the folder is closed, its children are not visible and
      // should not be highlighted (they do not exist in the DOM).
      assertFalse(!!getBookmarkRow('6'));
    });

    test('DragFolderIntoFolderExpanded', async () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      // Expand Folder 5 first to render its children.
      const folderToExpand = getBookmarkRow('5')!;
      const expandButton =
          folderToExpand.currentListItem_.shadowRoot.querySelector<HTMLElement>(
              '#expandButton')!;
      expandButton.click();
      await microtasksFinished();

      // Drag "Sibling folder" (ID: 7).
      const draggedFolder = getBookmarkRow('7')!;
      draggedFolder.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      // Hover over Folder 5.
      const dropFolder = getBookmarkRow('5')!;
      const dragOverRect = dropFolder.getBoundingClientRect();
      dropFolder.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      // Folder 5 and all of its visible children (6 and 10) should
      // highlight together (group highlight).
      assertEquals(
          DropPosition.INTO, dropFolder.getAttribute(DROP_POSITION_ATTR));

      const childBookmark = getBookmarkRow('6')!;
      assertEquals(
          DropPosition.INTO, childBookmark.getAttribute(DROP_POSITION_ATTR));

      const subFolder = getBookmarkRow('10')!;
      assertEquals(
          DropPosition.INTO, subFolder.getAttribute(DROP_POSITION_ATTR));

      // Child 11 is nested inside "Sub folder" 10, which is closed, so
      // 11 should not highlight.
      assertFalse(!!getBookmarkRow('11'));
    });

    test('DragFolderIntoSubFolder', async () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      // Expand Folder 5.
      const folder5 = getBookmarkRow('5')!;
      folder5.currentListItem_.shadowRoot
          .querySelector<HTMLElement>('#expandButton')!.click();
      await microtasksFinished();

      // Expand "Sub folder" (ID: 10) inside Folder 5.
      const folder10 = getBookmarkRow('10')!;
      folder10.currentListItem_.shadowRoot
          .querySelector<HTMLElement>('#expandButton')!.click();
      await microtasksFinished();

      // Drag "Sibling folder" (ID: 7).
      const draggedFolder = getBookmarkRow('7')!;
      draggedFolder.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      // Hover over the subfolder (Folder 10).
      const dropFolder = getBookmarkRow('10')!;
      const dragOverRect = dropFolder.getBoundingClientRect();
      dropFolder.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      // The subfolder and its visible child (11) should highlight.
      assertEquals(
          DropPosition.INTO, dropFolder.getAttribute(DROP_POSITION_ATTR));

      const deepBookmark = getBookmarkRow('11')!;
      assertEquals(
          DropPosition.INTO, deepBookmark.getAttribute(DROP_POSITION_ATTR));

      // The grandparent folder (Folder 5) should NOT show highlight.
      assertFalse(folder5.hasAttribute(DROP_POSITION_ATTR));
    });

    test('DragBookmarkIntoSubFolder', async () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      // Expand Folder 5 and Sub Folder 10 to expose their contents.
      getBookmarkRow('5')!.currentListItem_.shadowRoot
          .querySelector<HTMLElement>('#expandButton')!.click();
      await microtasksFinished();
      getBookmarkRow('10')!.currentListItem_.shadowRoot
          .querySelector<HTMLElement>('#expandButton')!.click();
      await microtasksFinished();

      // Drag a top-level URL bookmark (ID: 3).
      const draggedBookmark = getBookmarkRow('3')!;
      draggedBookmark.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      // Hover over the subfolder (Folder 10).
      const dropFolder = getBookmarkRow('10')!;
      const dragOverRect = dropFolder.getBoundingClientRect();
      dropFolder.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      // Folder 10 and its visible child (11) should highlight.
      assertEquals(
          DropPosition.INTO, dropFolder.getAttribute(DROP_POSITION_ATTR));
      assertEquals(
          DropPosition.INTO,
          getBookmarkRow('11')!.getAttribute(DROP_POSITION_ATTR));
    });

    test('DragFolderOntoItsOwnSubFolder', async () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      // Expand Folder 5.
      getBookmarkRow('5')!.currentListItem_.shadowRoot
          .querySelector<HTMLElement>('#expandButton')!.click();
      await microtasksFinished();

      // Drag Folder 5.
      const draggedFolder = getBookmarkRow('5')!;
      draggedFolder.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      // Hover over its own subfolder (Folder 10). This is a loop drop target.
      const dropFolder = getBookmarkRow('10')!;
      const dragOverRect = dropFolder.getBoundingClientRect();
      dropFolder.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      // The subfolder should highlight to show hover feedback.
      assertEquals(
          DropPosition.INTO, dropFolder.getAttribute(DROP_POSITION_ATTR));

      // Drop the folder onto the subfolder.
      dropFolder.dispatchEvent(
          new DragEvent('drop', {bubbles: true, composed: true}));

      // The drop must be cancelled (0 API calls) because a folder cannot be
      // dropped inside its own children. Highlights should be cleared.
      assertEquals(0, bookmarksApi.getCallCount('dropBookmarks'));
      assertFalse(dropFolder.hasAttribute(DROP_POSITION_ATTR));
    });

    test('DragBookmarkOntoSiblingBookmarkTopLevel', () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      // Drag a top-level bookmark (ID: 3).
      const draggedBookmark = getBookmarkRow('3')!;
      draggedBookmark.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      // Hover over its sibling bookmark (ID: 4) at the top level.
      const dropBookmark = getBookmarkRow('4')!;
      const dragOverRect = dropBookmark.getBoundingClientRect();
      dropBookmark.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      // The target parent is the root folder. Since it is the root, we
      // highlight the entire container.
      assertEquals(
          DropPosition.INTO, delegate.getAttribute(DROP_POSITION_ATTR));

      // Drop the bookmark.
      dropBookmark.dispatchEvent(
          new DragEvent('drop', {bubbles: true, composed: true}));

      // The drop must be cancelled (0 API calls) because it is a same-parent
      // drop (sibling move is a no-op). Highlights should be cleared.
      assertEquals(0, bookmarksApi.getCallCount('dropBookmarks'));
      assertFalse(delegate.hasAttribute(DROP_POSITION_ATTR));
    });

    test('DragFolderOntoSiblingFolderTopLevel', () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      // Drag Folder 5.
      const draggedFolder = getBookmarkRow('5')!;
      draggedFolder.dispatchEvent(new DragEvent(
          'dragstart',
          {bubbles: true, composed: true, clientX: 0, clientY: 0}));

      // Hover over sibling folder (Folder 7).
      const dropFolder = getBookmarkRow('7')!;
      const dragOverRect = dropFolder.getBoundingClientRect();
      dropFolder.dispatchEvent(new DragEvent('dragover', {
        bubbles: true,
        composed: true,
        clientX: dragOverRect.left,
        clientY: dragOverRect.top + (dragOverRect.height * .5),
      }));

      // Sibling folder should show the hover highlight.
      assertEquals(
          DropPosition.INTO, dropFolder.getAttribute(DROP_POSITION_ATTR));
    });
  });
});
