// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import {SortOrder, ViewType} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import type {BookmarksTreeNode} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {NESTED_BOOKMARKS_BASE_MARGIN, NESTED_BOOKMARKS_MARGIN_PER_DEPTH} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row.js';
import {PageCallbackRouter} from 'chrome://resources/cr_components/commerce/price_tracking.mojom-webui.js';
import {PriceTrackingBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';

import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createTestBookmarks, getBookmarks, getPowerBookmarksRowElement, getPowerBookmarksRowItemElement, initializeAppUi} from './power_bookmarks_app_test_util.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

export interface TestPowerBookmarksListElement {
  activeSortIndex: number;
  sortOrder: SortOrder;
}

const nestedBookmarks: BookmarksTreeNode[] = [
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
            title: 'Child folder Q',
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
          {
            id: '21',
            parentId: '5',
            index: 0,
            title: 'Child folder P',
            url: null,
            dateAdded: 8,
            dateLastUsed: null,
            unmodifiable: false,
            children: [
              {
                id: '25',
                parentId: '21',
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
          {
            id: '22',
            parentId: '5',
            index: 0,
            title: 'Nested bookmark X',
            url: 'http://nested/bookmark/',
            dateAdded: 9,
            dateLastUsed: 1,
            unmodifiable: false,
            children: null,
          },
          {
            id: '23',
            parentId: '5',
            index: 0,
            title: 'Nested bookmark Z',
            url: 'http://nested/bookmark/',
            dateAdded: 9,
            dateLastUsed: 1,
            unmodifiable: false,
            children: null,
          },
          {
            id: '24',
            parentId: '5',
            index: 0,
            title: 'Nested bookmark A',
            url: 'http://nested/bookmark/',
            dateAdded: 9,
            dateLastUsed: 1,
            unmodifiable: false,
            children: null,
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

const ARROW_RIGHT_EVENT = new KeyboardEvent(
    'keydown', {key: 'ArrowRight', bubbles: true, composed: true});
const ARROW_LEFT_EVENT = new KeyboardEvent(
    'keydown', {key: 'ArrowLeft', bubbles: true, composed: true});

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_app.js';
import type {PowerBookmarksAppElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_app.js';

suite('TreeView', () => {
  const FOLDERS = createTestBookmarks();
  let powerBookmarksApp: PowerBookmarksAppElement;
  let bookmarksApi: TestBookmarksApiProxy;
  const priceTrackingProxy = TestMock.fromClass(PriceTrackingBrowserProxyImpl);
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>&
      PageImageServiceHandlerRemote;
  let metrics: MetricsTracker;

  async function waitRebuildNavigationElements() {
    powerBookmarksApp.$.bookmarksList
        .flushNavigationElementsDebouncerForTesting();
    await microtasksFinished();
    await eventToPromise(
        'rebuild-navigation-elements', powerBookmarksApp.$.bookmarksList);
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    metrics = fakeMetricsPrivate();

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
      result: {imageUrl: 'https://example.com/image.png'},
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
      tooltipBack: 'Back',
    });

    powerBookmarksApp = await initializeAppUi(bookmarksApi);
  });

  test('ShowsExpandButtonForFolders', () => {
    const folderElement = getPowerBookmarksRowElement(powerBookmarksApp, '5');
    assertTrue(!!folderElement);

    const folderItem = getPowerBookmarksRowItemElement(powerBookmarksApp, '5');
    assertTrue(!!folderItem);

    let expandButton =
        folderItem.shadowRoot.querySelector<HTMLElement>('#expandButton');
    assertTrue(!!expandButton);

    const singleBookmarkItem =
        getPowerBookmarksRowItemElement(powerBookmarksApp, '3');
    assertTrue(!!singleBookmarkItem);

    expandButton = singleBookmarkItem.shadowRoot.querySelector<HTMLElement>(
        '#expandButton');
    assertFalse(!!expandButton);
  });

  test('SortsNestedBookmarksLists', async () => {
    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setAllBookmarks(structuredClone(nestedBookmarks));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);
    powerBookmarksApp = await initializeAppUi(bookmarksApi);

    const folderRow = getPowerBookmarksRowElement(powerBookmarksApp, '5');
    assertTrue(!!folderRow);
    assertFalse(folderRow.toggleExpand, 'Folder should be initially collapsed');

    // Focus the item before sending key presses.
    const rowItem =
        folderRow.shadowRoot.querySelector('power-bookmark-row-item');
    assertTrue(!!rowItem);
    const urlListItem = rowItem.$.crUrlListItem;
    urlListItem.focus();
    powerBookmarksApp.$.bookmarksList.getKeyboardNavigationServiceforTesting()
        .setCurrentFocusIndex(folderRow);

    // Expand with Right Arrow.
    const toggleEvent =
        eventToPromise('power-bookmark-toggle', powerBookmarksApp);
    const keyboardRebuilt = waitRebuildNavigationElements();
    folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await toggleEvent;
    await keyboardRebuilt;
    await microtasksFinished();

    // Default sort is kNewest.
    assertArrayEquals(
        powerBookmarksApp.$.bookmarksList
            .getKeyboardNavigationServiceforTesting()
            .getElementsForTesting()
            .map((el: HTMLElement) => el.id),
        [
          'bookmark-5',
          'bookmark-10',
          'bookmark-21',
          'bookmark-22',
          'bookmark-23',
          'bookmark-24',
          'bookmark-6',
          'bookmark-4',
          'bookmark-3',
        ]);

    const bookmarksList = powerBookmarksApp.$.bookmarksList as unknown as
        TestPowerBookmarksListElement;
    bookmarksList.activeSortIndex = 4;
    bookmarksList.sortOrder = SortOrder.kReverseAlphabetical;

    await microtasksFinished();

    assertArrayEquals(
        powerBookmarksApp.$.bookmarksList
            .getKeyboardNavigationServiceforTesting()
            .getElementsForTesting()
            .map((el: HTMLElement) => el.id),
        [
          'bookmark-3',
          'bookmark-4',
          'bookmark-5',
          'bookmark-10',
          'bookmark-21',
          'bookmark-23',
          'bookmark-22',
          'bookmark-24',
          'bookmark-6',
        ]);
  });

  test('ShowsCorrectFoldersOnTreeView', () => {
    assertEquals(
        FOLDERS[1]!.children!.length + 1,
        getBookmarks(powerBookmarksApp).length);
  });

  test('ExpandAndCollapseNestedBookmarks', async () => {
    const folderElement = getPowerBookmarksRowElement(powerBookmarksApp, '5');
    assertTrue(!!folderElement);

    const bookmarkRowItem =
        getPowerBookmarksRowItemElement(powerBookmarksApp, '5');
    assertTrue(!!bookmarkRowItem);
    const expandButton =
        bookmarkRowItem.shadowRoot.querySelector<HTMLElement>('#expandButton');
    assertTrue(!!expandButton);

    const keyboardRebuilt = waitRebuildNavigationElements();
    expandButton.click();
    await keyboardRebuilt;
    await microtasksFinished();

    // Verify nested bookmarks are now visible
    const nestedBookmarkElement =
        getPowerBookmarksRowElement(powerBookmarksApp, '6');
    assertTrue(isVisible(nestedBookmarkElement));
    // Verify that the nested bookmark has the correct depth
    assertEquals(1, nestedBookmarkElement!.depth);

    // Verify that the "more" button has a tooltip.
    const rowItem = nestedBookmarkElement!.shadowRoot.querySelector(
        'power-bookmark-row-item');
    assertTrue(!!rowItem);
    const dotsIcon = rowItem.shadowRoot.querySelector<HTMLElement>(
        'cr-icon-button[iron-icon=\'cr:more-vert\']');
    assertTrue(!!dotsIcon);
    assertEquals(loadTimeData.getString('tooltipMore'), dotsIcon.title);

    const bookmarkDiv =
        rowItem.shadowRoot.querySelector<HTMLElement>('#crUrlListItem');
    assertTrue(!!bookmarkDiv);

    // Check if the depth is correctly applied to the style
    const computedStyle = getComputedStyle(bookmarkDiv);
    const expectedMargin =
        nestedBookmarkElement!.depth * NESTED_BOOKMARKS_MARGIN_PER_DEPTH +
        NESTED_BOOKMARKS_BASE_MARGIN;
    assertEquals(`${expectedMargin}px`, computedStyle.paddingInlineStart);

    const collapseMetricsLogged = eventToPromise(
        'bookmark-count-recorded', powerBookmarksApp.$.bookmarksList);
    expandButton.click();
    await collapseMetricsLogged;

    await microtasksFinished();

    // Verify nested bookmarks are no longer in display list
    const items = powerBookmarksApp.$.bookmarksList.$.list.items;
    assertFalse(items.some(item => item.bookmark.id === '6'));

    // And verify visual hidden state in DOM
    const nestedBookmarkElementAfter =
        getPowerBookmarksRowElement(powerBookmarksApp, '6');
    assertFalse(isVisible(nestedBookmarkElementAfter));
  });


  test('expands and collapses folders with arrow keys', async () => {
    const folderRow = getPowerBookmarksRowElement(powerBookmarksApp, '5');
    assertTrue(!!folderRow);
    assertFalse(folderRow.toggleExpand, 'Folder should be initially collapsed');

    // Child should not be visible initially.
    assertFalse(
        !!getPowerBookmarksRowElement(powerBookmarksApp, '6'),
        'Child bookmark should not be visible initially');

    // Focus the item before sending key presses.
    const rowItem =
        folderRow.shadowRoot.querySelector('power-bookmark-row-item');
    assertTrue(!!rowItem);
    rowItem.$.crUrlListItem.focus();
    powerBookmarksApp.$.bookmarksList.getKeyboardNavigationServiceforTesting()
        .setCurrentFocusIndex(folderRow);

    // Expand with Right Arrow.
    let keyboardRebuilt = waitRebuildNavigationElements();
    let toggleEvent =
        eventToPromise('power-bookmark-toggle', powerBookmarksApp);
    folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await toggleEvent;
    await keyboardRebuilt;
    await microtasksFinished();
    assertTrue(
        folderRow.toggleExpand, 'Folder should be expanded after ArrowRight');
    const childRow = getPowerBookmarksRowElement(powerBookmarksApp, '6');
    assertTrue(isVisible(childRow), 'Child bookmark should be visible');

    // Collapse with Left Arrow.
    keyboardRebuilt = waitRebuildNavigationElements();
    toggleEvent = eventToPromise('power-bookmark-toggle', powerBookmarksApp);
    folderRow.dispatchEvent(ARROW_LEFT_EVENT);
    await toggleEvent;
    await keyboardRebuilt;
    await microtasksFinished();
    assertFalse(
        folderRow.toggleExpand, 'Folder should be collapsed after ArrowLeft');
    const items = powerBookmarksApp.$.bookmarksList.$.list.items;
    assertFalse(
        items.some(item => item.bookmark.id === '6'),
        'Child bookmark should not be in display list');

    const nestedBookmarkElementAfter =
        getPowerBookmarksRowElement(powerBookmarksApp, '6');
    assertFalse(
        isVisible(nestedBookmarkElementAfter),
        'Child bookmark should not be visible');
  });

  test(
      'moves focus to first child on right arrow if already open', async () => {
        const folderRow = getPowerBookmarksRowElement(powerBookmarksApp, '5');
        assertTrue(!!folderRow);
        const folderItem =
            folderRow.shadowRoot.querySelector('power-bookmark-row-item');
        assertTrue(!!folderItem);
        powerBookmarksApp.$.bookmarksList
            .flushNavigationElementsDebouncerForTesting();

        folderItem.$.crUrlListItem.focus();
        powerBookmarksApp.$.bookmarksList
            .getKeyboardNavigationServiceforTesting()
            .setCurrentFocusIndex(folderRow);

        // Expand the folder first.
        const keyboardRebuilt = waitRebuildNavigationElements();
        const toggleEvent =
            eventToPromise('power-bookmark-toggle', powerBookmarksApp);
        folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
        await toggleEvent;
        await keyboardRebuilt;
        await microtasksFinished();
        assertTrue(folderRow.toggleExpand, 'Folder should be expanded');

        const childRow = getPowerBookmarksRowElement(powerBookmarksApp, '6')!;

        // Right arrow on expanded folder should move focus.
        folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
        await microtasksFinished();

        assertEquals(
            childRow,
            powerBookmarksApp.$.bookmarksList.shadowRoot.activeElement,
            'Focus should move to the first child');
      });

  test('right arrow does nothing on non-folder', async () => {
    const bookmarkRow = getPowerBookmarksRowItemElement(powerBookmarksApp, '3');
    assertTrue(!!bookmarkRow);

    const urlListItem = bookmarkRow.$.crUrlListItem;
    urlListItem.focus();

    // This should not throw errors or change state.
    bookmarkRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await microtasksFinished();

    // No toggleExpand property to check, just make sure nothing broke.
    assertTrue(!!getPowerBookmarksRowItemElement(powerBookmarksApp, '3'));
  });

  test('moves focus to parent on left arrow from child', async () => {
    const folderRow = getPowerBookmarksRowElement(powerBookmarksApp, '5');
    assertTrue(!!folderRow);
    const folderItem =
        folderRow.shadowRoot.querySelector('power-bookmark-row-item');
    assertTrue(!!folderItem);
    powerBookmarksApp.$.bookmarksList
        .flushNavigationElementsDebouncerForTesting();

    folderItem.$.crUrlListItem.focus();
    powerBookmarksApp.$.bookmarksList.getKeyboardNavigationServiceforTesting()
        .setCurrentFocusIndex(folderRow);

    // Expand the folder first.
    const keyboardRebuilt = waitRebuildNavigationElements();
    const toggleEvent =
        eventToPromise('power-bookmark-toggle', powerBookmarksApp);
    folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await toggleEvent;
    await keyboardRebuilt;
    await microtasksFinished();
    assertTrue(folderRow.toggleExpand, 'Folder should be expanded');

    const childRow = getPowerBookmarksRowElement(powerBookmarksApp, '6')!;

    // Right arrow on expanded folder should move focus.
    folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await microtasksFinished();

    assertEquals(
        childRow, powerBookmarksApp.$.bookmarksList.shadowRoot.activeElement,
        'Focus should move to the first child');

    childRow.dispatchEvent(ARROW_LEFT_EVENT);
    await microtasksFinished();

    assertEquals(
        folderRow.id,
        powerBookmarksApp.$.bookmarksList.shadowRoot.activeElement!.id,
        'Focus should move to the parent row');
  });

  test('LogsMetricsCountExpanded', async () => {
    powerBookmarksApp = await initializeAppUi(bookmarksApi);

    const folderRow = getPowerBookmarksRowItemElement(powerBookmarksApp, '5');
    assertTrue(!!folderRow);

    const urlListItem = folderRow.$.crUrlListItem;
    urlListItem.focus();

    const toggleEvent =
        eventToPromise('power-bookmark-toggle', powerBookmarksApp);
    const metricsLogged = eventToPromise(
        'bookmark-count-recorded', powerBookmarksApp.$.bookmarksList);
    folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await toggleEvent;
    await metricsLogged;

    assertEquals(
        1, metrics.count('PowerBookmarks.SidePanel.BookmarksShown', 5));
  });


  test('UpdatesChildrenAfterMove', async () => {
    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setAllBookmarks(structuredClone(nestedBookmarks));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);
    powerBookmarksApp = await initializeAppUi(bookmarksApi);

    const folder5 = getPowerBookmarksRowElement(powerBookmarksApp, '5')!;
    const folder6 = getPowerBookmarksRowElement(powerBookmarksApp, '6')!;
    assertTrue(!!folder5, 'Folder 5 should exist');
    assertTrue(!!folder6, 'Folder 6 should exist');

    // Expand Folder 5 and Folder 6 to reveal their children.
    const folderItem5 =
        getPowerBookmarksRowItemElement(powerBookmarksApp, '5')!;
    let metricsLogged = eventToPromise(
        'bookmark-count-recorded', powerBookmarksApp.$.bookmarksList);
    folderItem5.shadowRoot.querySelector<HTMLElement>('#expandButton')!.click();
    await metricsLogged;
    await microtasksFinished();

    const folderItem6 =
        getPowerBookmarksRowItemElement(powerBookmarksApp, '6')!;
    metricsLogged = eventToPromise(
        'bookmark-count-recorded', powerBookmarksApp.$.bookmarksList);
    folderItem6.shadowRoot.querySelector<HTMLElement>('#expandButton')!.click();
    await metricsLogged;
    await microtasksFinished();

    const row22 = getPowerBookmarksRowElement(powerBookmarksApp, '22')!;
    assertTrue(isVisible(row22), 'Bookmark 22 should exist');
    assertEquals(
        '5', row22.bookmark.parentId,
        'Bookmark 22 should initially be in folder 5');

    // Validate index order before move in flat display list
    const list = powerBookmarksApp.$.bookmarksList.$.list;
    let items = list.items;
    const indexOf5 = items.findIndex(item => item.bookmark.id === '5');
    let indexOf6 = items.findIndex(item => item.bookmark.id === '6');
    let indexOf22 = items.findIndex(item => item.bookmark.id === '22');
    assertTrue(indexOf22 > indexOf5, 'Bookmark 22 should be below folder 5');
    assertTrue(indexOf22 < indexOf6, 'Bookmark 22 should be above folder 6');

    bookmarksApi.callbackRouterRemote.onBookmarkNodeMoved(
        /*oldParentId=*/ '5', /*oldIndex=*/ 2, /*newParentId=*/ '6',
        /*newIndex=*/ 0);
    await microtasksFinished();

    const movedRow22 = getPowerBookmarksRowElement(powerBookmarksApp, '22')!;
    assertTrue(isVisible(movedRow22), 'Bookmark 22 should exist after move');
    assertEquals(
        '6', movedRow22.bookmark.parentId,
        'Bookmark 22 should be in folder 6 after move');

    // Validate index order after move in flat display list
    items = list.items;
    indexOf6 = items.findIndex(item => item.bookmark.id === '6');
    indexOf22 = items.findIndex(item => item.bookmark.id === '22');
    assertTrue(
        indexOf22 > indexOf6,
        'Bookmark 22 should be below folder 6 after move');
  });
});
