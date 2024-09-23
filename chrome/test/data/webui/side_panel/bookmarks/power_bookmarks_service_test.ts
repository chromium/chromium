// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {PowerBookmarksService} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_service.js';
import {BrowserProxyImpl as ShoppingServiceApiProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
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
          dateLastUsed: 4,
        },
        {
          id: '4',
          parentId: '2',
          title: 'Second child bookmark',
          url: 'http://child/bookmark/2/',
          dateAdded: 3,
          dateLastUsed: 3,
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

  // More complex folder structure, including a more extensive hierarchy and
  // inverted dateAdded between parents/children. Not used by default in tests.
  const complexFolders: chrome.bookmarks.BookmarkTreeNode[] = [
    {
      id: '2',
      parentId: '0',
      title: 'Other Bookmarks',
      children: [
        {
          id: '3',
          parentId: '2',
          title: 'Child folder',
          dateAdded: 4,
          children: [
            {
              id: '7',
              parentId: '3',
              title: 'Nested bookmark',
              url: 'http://nested/bookmark/',
              dateAdded: 1,
              dateLastUsed: 10,
            },
          ],
        },
        {
          id: '4',
          parentId: '2',
          title: 'Child folder',
          dateAdded: 2,
          children: [
            {
              id: '8',
              parentId: '4',
              title: 'Nested bookmark',
              url: 'http://nested/bookmark/',
              dateAdded: 5,
              dateLastUsed: 6,
            },
          ],
        },
        {
          id: '5',
          parentId: '2',
          title: 'Child folder',
          dateAdded: 6,
          children: [
            {
              id: '10',
              parentId: '5',
              title: 'Child folder',
              dateAdded: 8,
              children: [
                {
                  id: '13',
                  parentId: '10',
                  title: 'Nested folder',
                  dateAdded: 0,
                  children: [],
                },
              ],
            },
          ],
        },
        {
          id: '6',
          parentId: '2',
          title: 'Child folder',
          dateAdded: 3,
          children: [
            {
              id: '12',
              parentId: '6',
              title: 'Child folder',
              dateAdded: 3,
              children: [
                {
                  id: '14',
                  parentId: '12',
                  title: 'Nested bookmark',
                  url: 'http://nested/bookmark/',
                  dateAdded: 9,
                  dateLastUsed: 1,
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
    bookmarksApi.setFolders(structuredClone(folders));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    shoppingServiceApi = new TestShoppingServiceApiProxy();
    ShoppingServiceApiProxyImpl.setInstance(shoppingServiceApi);

    const pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);

    imageServiceHandler = TestMock.fromClass(PageImageServiceHandlerRemote);
    PageImageServiceBrowserProxy.setInstance(
        new PageImageServiceBrowserProxy(imageServiceHandler));
    imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
      result: {imageUrl: {url: 'https://example.com/image.png'}},
    }));

    loadTimeData.overrideValues({
      urlImagesEnabled: true,
    });

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
    bookmarksApi.setFolders(structuredClone(complexFolders));
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
    bookmarksApi.setFolders(structuredClone(complexFolders));
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
    bookmarksApi.setFolders(structuredClone(complexFolders));
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
});
