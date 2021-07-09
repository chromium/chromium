// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReadLaterUI is a Mojo WebUI controller and therefore needs mojo defined to
// finish running its tests.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {BookmarkFolderElement, FOLDER_OPEN_CHANGED_EVENT} from 'chrome://read-later.top-chrome/side_panel/bookmark_folder.js';
import {BookmarksApiProxy} from 'chrome://read-later.top-chrome/side_panel/bookmarks_api_proxy.js';
import {BookmarksListElement, LOCAL_STORAGE_OPEN_FOLDERS_KEY} from 'chrome://read-later.top-chrome/side_panel/bookmarks_list.js';
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
    window.localStorage[LOCAL_STORAGE_OPEN_FOLDERS_KEY] = undefined;
    document.body.innerHTML = '';

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(
        /** @type {!Array<!chrome.bookmarks.BookmarkTreeNode>} */ (
            JSON.parse(JSON.stringify(folders))));
    BookmarksApiProxy.setInstance(bookmarksApi);

    bookmarksList = /** @type {!BookmarksListElement} */ (
        document.createElement('bookmarks-list'));
    document.body.appendChild(bookmarksList);

    await flushTasks();
  });

  test('GetsAndShowsFolders', () => {
    assertEquals(1, bookmarksApi.getCallCount('getFolders'));
    assertEquals(folders.length, getFolderElements(bookmarksList).length);
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

  test('AddsCreatedBookmark', async () => {
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
    childFolder.shadowRoot.querySelector('.row').click();  // Open folder.
    await flushTasks();
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

  test('DefaultsToFirstFolderBeingOpen', () => {
    assertEquals(
        JSON.stringify([folders[0].id]),
        window.localStorage[LOCAL_STORAGE_OPEN_FOLDERS_KEY]);
  });

  test('UpdatesLocalStorageOnFolderOpenChanged', () => {
    bookmarksList.dispatchEvent(new CustomEvent(FOLDER_OPEN_CHANGED_EVENT, {
      bubbles: true,
      composed: true,
      detail: {
        id: folders[0].id,
        open: false,
      }
    }));
    assertEquals(
        JSON.stringify([]),
        window.localStorage[LOCAL_STORAGE_OPEN_FOLDERS_KEY]);

    bookmarksList.dispatchEvent(new CustomEvent(FOLDER_OPEN_CHANGED_EVENT, {
      bubbles: true,
      composed: true,
      detail: {
        id: '5001',
        open: true,
      }
    }));
    assertEquals(
        JSON.stringify(['5001']),
        window.localStorage[LOCAL_STORAGE_OPEN_FOLDERS_KEY]);
  });

  test('MovesFocusBetweenFolders', () => {
    const folderElements = getFolderElements(bookmarksList);

    /** @param {string} key */
    function dispatchArrowKey(key) {
      bookmarksList.dispatchEvent(new KeyboardEvent('keydown', {key}));
    }

    /** @param {number} index */
    function assertActiveElement(index) {
      assertEquals(
          folderElements[index], bookmarksList.shadowRoot.activeElement);
    }

    // Move focus to the first folder.
    folderElements[0].moveFocus(1);
    assertActiveElement(0);

    // One ArrowDown key should still keep focus in the first folder since the
    // folder has children.
    dispatchArrowKey('ArrowDown');
    assertActiveElement(0);

    // Two ArrowsDown to eventually make it to the second folder.
    dispatchArrowKey('ArrowDown');
    dispatchArrowKey('ArrowDown');
    assertActiveElement(1);

    // One ArrowsDown to eventually make it to the third folder.
    dispatchArrowKey('ArrowDown');
    assertActiveElement(2);

    // One ArrowsDown to loop back to the first folder.
    dispatchArrowKey('ArrowDown');
    assertActiveElement(0);

    // One ArrowUp to loop back to the last folder.
    dispatchArrowKey('ArrowUp');
    assertActiveElement(2);

    // One ArrowUp to loop back to the second folder.
    dispatchArrowKey('ArrowUp');
    assertActiveElement(1);
  });
});
