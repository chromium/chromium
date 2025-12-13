// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import type {BookmarksTreeNode} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {PowerBookmarkRowElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row.js';
import {DROP_POSITION_ATTR, DropPosition} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_drag_manager.js';
import {PowerBookmarksListElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelPowerBookmarkDragManagerTest', () => {
  let delegate: PowerBookmarksListElement;
  let bookmarksApi: TestBookmarksApiProxy;

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
          ],
        },
      ],
    },
  ];

  function getBookmarkRow(id: string) {
    const rows = delegate.shadowRoot!.querySelectorAll('power-bookmark-row');
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

    delegate = new PowerBookmarksListElement();
    document.body.appendChild(delegate);

    await bookmarksApi.whenCalled('getAllBookmarks');
    await flushTasks();
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
      loadTimeData.overrideValues({
        bookmarksTreeViewEnabled: true,
      });
    });

    test('CancelsDropWithinSameParent', () => {
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
      dropFolder.dispatchEvent(
          new DragEvent('drop', {bubbles: true, composed: true}));

      // The drop is cancelled instead of using the fallback bookmark.
      assertEquals(0, bookmarksApi.getCallCount('dropBookmarks'));
    });

    test('UsesFallbackToDropOnTopLevelFolder', async () => {
      chrome.bookmarkManagerPrivate.startDrag = () => {};

      const folderToExpand = getBookmarkRow('5')!;
      folderToExpand.currentUrlListItem_.click();
      await flushTasks();

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
      dropBookmark.dispatchEvent(
          new DragEvent('drop', {bubbles: true, composed: true}));

      assertEquals(1, bookmarksApi.getCallCount('dropBookmarks'));
      assertEquals(
          'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          bookmarksApi.getArgs('dropBookmarks')[0]);
    });
  });
});
