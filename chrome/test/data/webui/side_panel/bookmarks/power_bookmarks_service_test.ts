// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shared.mojom-webui.js';
import type {BookmarksTreeNode} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {PowerBookmarksService} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_service.js';
import {ShoppingServiceBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';

import {TestBrowserProxy as TestShoppingServiceApiProxy} from './commerce/test_shopping_service_api_proxy.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {TestPowerBookmarksDelegate} from './test_power_bookmarks_delegate.js';

class ServiceTestPowerBookmarksDelegate extends TestPowerBookmarksDelegate {
  override getTrackedProductInfos() {
    const productInfo = {
      title: 'Sample Product',
      clusterTitle: 'Sample Cluster',
      domain: 'sampledomain.com',
      imageUrl: {url: 'http://example.com/sample.jpg'},
      productUrl: {url: 'http://example.com/sample-product'},
      currentPrice: '29.99',
      previousPrice: '39.99',
      clusterId: BigInt(1),
      categoryLabels: ['electronics', 'gadgets'],
      price: '29.99',
      rating: '4.5',
      description: 'This is a sample product description.',
      priceSummary: '',
    };

    const bookmarkProductInfo: BookmarkProductInfo = {
      bookmarkId: BigInt(3),
      info: productInfo,
    };
    this.methodCalled('getTrackedProductInfos');

    const trackedProductInfos = {'3': bookmarkProductInfo};
    return trackedProductInfos;
  }
}

suite('SidePanelPowerBookmarksServiceTest', () => {
  let delegate: ServiceTestPowerBookmarksDelegate;
  let service: PowerBookmarksService;
  let bookmarksApi: TestBookmarksApiProxy;
  let shoppingServiceApi: TestShoppingServiceApiProxy;
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>&
      PageImageServiceHandlerRemote;

  const folders: BookmarksTreeNode[] = [
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
          dateLastUsed: 4,
          children: null,
          unmodifiable: false,
        },
        {
          id: '4',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          index: 1,
          title: 'Second child bookmark',
          url: 'http://child/bookmark/2/',
          dateAdded: 3,
          dateLastUsed: 3,
          children: null,
          unmodifiable: false,
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
              children: null,
              unmodifiable: false,
            },
          ],
        },
      ],
    },
  ];

  // More complex folder structure, including a more extensive hierarchy and
  // inverted dateAdded between parents/children. Not used by default in tests.
  const complexAllBookmarks: BookmarksTreeNode[] = [
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
          title: 'Child folder',
          url: null,
          dateAdded: 4,
          dateLastUsed: null,
          unmodifiable: false,
          children: [
            {
              id: '7',
              parentId: '3',
              index: 0,
              title: 'Nested bookmark',
              url: 'http://nested/bookmark/',
              dateAdded: 1,
              dateLastUsed: 10,
              unmodifiable: false,
              children: null,
            },
          ],
        },
        {
          id: '4',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          index: 1,
          title: 'Child folder',
          url: null,
          dateAdded: 2,
          dateLastUsed: null,
          unmodifiable: false,
          children: [
            {
              id: '8',
              parentId: '4',
              index: 0,
              title: 'Nested bookmark',
              url: 'http://nested/bookmark/',
              dateAdded: 5,
              dateLastUsed: 6,
              unmodifiable: false,
              children: null,
            },
          ],
        },
        {
          id: '5',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          index: 2,
          title: 'Child folder',
          url: null,
          dateAdded: 6,
          dateLastUsed: null,
          unmodifiable: false,
          children: [
            {
              id: '10',
              parentId: '5',
              index: 0,
              title: 'Child folder',
              url: null,
              dateAdded: 8,
              dateLastUsed: null,
              unmodifiable: false,
              children: [
                {
                  id: '13',
                  parentId: '10',
                  index: 0,
                  title: 'Nested folder',
                  url: null,
                  dateAdded: 0,
                  dateLastUsed: null,
                  unmodifiable: false,
                  children: [],
                },
              ],
            },
          ],
        },
        {
          id: '6',
          parentId: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
          index: 3,
          title: 'Child folder',
          url: null,
          dateAdded: 3,
          dateLastUsed: null,
          unmodifiable: false,
          children: [
            {
              id: '12',
              parentId: '6',
              index: 0,
              title: 'Child folder',
              url: null,
              dateAdded: 3,
              dateLastUsed: null,
              unmodifiable: false,
              children: [
                {
                  id: '14',
                  parentId: '12',
                  index: 0,
                  title: 'Nested bookmark',
                  url: 'http://nested/bookmark/',
                  dateAdded: 9,
                  dateLastUsed: 1,
                  unmodifiable: false,
                  children: null,
                },
              ],
            },
          ],
        },
      ],
    },
  ];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setAllBookmarks(structuredClone(folders));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    shoppingServiceApi = new TestShoppingServiceApiProxy();
    ShoppingServiceBrowserProxyImpl.setInstance(shoppingServiceApi);

    const pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);

    imageServiceHandler = TestMock.fromClass(PageImageServiceHandlerRemote);
    PageImageServiceBrowserProxy.setInstance(
        new PageImageServiceBrowserProxy(imageServiceHandler));
    imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
      result: {imageUrl: {url: 'https://example.com/image.png'}},
    }));

    delegate = new ServiceTestPowerBookmarksDelegate();
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

  test('FiltersByFolderAndSearchQuery', () => {
    const folder = service.findBookmarkWithId('5');
    const primaryList = service.filterBookmarks(folder, 0, 'http', []);
    const secondaryList =
        service.filterBookmarks(undefined, 0, 'http', [], folder);
    assertEquals(primaryList.length, 1);
    assertEquals(secondaryList.length, 2);
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

  test('SortsByNewestWithComplexDescendants', async () => {
    bookmarksApi.setAllBookmarks(complexAllBookmarks);
    service.startListening();

    await delegate.whenCalled('onBookmarksLoaded');

    const sortedBookmarks =
        service.filterBookmarks(undefined, 0, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '6');
    assertEquals(sortedBookmarks[1]!.id, '5');
    assertEquals(sortedBookmarks[2]!.id, '4');
    assertEquals(sortedBookmarks[3]!.id, '3');
  });

  test('SortsByOldest', () => {
    const sortedBookmarks =
        service.filterBookmarks(undefined, 1, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '5');
    assertEquals(sortedBookmarks[1]!.id, '3');
    assertEquals(sortedBookmarks[2]!.id, '4');
  });

  test('SortsByOldestWithComplexDescendants', async () => {
    bookmarksApi.setAllBookmarks(complexAllBookmarks);
    service.startListening();

    await delegate.whenCalled('onBookmarksLoaded');

    const sortedBookmarks =
        service.filterBookmarks(undefined, 1, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '5');
    assertEquals(sortedBookmarks[1]!.id, '3');
    assertEquals(sortedBookmarks[2]!.id, '4');
    assertEquals(sortedBookmarks[3]!.id, '6');
  });

  test('SortsByLastOpened', () => {
    const sortedBookmarks =
        service.filterBookmarks(undefined, 2, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '5');
    assertEquals(sortedBookmarks[1]!.id, '3');
    assertEquals(sortedBookmarks[2]!.id, '4');
  });

  test('SortsByLastOpenedWithComplexDescendants', async () => {
    bookmarksApi.setAllBookmarks(complexAllBookmarks);
    service.startListening();

    await delegate.whenCalled('onBookmarksLoaded');

    const sortedBookmarks =
        service.filterBookmarks(undefined, 2, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '3');
    assertEquals(sortedBookmarks[1]!.id, '5');
    assertEquals(sortedBookmarks[2]!.id, '4');
    assertEquals(sortedBookmarks[3]!.id, '6');
  });

  test('SortsByAToZ', () => {
    const sortedBookmarks =
        service.filterBookmarks(undefined, 3, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '5');
    assertEquals(sortedBookmarks[1]!.id, '3');
    assertEquals(sortedBookmarks[2]!.id, '4');
  });

  test('SortsByZToA', () => {
    const sortedBookmarks =
        service.filterBookmarks(undefined, 4, undefined, []);
    assertEquals(sortedBookmarks[0]!.id, '5');
    assertEquals(sortedBookmarks[1]!.id, '4');
    assertEquals(sortedBookmarks[2]!.id, '3');
  });

  test('CallsOnBookmarkChanged', async () => {
    const changedBookmark = folders[0]!.children![0]!;
    bookmarksApi.callbackRouterRemote.onBookmarkNodeChanged(
        changedBookmark.id, 'New title', 'http://new/url');
    await flushTasks();

    assertEquals(delegate.getCallCount('onBookmarkChanged'), 1);
  });

  test('CallsOnBookmarkAdded', async () => {
    bookmarksApi.callbackRouterRemote.onBookmarkNodeAdded({
      id: '999',
      title: 'New bookmark',
      index: 0,
      parentId: folders[0]!.id,
      url: 'http://new/bookmark',
      children: null,
      dateAdded: null,
      dateLastUsed: null,
      unmodifiable: false,
    });
    await flushTasks();

    assertEquals(delegate.getCallCount('onBookmarkAdded'), 1);
  });

  test('CallsOnBookmarkMoved', async () => {
    const movedBookmark = folders[0]!.children![2]!.children![0]!;
    assertTrue(!!movedBookmark);
    bookmarksApi.callbackRouterRemote.onBookmarkNodeMoved(
        /*oldParentId=*/ folders[0]!.children![2]!
            .id,  // Moving from child folder.
        /*oldIndex=*/ 0,
        /*parentId=*/ folders[0]!.id,  // Moving to other bookmarks.
        /*index=*/ 0,
    );
    await flushTasks();

    assertEquals(delegate.getCallCount('onBookmarkMoved'), 1);
  });

  test('CallsOnBookmarkNodesRemoved', async () => {
    bookmarksApi.callbackRouterRemote.onBookmarkNodesRemoved(['3', '4']);
    await flushTasks();

    assertEquals(delegate.getCallCount('onBookmarkRemoved'), 2);
  });

  test('FindsBookmarkWithId', () => {
    const bookmarkWithValidId = service.findBookmarkWithId('6');
    assertEquals(bookmarkWithValidId!.id, '6');

    const bookmarkWithInvalidId = service.findBookmarkWithId('100');
    assertEquals(bookmarkWithInvalidId, undefined);
  });

  test('CanAddUrl', () => {
    const folder = service.findBookmarkWithId('SIDE_PANEL_OTHER_BOOKMARKS_ID');
    assertEquals(service.canAddUrl('http://new/url/', folder), true);
    assertEquals(service.canAddUrl('http://child/bookmark/1/', folder), false);
    assertEquals(service.canAddUrl('http://nested/bookmark/', folder), true);
  });

  test('RequestsImages', async () => {
    assertEquals(imageServiceHandler.getCallCount('getPageImageUrl'), 0);

    service.setMaxImageServiceRequestsForTesting(2);
    service.refreshDataForBookmarks([
      service.findBookmarkWithId('3')!,
      service.findBookmarkWithId('4')!,
      service.findBookmarkWithId('6')!,
    ]);

    assertEquals(imageServiceHandler.getCallCount('getPageImageUrl'), 2);
    await flushTasks();
    assertEquals(imageServiceHandler.getCallCount('getPageImageUrl'), 3);
  });

  test('OnBookmarkParentFolderChildrenReordered', async () => {
    const folder = service.findBookmarkWithId('SIDE_PANEL_OTHER_BOOKMARKS_ID')!;
    const b3 = service.findBookmarkWithId('3')!;
    const b4 = service.findBookmarkWithId('4')!;
    const b5 = service.findBookmarkWithId('5')!;

    assertDeepEquals(folder.children!, [b3, b4, b5]);

    bookmarksApi.callbackRouterRemote.onBookmarkParentFolderChildrenReordered(
        'SIDE_PANEL_OTHER_BOOKMARKS_ID', ['4', '5', '3']);
    await flushTasks();

    assertDeepEquals(folder.children!, [b4, b5, b3]);
  });
});
