// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import {SortOrder, ViewType} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import type {PowerBookmarkRowElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row.js';
import {NESTED_BOOKMARKS_BASE_MARGIN, NESTED_BOOKMARKS_MARGIN_PER_DEPTH} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row.js';
import type {PowerBookmarksListElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';
import {PageCallbackRouter} from 'chrome://resources/cr_components/commerce/price_tracking.mojom-webui.js';
import {PriceTrackingBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {createTestBookmarks, getBookmarks, getPowerBookmarksRowElement, initializeUi} from './power_bookmarks_list_test_util.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('TreeView', () => {
  const FOLDERS = createTestBookmarks();
  let powerBookmarksList: PowerBookmarksListElement;
  let bookmarksApi: TestBookmarksApiProxy;
  const priceTrackingProxy = TestMock.fromClass(PriceTrackingBrowserProxyImpl);
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>&
      PageImageServiceHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setAllBookmarks(structuredClone(FOLDERS));
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
      bookmarksTreeViewEnabled: true,
    });

    powerBookmarksList = await initializeUi(bookmarksApi);
  });

  test('ShowsExpandButtonForFolders', () => {
    const folderElement = getPowerBookmarksRowElement(powerBookmarksList, '5');
    assertTrue(!!folderElement);

    let expandButton =
        folderElement.shadowRoot.querySelector<PowerBookmarkRowElement>(
            '#expandButton');
    // Assert that the expand button is present for folders
    assertTrue(!!expandButton);

    const singleBookmarkElement =
        getPowerBookmarksRowElement(powerBookmarksList, '3');
    assertTrue(!!singleBookmarkElement);

    expandButton =
        singleBookmarkElement.shadowRoot.querySelector<PowerBookmarkRowElement>(
            '#expandButton');
    // Assert that the expand button is not present for single bookmarks
    assertFalse(!!expandButton);
  });

  test('ShowsCorrectFoldersOnTreeView', () => {
    assertEquals(
        FOLDERS[1]!.children!.length + 1,
        getBookmarks(powerBookmarksList).length);
  });

  test('ExpandAndCollapseNestedBookmarks', async () => {
    const folderElement = getPowerBookmarksRowElement(powerBookmarksList, '5');
    assertTrue(!!folderElement);

    const expandButton =
        folderElement.shadowRoot.querySelector<PowerBookmarkRowElement>(
            '#expandButton');
    assertTrue(!!expandButton);

    expandButton.click();
    await expandButton.updateComplete;
    await folderElement.updateComplete;

    // Verify nested bookmarks are now visible
    const nestedBookmarkElement =
        folderElement.shadowRoot.querySelector<PowerBookmarkRowElement>(
            '#bookmark-6');
    assertTrue(!!nestedBookmarkElement);
    // Verify that the nested bookmark has the correct depth
    assertEquals(1, nestedBookmarkElement.depth);

    // Verify that the "more" button has a tooltip.
    const dotsIcon =
        nestedBookmarkElement.shadowRoot.querySelector<HTMLElement>(
            'cr-icon-button[iron-icon=\'cr:more-vert\']');
    assertTrue(!!dotsIcon);
    assertEquals(loadTimeData.getString('tooltipMore'), dotsIcon.title);

    const bookmarkDiv =
        nestedBookmarkElement.shadowRoot.querySelector<HTMLElement>(
            '#bookmark');
    assertTrue(!!bookmarkDiv);

    // Check if the depth is correctly applied to the style
    const computedStyle = getComputedStyle(bookmarkDiv);
    const expectedMargin =
        nestedBookmarkElement.depth * NESTED_BOOKMARKS_MARGIN_PER_DEPTH +
        NESTED_BOOKMARKS_BASE_MARGIN;
    assertEquals(`${expectedMargin}px`, computedStyle.marginLeft);

    expandButton.click();
    await expandButton.updateComplete;
    await folderElement.updateComplete;

    // Verify nested bookmarks are no longer visible
    const collapsedNestedBookmarkElement =
        folderElement.shadowRoot.querySelector<PowerBookmarkRowElement>(
            '#bookmark-6');
    assertFalse(!!collapsedNestedBookmarkElement);
  });
});
