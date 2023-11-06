// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {DROP_POSITION_ATTR, DropPosition, PowerBookmarksDragManager} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_drag_manager.js';
import {PowerBookmarksListElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelPowerBookmarkDragManagerTest', () => {
  let delegate: PowerBookmarksListElement;

  const folders: chrome.bookmarks.BookmarkTreeNode[] = [
    {
      id: '2',
      parentId: '0',
      title: 'Other Bookmarks',
      children: [
        {
          id: '3',
          parentId: '2',
          title: 'First child bookmark',
          url: 'http://child/bookmark/1/',
          dateAdded: 1,
        },
        {
          id: '4',
          parentId: '2',
          title: 'Second child bookmark',
          url: 'http://child/bookmark/2/',
          dateAdded: 3,
        },
        {
          id: '5',
          parentId: '2',
          title: 'Child folder',
          dateAdded: 2,
          children: [
            {
              id: '6',
              parentId: '5',
              title: 'Nested bookmark',
              url: 'http://nested/bookmark/',
              dateAdded: 4,
            },
          ],
        },
      ],
    },
  ];

  function getDraggableElements() {
    return delegate.shadowRoot!.querySelectorAll('power-bookmark-row');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(structuredClone(folders));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    loadTimeData.overrideValues({
      editBookmarksEnabled: true,
    });

    delegate = new PowerBookmarksListElement();
    new PowerBookmarksDragManager(delegate);
    document.body.appendChild(delegate);

    await bookmarksApi.whenCalled('getFolders');
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

    const draggableBookmark = getDraggableElements()[0]!;
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
    const draggableElements = getDraggableElements();
    const draggedBookmark = draggableElements[1]!;
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

    const dragOverFolder = draggableElements[0]!;
    assertDropPosition(dragOverFolder, DropPosition.INTO);
  });

  test('DropsIntoFolder', () => {
    let calledId;
    let calledIndex;
    chrome.bookmarkManagerPrivate.startDrag = () => {};
    chrome.bookmarkManagerPrivate.drop = (id, index) => {
      calledId = id;
      calledIndex = index;
      return Promise.resolve();
    };

    const draggableElements = getDraggableElements();
    const draggedBookmark = draggableElements[1]!;
    draggedBookmark.dispatchEvent(new DragEvent(
        'dragstart', {bubbles: true, composed: true, clientX: 0, clientY: 0}));

    const dropFolder = draggableElements[0]!;
    const dragOverRect = dropFolder.getBoundingClientRect();
    dropFolder.dispatchEvent(new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      clientX: dragOverRect.left,
      clientY: dragOverRect.top + (dragOverRect.height * .5),
    }));
    dropFolder.dispatchEvent(
        new DragEvent('drop', {bubbles: true, composed: true}));

    assertEquals('5', calledId);
    assertEquals(undefined, calledIndex);
  });
});
