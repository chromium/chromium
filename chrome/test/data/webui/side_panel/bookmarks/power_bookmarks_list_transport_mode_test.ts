// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import {ActionSource, SortOrder, ViewType} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import type {PowerBookmarksListElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';
import {PageCallbackRouter} from 'chrome://resources/cr_components/commerce/price_tracking.mojom-webui.js';
import {PriceTrackingBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createTestBookmarks, getBookmarkWithId, initializeUi} from './power_bookmarks_list_test_util.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('TransportMode', () => {
  let powerBookmarksList: PowerBookmarksListElement;
  let bookmarksApi: TestBookmarksApiProxy;
  const priceTrackingProxy = TestMock.fromClass(PriceTrackingBrowserProxyImpl);
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>&
      PageImageServiceHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setAllBookmarks(structuredClone(createTestBookmarks()));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    priceTrackingProxy.reset();
    const callbackRouter = new PageCallbackRouter();
    priceTrackingProxy.setResultFor('getCallbackRouter', callbackRouter);
    priceTrackingProxy.setResultFor(
        'getAllPriceTrackedBookmarkProductInfo',
        Promise.resolve({productInfos: []}));
    priceTrackingProxy.setResultFor(
        'getAllShoppingBookmarkProductInfo',
        Promise.resolve({productInfos: []}));
    priceTrackingProxy.setResultFor(
        'getShoppingCollectionBookmarkFolderId',
        Promise.resolve({collectionId: BigInt(-1)}));
    PriceTrackingBrowserProxyImpl.setInstance(priceTrackingProxy);

    imageServiceHandler = TestMock.fromClass(PageImageServiceHandlerRemote);
    PageImageServiceBrowserProxy.setInstance(
        new PageImageServiceBrowserProxy(imageServiceHandler));
    imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
      result: {imageUrl: {url: 'https://example.com/image.png'}},
    }));

    loadTimeData.overrideValues({
      sortOrder: SortOrder.kNewest,
      viewType: ViewType.kCompact,
      emptyTitle: 'empty title base',
      emptyTitleSearch: 'empty title search',
      emptyTitleFolder: 'folder is empty',
      emptyBodyFolder: 'folder body',
      emptyTitleGuest: 'guest title',
      emptyBodyGuest: 'guest body',
      bookmarksTreeViewEnabled: false,
      isBookmarksInTransportModeEnabled: true,
      splitViewEnabled: false,
    });

    powerBookmarksList = await initializeUi(bookmarksApi);
  });

  test('EditBookmarkWithBookmarksInTransportModeEnabled', async () => {
    const bookmarkId = '3';
    const contextMenu = powerBookmarksList.$.contextMenu;
    const editClicked = eventToPromise('edit-clicked', contextMenu);

    // Open the context menu.
    contextMenu.showAtPosition(
        new MouseEvent('click'),
        [getBookmarkWithId(powerBookmarksList, bookmarkId)!], false, false,
        false);
    await waitAfterNextRender(contextMenu);

    // Get the edit option in the menu.
    const menuItems =
        contextMenu.shadowRoot!.querySelectorAll('.dropdown-item');
    assertEquals(
        menuItems[3]!.textContent.includes(loadTimeData.getString('menuEdit')),
        true);
    const editItem = contextMenu.shadowRoot!.querySelectorAll<HTMLElement>(
        '.dropdown-item')[3]!;

    // Click on edit and wait for the call to propagate.
    editItem.click();
    await editClicked;
    await flushTasks();

    // The native edit dialog is opened.
    assertEquals(1, bookmarksApi.getCallCount('contextMenuEdit'));
    assertEquals(bookmarkId, bookmarksApi.getArgs('contextMenuEdit')[0][0][0]);
    assertEquals(
        ActionSource.kBookmark, bookmarksApi.getArgs('contextMenuEdit')[0][1]);
  });


  test('MoveBookmarksWithBookmarksInTransportModeEnabled', async () => {
    const bookmarkId = '3';
    const bookmarks = [
      getBookmarkWithId(powerBookmarksList, bookmarkId)!,
      getBookmarkWithId(powerBookmarksList, '5')!,
    ];
    const contextMenu = powerBookmarksList.$.contextMenu;
    const editClicked = eventToPromise('edit-clicked', contextMenu);

    // Open the context menu.
    contextMenu.showAtPosition(
        new MouseEvent('click'), bookmarks, false, false, false);
    await waitAfterNextRender(contextMenu);

    // Get the move option in the menu.
    const menuItems =
        contextMenu.shadowRoot!.querySelectorAll('.dropdown-item');
    assertEquals(
        menuItems[4]!.textContent.includes(
            loadTimeData.getString('tooltipMove')),
        true);
    const moveItem = contextMenu.shadowRoot!.querySelectorAll<HTMLElement>(
        '.dropdown-item')[4]!;

    // Click on move and wait for the call to propagate.
    moveItem.click();
    await editClicked;
    await flushTasks();

    // The native move dialog is opened.
    assertEquals(1, bookmarksApi.getCallCount('contextMenuMove'));
    assertEquals(bookmarkId, bookmarksApi.getArgs('contextMenuMove')[0][0][0]);
    assertEquals(
        ActionSource.kBookmark, bookmarksApi.getArgs('contextMenuMove')[0][1]);
  });
});
