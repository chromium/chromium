// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {ShoppingListApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/commerce/shopping_list_api_proxy.js';
import {PowerBookmarkRowElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row.js';
import {PowerBookmarksListElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';

import {TestShoppingListApiProxy} from './commerce/test_shopping_list_api_proxy.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelPowerBookmarksListTest', () => {
  let powerBookmarksList: PowerBookmarksListElement;
  let bookmarksApi: TestBookmarksApiProxy;
  let shoppingListApi: TestShoppingListApiProxy;

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

  function getBookmarkElements(root: HTMLElement): HTMLElement[] {
    return Array.from(root.shadowRoot!.querySelectorAll('power-bookmark-row'));
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(JSON.parse(JSON.stringify(folders)));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    shoppingListApi = new TestShoppingListApiProxy();
    ShoppingListApiProxyImpl.setInstance(shoppingListApi);

    const pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);

    powerBookmarksList = document.createElement('power-bookmarks-list');
    document.body.appendChild(powerBookmarksList);

    await bookmarksApi.whenCalled('getFolders');
    await flushTasks();
  });

  test('GetsAndShowsTopLevelBookmarks', () => {
    assertEquals(1, bookmarksApi.getCallCount('getFolders'));
    assertEquals(
        folders[0]!.children!.length,
        getBookmarkElements(powerBookmarksList).length);
  });

  test('DefaultsToSortByNewest', () => {
    const bookmarkElements = getBookmarkElements(powerBookmarksList);
    // All folders should come first
    assertEquals(bookmarkElements[0]!.id, 'bookmark-5');
    // Newest URL should come next
    assertEquals(bookmarkElements[1]!.id, 'bookmark-4');
    // Older URL should be last
    assertEquals(bookmarkElements[2]!.id, 'bookmark-3');
  });

  test('UpdatesChangedBookmarks', () => {
    const changedBookmark = folders[0]!.children![0]!;
    bookmarksApi.callbackRouter.onChanged.callListeners(changedBookmark.id, {
      title: 'New title',
      url: 'http://new/url',
    });
    flush();

    const bookmarkElement = getBookmarkElements(powerBookmarksList)[2]!;
    assertEquals(
        'New title',
        (bookmarkElement as PowerBookmarkRowElement).bookmark.title);
    assertEquals(
        'http://new/url',
        (bookmarkElement as PowerBookmarkRowElement).bookmark.url);
    assertNotEquals(
        undefined,
        Array.from(bookmarkElement.shadowRoot!.querySelectorAll('button'))
            .find(
                el => el.textContent && el.textContent.trim() === 'New title'));
  });

  test('AddsCreatedBookmark', async () => {
    bookmarksApi.callbackRouter.onCreated.callListeners('999', {
      id: '999',
      title: 'New bookmark',
      index: 0,
      parentId: folders[0]!.id,
      url: 'http://new/bookmark',
    });
    flush();

    const bookmarkElements = getBookmarkElements(powerBookmarksList);
    assertEquals(4, bookmarkElements.length);
  });

  test('AddsCreatedBookmarkForNewFolder', () => {
    // Create a new folder without a children array.
    bookmarksApi.callbackRouter.onCreated.callListeners('1000', {
      id: '1000',
      title: 'New folder',
      index: 0,
      parentId: folders[0]!.id,
    });
    flush();

    // Create a new bookmark within that folder.
    bookmarksApi.callbackRouter.onCreated.callListeners('1001', {
      id: '1001',
      title: 'New bookmark in new folder',
      index: 0,
      parentId: '1000',
      url: 'http://google.com',
    });
    flush();

    const newFolder = getBookmarkElements(powerBookmarksList)[0]!;
    assertEquals(
        1, (newFolder as PowerBookmarkRowElement).bookmark.children!.length);
  });

  test('MovesBookmarks', () => {
    const movedBookmark = folders[0]!.children![2]!.children![0]!;
    bookmarksApi.callbackRouter.onMoved.callListeners(movedBookmark.id, {
      index: 0,
      parentId: folders[0]!.id,                   // Moving to other bookmarks.
      oldParentId: folders[0]!.children![2]!.id,  // Moving from child folder.
      oldIndex: 0,
    });
    flush();

    const bookmarkElements = getBookmarkElements(powerBookmarksList);
    assertEquals(4, bookmarkElements.length);
    const childFolder = bookmarkElements[0]!;
    assertEquals('5', (childFolder as PowerBookmarkRowElement).bookmark.id);
    assertEquals(
        0, (childFolder as PowerBookmarkRowElement).bookmark.children!.length);
  });

  test('MovesBookmarksIntoNewFolder', () => {
    // Create a new folder without a children array.
    bookmarksApi.callbackRouter.onCreated.callListeners('1000', {
      id: '1000',
      title: 'New folder',
      index: 0,
      parentId: folders[0]!.id,
    });
    flush();

    const movedBookmark = folders[0]!.children![2]!.children![0]!;
    bookmarksApi.callbackRouter.onMoved.callListeners(movedBookmark.id, {
      index: 0,
      parentId: '1000',
      oldParentId: folders[0]!.children![2]!.id,
      oldIndex: 0,
    });
    flush();

    const newFolder =
        powerBookmarksList.shadowRoot!.querySelector('#bookmark-1000');
    assertEquals(
        1, (newFolder as PowerBookmarkRowElement).bookmark.children!.length);
  });

  test('SetsCompactDescription', async () => {
    const bookmarkElements = getBookmarkElements(powerBookmarksList);
    const folderElement = bookmarkElements[0]!;
    assertEquals(folderElement.id, 'bookmark-5');

    const descriptionElement =
        folderElement.shadowRoot!.getElementById('description');
    const pluralString =
        await PluralStringProxyImpl.getInstance().getPluralString('foo', 1);
    assertEquals(descriptionElement!.textContent!.includes(pluralString), true);
  });

  test('SetsExpandedDescription', () => {
    const menu =
        powerBookmarksList.shadowRoot!.querySelector('cr-action-menu')!;
    menu.showAt(powerBookmarksList);
    const visualViewButton: HTMLElement = menu.querySelector('#visualView')!;
    visualViewButton.click();

    const bookmarkElements = getBookmarkElements(powerBookmarksList);
    const folderElement = bookmarkElements[1]!;
    assertEquals(folderElement.id, 'bookmark-4');

    const descriptionElement =
        folderElement.shadowRoot!.getElementById('description');
    const expandedDescription = 'child';
    assertEquals(
        descriptionElement!.textContent!.includes(expandedDescription), true);
  });

  test('SetsExpandedSearchResultDescription', () => {
    const menu =
        powerBookmarksList.shadowRoot!.querySelector('cr-action-menu')!;
    menu.showAt(powerBookmarksList);
    const visualViewButton: HTMLElement = menu.querySelector('#visualView')!;
    visualViewButton.click();

    const searchField = powerBookmarksList.shadowRoot!.querySelector(
        'cr-toolbar-search-field')!;
    searchField.$.searchInput.value = 'child bookmark';
    searchField.onSearchTermInput();
    searchField.onSearchTermSearch();

    flush();

    const bookmarkElements = getBookmarkElements(powerBookmarksList);
    const folderElement = bookmarkElements[0]!;
    assertEquals(folderElement.id, 'bookmark-4');

    const descriptionElement =
        folderElement.shadowRoot!.getElementById('description');
    const expandedDescription = 'child - All Bookmarks';
    assertEquals(
        descriptionElement!.textContent!.includes(expandedDescription), true);
  });
});
