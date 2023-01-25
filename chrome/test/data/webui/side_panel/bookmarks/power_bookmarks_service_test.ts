// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {ShoppingListApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/commerce/shopping_list_api_proxy.js';
import {PowerBookmarksService} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_service.js';
import {BookmarkProductInfo} from 'chrome://bookmarks-side-panel.top-chrome/shopping_list.mojom-webui.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';

import {TestShoppingListApiProxy} from './commerce/test_shopping_list_api_proxy.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {TestPowerBookmarksDelegate} from './test_power_bookmarks_delegate.js';

suite('SidePanelPowerBookmarksServiceTest', () => {
  let delegate: TestPowerBookmarksDelegate;
  let service: PowerBookmarksService;
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

  const products: BookmarkProductInfo[] = [{
    bookmarkId: BigInt(3),
    info: {
      title: 'Product Foo',
      domain: 'foo.com',
      imageUrl: {url: 'https://foo.com/image'},
      productUrl: {url: 'https://foo.com/product'},
      currentPrice: '$12',
      previousPrice: '$34',
    },
  }];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(JSON.parse(JSON.stringify(folders)));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    shoppingListApi = new TestShoppingListApiProxy();
    shoppingListApi.setProducts(products);
    ShoppingListApiProxyImpl.setInstance(shoppingListApi);

    const pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);

    delegate = new TestPowerBookmarksDelegate();
    service = new PowerBookmarksService(delegate);
    service.startListening();

    await delegate.whenCalled('onBookmarksLoaded');
  });

  test('FiltersTopLevelBookmarks', () => {
    const defaultBookmarks =
        service.filterBookmarks(undefined, 0, undefined, []);
    assertEquals(folders[0]!.children!.length, defaultBookmarks.length);
  });

  test('FiltersByFolder', () => {
    const folder = service.findBookmarkWithId('5');
    const folderBookmarks = service.filterBookmarks(folder, 0, undefined, []);
    assertEquals(
        folders[0]!.children![2]!.children!.length, folderBookmarks.length);
  });

  test('FiltersBySearchQuery', () => {
    const searchBookmarks = service.filterBookmarks(undefined, 0, 'http', []);
    assertEquals(searchBookmarks.length, 3);
  });

  test('FiltersByPriceTracking', () => {
    const searchBookmarks = service.filterBookmarks(
        undefined, 0, undefined,
        [{label: 'Price Tracking', icon: '', active: true}]);
    assertEquals(searchBookmarks.length, 1);
    assertEquals(searchBookmarks[0]!.id, '3');
  });

  test('SortsByNewest', () => {
    const sortedBookmarks =
        service.filterBookmarks(undefined, 0, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '5');
    assertEquals(sortedBookmarks[1]!.id, '4');
    assertEquals(sortedBookmarks[2]!.id, '3');
  });

  test('SortsByOldest', () => {
    const sortedBookmarks =
        service.filterBookmarks(undefined, 1, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '5');
    assertEquals(sortedBookmarks[1]!.id, '3');
    assertEquals(sortedBookmarks[2]!.id, '4');
  });

  test('SortsByAToZ', () => {
    const sortedBookmarks =
        service.filterBookmarks(undefined, 2, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '5');
    assertEquals(sortedBookmarks[1]!.id, '3');
    assertEquals(sortedBookmarks[2]!.id, '4');
  });

  test('SortsByZToA', () => {
    const sortedBookmarks =
        service.filterBookmarks(undefined, 3, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '5');
    assertEquals(sortedBookmarks[1]!.id, '4');
    assertEquals(sortedBookmarks[2]!.id, '3');
  });

  test('CallsOnBookmarkChanged', () => {
    const changedBookmark = folders[0]!.children![0]!;
    bookmarksApi.callbackRouter.onChanged.callListeners(changedBookmark.id, {
      title: 'New title',
      url: 'http://new/url',
    });

    assertEquals(delegate.getCallCount('onBookmarkChanged'), 1);
  });

  test('CallsOnBookmarkCreated', async () => {
    bookmarksApi.callbackRouter.onCreated.callListeners('999', {
      id: '999',
      title: 'New bookmark',
      index: 0,
      parentId: folders[0]!.id,
      url: 'http://new/bookmark',
    });

    assertEquals(delegate.getCallCount('onBookmarkCreated'), 1);
  });

  test('CallsOnBookmarkMoved', () => {
    const movedBookmark = folders[0]!.children![2]!.children![0]!;
    bookmarksApi.callbackRouter.onMoved.callListeners(movedBookmark.id, {
      index: 0,
      parentId: folders[0]!.id,                   // Moving to other bookmarks.
      oldParentId: folders[0]!.children![2]!.id,  // Moving from child folder.
      oldIndex: 0,
    });

    assertEquals(delegate.getCallCount('onBookmarkMoved'), 1);
  });

  test('CallsOnBookmarkRemoved', () => {
    bookmarksApi.callbackRouter.onRemoved.callListeners('4');

    assertEquals(delegate.getCallCount('onBookmarkRemoved'), 1);
  });

  test('FindsBookmarkWithId', () => {
    const bookmarkWithValidId = service.findBookmarkWithId('6');
    assertEquals(bookmarkWithValidId!.id, '6');

    const bookmarkWithInvalidId = service.findBookmarkWithId('100');
    assertEquals(bookmarkWithInvalidId, undefined);
  });

  test('CanAddUrl', () => {
    const folder = service.findBookmarkWithId('2');
    assertEquals(service.canAddUrl('http://new/url/', folder), true);
    assertEquals(service.canAddUrl('http://child/bookmark/1/', folder), false);
    assertEquals(service.canAddUrl('http://nested/bookmark/', folder), true);
  });
});
