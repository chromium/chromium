// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://read-later.top-chrome/side_panel/bookmarks_list.js';

import {BookmarkFolderElement} from 'chrome://read-later.top-chrome/side_panel/bookmark_folder.js';
import {BookmarksApiProxyImpl} from 'chrome://read-later.top-chrome/side_panel/bookmarks_api_proxy.js';
import {BookmarksListElement} from 'chrome://read-later.top-chrome/side_panel/bookmarks_list.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelBookmarksListInteractiveUITest', () => {
  let bookmarksList: BookmarksListElement;
  let bookmarksApi: TestBookmarksApiProxy;

  const folders: chrome.bookmarks.BookmarkTreeNode[] = [
    {
      id: '0',
      parentId: 'root',
      title: 'Bookmarks bar',
      children: [
        {
          id: '3',
          parentId: '0',
          title: 'Child bookmark',
          url: 'http://child/bookmark/',
        },
        {
          id: '4',
          parentId: '0',
          title: 'Child folder',
          children: [
            {
              id: '5',
              parentId: '4',
              title: 'Nested bookmark',
              url: 'http://nested/bookmark/',
            },
          ],
        }
      ],
    },
    {
      id: '1',
      parentId: 'root',
      title: 'Other bookmarks',
      children: [],
    },
    {
      id: '2',
      title: 'Mobile bookmarks',
      children: [],
    },
  ];

  function getFolderElements(root: HTMLElement):
      NodeListOf<BookmarkFolderElement> {
    return root.shadowRoot!.querySelectorAll('bookmark-folder');
  }

  function getBookmarkElements(root: HTMLElement): NodeListOf<HTMLElement> {
    return root.shadowRoot!.querySelectorAll('.bookmark');
  }

  setup(async () => {
    document.body.innerHTML = '';

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(JSON.parse(JSON.stringify(folders)));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    bookmarksList = document.createElement('bookmarks-list');
    document.body.appendChild(bookmarksList);
    await flushTasks();
  });

  test('MovesFocusBetweenFolders', () => {
    const folderElements = getFolderElements(bookmarksList);

    function assertActiveElement(index: number) {
      assertEquals(
          folderElements[index], bookmarksList.shadowRoot!.activeElement);
    }

    // Move focus to the first folder.
    folderElements[0]!.moveFocus(1);
    assertActiveElement(0);

    // One ArrowDown key should still keep focus in the first folder since the
    // folder has children.
    keyDownOn(bookmarksList, 0, [], 'ArrowDown');
    assertActiveElement(0);

    // Two ArrowsDown to eventually make it to the second folder.
    keyDownOn(bookmarksList, 0, [], 'ArrowDown');
    keyDownOn(bookmarksList, 0, [], 'ArrowDown');
    assertActiveElement(1);

    // One ArrowsDown to eventually make it to the third folder.
    keyDownOn(bookmarksList, 0, [], 'ArrowDown');
    assertActiveElement(2);

    // One ArrowsDown to loop back to the first folder.
    keyDownOn(bookmarksList, 0, [], 'ArrowDown');
    assertActiveElement(0);

    // One ArrowUp to loop back to the last folder.
    keyDownOn(bookmarksList, 0, [], 'ArrowUp');
    assertActiveElement(2);

    // One ArrowUp to loop back to the second folder.
    keyDownOn(bookmarksList, 0, [], 'ArrowUp');
    assertActiveElement(1);
  });

  test('CutsCopyPastesBookmark', async () => {
    const folderElement = getFolderElements(bookmarksList)[0]!;
    const bookmarkElement = getBookmarkElements(folderElement)[0]!;
    bookmarkElement.focus();
    assertEquals(bookmarkElement, folderElement.shadowRoot!.activeElement);

    keyDownOn(bookmarkElement, 0, ['ctrl'], 'x');
    const cutId = await bookmarksApi.whenCalled('cutBookmark');
    assertEquals('3', cutId);

    keyDownOn(bookmarkElement, 0, ['ctrl'], 'c');
    const copiedId = await bookmarksApi.whenCalled('copyBookmark');
    assertEquals('3', copiedId);

    keyDownOn(bookmarkElement, 0, ['ctrl'], 'v');
    let [pastedId, pastedDestinationId] =
        await bookmarksApi.whenCalled('pasteToBookmark');
    assertEquals('0', pastedId);
    assertEquals('3', pastedDestinationId);
  });
});