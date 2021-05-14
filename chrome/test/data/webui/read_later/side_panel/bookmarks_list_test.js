// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReadLaterUI is a Mojo WebUI controller and therefore needs mojo defined to
// finish running its tests.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {BookmarkFolderElement} from 'chrome://read-later.top-chrome/side_panel/bookmark_folder.js';
import {BookmarksApiProxyImpl} from 'chrome://read-later.top-chrome/side_panel/bookmarks_api_proxy.js';
import {BookmarksListElement} from 'chrome://read-later.top-chrome/side_panel/bookmarks_list.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelBookmarksListTest', () => {
  /** @type {!BookmarksListElement} */
  let bookmarksList;

  /** @type {!TestBookmarksApiProxy} */
  let bookmarksApi;

  /** @type {!Array<!chrome.bookmarks.BookmarkTreeNode>} */
  const folders = [
    {
      id: '0',
      title: 'Bookmarks bar',
      children: [
        {
          id: '3',
          title: 'Child bookmark',
          url: 'http://child/bookmark/',
        },
        {
          id: '4',
          title: 'Child folder',
          children: [
            {
              id: '5',
              title: 'Nested bookmark',
              url: 'http://nested/bookmark/',
            },
          ],
        }
      ],
    },
    {
      id: '1',
      title: 'Other bookmarks',
      children: [],
    },
    {
      id: '2',
      title: 'Mobile bookmarks',
      children: [],
    },
  ];

  /**
   * @param {!HTMLElement} root
   * @return {!Array<!BookmarkFolderElement>}
   */
  function getFolderElements(root) {
    return /** @type {!Array<!BookmarkFolderElement>} */ (
        Array.from(root.shadowRoot.querySelectorAll('bookmark-folder')));
  }

  /**
   * @param {!HTMLElement} root
   * @return {!Array<!HTMLElement>}
   */
  function getBookmarkElements(root) {
    return /** @type {!Array<!HTMLElement>} */ (
        Array.from(root.shadowRoot.querySelectorAll('.bookmark')));
  }

  setup(async () => {
    document.body.innerHTML = '';

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(
        /** @type {!Array<!chrome.bookmarks.BookmarkTreeNode>} */ (
            JSON.parse(JSON.stringify(folders))));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    bookmarksList = /** @type {!BookmarksListElement} */ (
        document.createElement('bookmarks-list'));
    document.body.appendChild(bookmarksList);

    await flushTasks();
  });

  test('GetsAndShowsFolders', () => {
    assertEquals(1, bookmarksApi.getCallCount('getFolders'));
    assertEquals(folders.length, getFolderElements(bookmarksList).length);
  });

  test('OpensFirstFolderByDefault', () => {
    assertTrue(getFolderElements(bookmarksList)[0].openByDefault);
  });

  test('UpdatesChangedBookmarks', () => {
    const rootFolderIndex = 0;
    const bookmarkIndex = 0;

    const changedBookmark = folders[rootFolderIndex].children[bookmarkIndex];
    bookmarksApi.callbackRouter.onChanged.dispatchEvent(changedBookmark.id, {
      title: 'New title',
      url: 'http://new/url',
    });

    const folderElement = getFolderElements(bookmarksList)[rootFolderIndex];
    const bookmarkElement = getBookmarkElements(folderElement)[bookmarkIndex];
    assertEquals('New title', bookmarkElement.innerText);
    assertEquals('http://new/url', bookmarkElement.href);
  });

  test('UpdatesReorderedChildren', () => {
    // Reverse the children of Bookmarks bar.
    const children = folders[0].children;
    const reverseOrder = children.map(child => child.id).reverse();
    bookmarksApi.callbackRouter.onChildrenReordered.dispatchEvent(
        folders[0].id, {childIds: reverseOrder});
    flush();

    const rootFolderElement = getFolderElements(bookmarksList)[0];
    const childFolder = getFolderElements(rootFolderElement)[0];
    const childBookmark = getBookmarkElements(rootFolderElement)[0];
    assertTrue(
        !!(childFolder.compareDocumentPosition(childBookmark) &
           Node.DOCUMENT_POSITION_FOLLOWING));
  });

  test('AddsCreatedBookmark', () => {
    bookmarksApi.callbackRouter.onCreated.dispatchEvent('999', {
      id: '999',
      title: 'New bookmark',
      index: 0,
      parentId: '4',
      url: '//new/bookmark',
    });
    flush();

    const rootFolderElement = getFolderElements(bookmarksList)[0];
    const childFolder = getFolderElements(rootFolderElement)[0];
    const childFolderBookmarks = getBookmarkElements(childFolder);
    assertEquals(2, childFolderBookmarks.length);
  });

  test('MovesBookmarks', () => {
    const movedBookmark = folders[0].children[1].children[0];
    bookmarksApi.callbackRouter.onMoved.dispatchEvent(movedBookmark.id, {
      index: 0,
      parentId: folders[0].id,                 // Moving to bookmarks bar.
      oldParentId: folders[0].children[1].id,  // Moving from child folder.
      oldIndex: 0,
    });
    flush();

    const bookmarksBarFolder = getFolderElements(bookmarksList)[0];
    const movedBookmarkElement = getBookmarkElements(bookmarksBarFolder)[0];
    assertEquals('Nested bookmark', movedBookmarkElement.innerText);

    const childFolder = getFolderElements(bookmarksBarFolder)[0];
    const childFolderBookmarks = getBookmarkElements(childFolder);
    assertEquals(0, childFolderBookmarks.length);
  });
});