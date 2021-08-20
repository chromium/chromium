// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {BookmarksApiProxy} from 'chrome://read-later.top-chrome/side_panel/bookmarks_api_proxy.js';
import {BookmarksDragManager} from 'chrome://read-later.top-chrome/side_panel/bookmarks_drag_manager.js';
import {BookmarksListElement} from 'chrome://read-later.top-chrome/side_panel/bookmarks_list.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertDeepEquals, assertEquals, assertFalse} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelBookmarkDragManagerTest', () => {
  /** @type {!BookmarkDragManager} */
  let bookmarkDragManager;

  /** @type {!BookmarksListElement} */
  let delegate;

  /** @type {!Array<!chrome.bookmarks.BookmarkTreeNode>} */
  const folders = [{
    id: '1',
    title: 'Bookmarks bar',
    parentId: '0',
    children: [
      {
        id: '2',
        title: 'Google',
        url: 'http://google.com',
      },
    ],
  }];

  function getDraggableBookmarkByIndex(index) {
    return delegate.shadowRoot.querySelector('bookmark-folder')
        .shadowRoot.querySelectorAll('.bookmark')[index];
  }

  setup(async () => {
    document.body.innerHTML = '';

    loadTimeData.overrideValues({
      bookmarksDragAndDropEnabled: true,
    });

    const bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(
        /** @type {!Array<!chrome.bookmarks.BookmarkTreeNode>} */ (
            JSON.parse(JSON.stringify(folders))));
    BookmarksApiProxy.setInstance(bookmarksApi);

    delegate = new BookmarksListElement();
    bookmarkDragManager = new BookmarksDragManager(delegate);
    document.body.appendChild(delegate);

    await flushTasks();
  });

  test('DragStartCallsAPI', () => {
    let calledIds, calledIndex, calledTouch, calledX, calledY;
    chrome.bookmarkManagerPrivate.startDrag = (ids, index, touch, x, y) => {
      calledIds = ids;
      calledIndex = index;
      calledTouch = touch;
      calledX = x;
      calledY = y;
    };

    const draggableBookmark = getDraggableBookmarkByIndex(0);
    draggableBookmark.dispatchEvent(new DragEvent(
        'dragstart',
        {bubbles: true, composed: true, clientX: 100, clientY: 200}));

    assertDeepEquals(['2'], calledIds);
    assertEquals(0, calledIndex);
    assertFalse(calledTouch);
    assertEquals(100, calledX);
    assertEquals(200, calledY);
  });
});