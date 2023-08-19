// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/bookmarks_list.js';

import {BookmarkFolderElement} from 'chrome://bookmarks-side-panel.top-chrome/bookmark_folder.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {BookmarksListElement} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_list.js';
import {ShoppingListApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {down, keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestShoppingListApiProxy} from './commerce/test_shopping_list_api_proxy.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelBookmarksListFocusTest', () => {
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
        },
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
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(JSON.parse(JSON.stringify(folders)));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    const shoppingListApi = new TestShoppingListApiProxy();
    ShoppingListApiProxyImpl.setInstance(shoppingListApi);

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

    // Hide focus states and focus.
    FocusOutlineManager.forDocument(document).visible = false;
    bookmarkElement.focus();
    assertEquals(bookmarkElement, folderElement.shadowRoot!.activeElement);

    // When focus is hidden, keyboard shortcuts should not be allowed.
    keyDownOn(bookmarkElement, 0, ['ctrl'], 'x');
    assertEquals(0, bookmarksApi.getCallCount('cutBookmark'));

    // Show focus states, which should allow keyboard shortcuts.
    FocusOutlineManager.forDocument(document).visible = true;
    keyDownOn(bookmarkElement, 0, ['ctrl'], 'x');
    const cutId = await bookmarksApi.whenCalled('cutBookmark');
    assertEquals(1, bookmarksApi.getCallCount('cutBookmark'));
    assertEquals('3', cutId);

    keyDownOn(bookmarkElement, 0, ['ctrl'], 'c');
    const copiedId = await bookmarksApi.whenCalled('copyBookmark');
    assertEquals('3', copiedId);

    keyDownOn(bookmarkElement, 0, ['ctrl'], 'v');
    const [pastedId, pastedDestinationId] =
        await bookmarksApi.whenCalled('pasteToBookmark');
    assertEquals('0', pastedId);
    assertEquals('3', pastedDestinationId);
  });

  test('ShowsFocusStateOnDrop', () => {
    const focusOutlineManager = FocusOutlineManager.forDocument(document);

    // Mousedown to hide focus state initially.
    down(bookmarksList, {x: 0, y: 0});
    assertFalse(focusOutlineManager.visible);

    // Perform a drop and assert that focus state is visible.
    bookmarksList.onFinishDrop([folders[0]!.children![0]!]);
    assertTrue(focusOutlineManager.visible);

    // Make sure on the next mouse event, the focus state gets rehidden.
    down(bookmarksList, {x: 0, y: 0});
    assertFalse(focusOutlineManager.visible);
  });
});
