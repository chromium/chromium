// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import {SortOrder, ViewType} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import type {BookmarksTreeNode} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
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
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createTestBookmarks, getBookmarks, getPowerBookmarksRowElement, initializeUi} from './power_bookmarks_list_test_util.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

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

suite('TreeView', () => {
  const FOLDERS = createTestBookmarks();
  let powerBookmarksList: PowerBookmarksListElement;
  let bookmarksApi: TestBookmarksApiProxy;
  const priceTrackingProxy = TestMock.fromClass(PriceTrackingBrowserProxyImpl);
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>&
      PageImageServiceHandlerRemote;
  let metrics: MetricsTracker;

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

  test('SortsNestedBookmarksLists', async () => {
    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setAllBookmarks(nestedBookmarks);
    BookmarksApiProxyImpl.setInstance(bookmarksApi);
    powerBookmarksList = await initializeUi(bookmarksApi);

    const folderRow = getPowerBookmarksRowElement(powerBookmarksList, '5');
    assertTrue(!!folderRow);
    assertFalse(folderRow.toggleExpand, 'Folder should be initially collapsed');

    // Focus the item before sending key presses.
    const urlListItem = folderRow.shadowRoot.querySelector('cr-url-list-item')!;
    urlListItem.focus();
    powerBookmarksList.getKeyboardNavigationServiceforTesting()
        .setCurrentFocusIndex(folderRow);

    // Expand with Right Arrow.
    const toggleEvent =
        eventToPromise('power-bookmark-toggle', powerBookmarksList);
    folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await toggleEvent;
    await flushTasks();
    await waitAfterNextRender(powerBookmarksList);

    // Default sort is kNewest.
    assertEquals(
        JSON.stringify(
            powerBookmarksList.getKeyboardNavigationServiceforTesting()
                .getElementsForTesting()
                .map((el: HTMLElement) => el.id)),
        JSON.stringify([
          'bookmark-5',
          'bookmark-10',
          'bookmark-21',
          'bookmark-22',
          'bookmark-23',
          'bookmark-24',
          'bookmark-6',
          'bookmark-4',
          'bookmark-3',
        ]));

    folderRow.activeSortIndex = 4;
    await microtasksFinished();
    await flushTasks();

    assertEquals(
        JSON.stringify(
            powerBookmarksList.getKeyboardNavigationServiceforTesting()
                .getElementsForTesting()
                .map((el: HTMLElement) => el.id)),
        JSON.stringify([
          'bookmark-5',
          'bookmark-10',
          'bookmark-21',
          'bookmark-23',
          'bookmark-22',
          'bookmark-24',
          'bookmark-6',
          'bookmark-4',
          'bookmark-3',
        ]));
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
        getPowerBookmarksRowElement(folderElement, '6');
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
    assertFalse(!!getPowerBookmarksRowElement(folderElement, '6'));
  });


  test('expands and collapses folders with arrow keys', async () => {
    const folderRow = getPowerBookmarksRowElement(powerBookmarksList, '5');
    assertTrue(!!folderRow);
    assertFalse(folderRow.toggleExpand, 'Folder should be initially collapsed');

    // Child should not be visible initially.
    assertFalse(
        !!getPowerBookmarksRowElement(powerBookmarksList, '6'),
        'Child bookmark should not be visible initially');

    // Focus the item before sending key presses.
    const urlListItem = folderRow.shadowRoot.querySelector('cr-url-list-item')!;
    urlListItem.focus();
    powerBookmarksList.getKeyboardNavigationServiceforTesting()
        .setCurrentFocusIndex(folderRow);

    // Expand with Right Arrow.
    let toggleEvent =
        eventToPromise('power-bookmark-toggle', powerBookmarksList);
    folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await toggleEvent;
    await flushTasks();
    await waitAfterNextRender(powerBookmarksList);
    assertTrue(
        folderRow.toggleExpand, 'Folder should be expanded after ArrowRight');
    assertTrue(
        !!getPowerBookmarksRowElement(folderRow, '6'),
        'Child bookmark should be visible');

    // Collapse with Left Arrow.
    toggleEvent = eventToPromise('power-bookmark-toggle', powerBookmarksList);
    folderRow.dispatchEvent(ARROW_LEFT_EVENT);
    await toggleEvent;
    await flushTasks();
    await waitAfterNextRender(powerBookmarksList);
    assertFalse(
        folderRow.toggleExpand, 'Folder should be collapsed after ArrowLeft');
    assertFalse(
        !!getPowerBookmarksRowElement(folderRow, '6'),
        'Child bookmark should not be visible');
  });

  test(
      'moves focus to first child on right arrow if already open', async () => {
        const folderRow = getPowerBookmarksRowElement(powerBookmarksList, '5');
        assertTrue(!!folderRow);
        await flushTasks();

        const urlListItem =
            folderRow.shadowRoot.querySelector('cr-url-list-item')!;
        urlListItem.focus();
        powerBookmarksList.getKeyboardNavigationServiceforTesting()
            .setCurrentFocusIndex(folderRow);

        // Expand the folder first.
        const toggleEvent =
            eventToPromise('power-bookmark-toggle', powerBookmarksList);
        folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
        await toggleEvent;
        await flushTasks();
        await waitAfterNextRender(powerBookmarksList);
        assertTrue(folderRow.toggleExpand, 'Folder should be expanded');

        const childRow = getPowerBookmarksRowElement(folderRow, '6')!;

        // Right arrow on expanded folder should move focus.
        folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
        await flushTasks();
        await waitAfterNextRender(powerBookmarksList);

        assertEquals(
            childRow.id, folderRow.shadowRoot.activeElement!.id,
            'Focus should move to the first child');
      });

  test('right arrow does nothing on non-folder', async () => {
    const bookmarkRow = getPowerBookmarksRowElement(powerBookmarksList, '3');
    assertTrue(!!bookmarkRow);

    const urlListItem =
        bookmarkRow.shadowRoot.querySelector('cr-url-list-item')!;
    urlListItem.focus();

    // This should not throw errors or change state.
    bookmarkRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await flushTasks();

    // No toggleExpand property to check, just make sure nothing broke.
    assertTrue(!!getPowerBookmarksRowElement(powerBookmarksList, '3'));
  });

  test('moves focus to parent on left arrow from child', async () => {
    const folderRow = getPowerBookmarksRowElement(powerBookmarksList, '5');
    assertTrue(!!folderRow);
    await flushTasks();

    const urlListItem = folderRow.shadowRoot.querySelector('cr-url-list-item')!;
    urlListItem.focus();
    powerBookmarksList.getKeyboardNavigationServiceforTesting()
        .setCurrentFocusIndex(folderRow);

    // Expand the folder first.
    const toggleEvent =
        eventToPromise('power-bookmark-toggle', powerBookmarksList);
    folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await toggleEvent;
    await flushTasks();
    await waitAfterNextRender(powerBookmarksList);
    assertTrue(folderRow.toggleExpand, 'Folder should be expanded');

    const childRow = getPowerBookmarksRowElement(folderRow, '6')!;

    // Right arrow on expanded folder should move focus.
    folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await flushTasks();
    await waitAfterNextRender(powerBookmarksList);

    assertEquals(
        childRow.id, folderRow.shadowRoot.activeElement!.id,
        'Focus should move to the first child');

    childRow.dispatchEvent(ARROW_LEFT_EVENT);
    await flushTasks();
    await waitAfterNextRender(powerBookmarksList);

    assertEquals(
        folderRow.id, powerBookmarksList.shadowRoot!.activeElement!.id,
        'Focus should move to the parent row');
  });

  test('LogsMetricsCountExpanded', async () => {
    powerBookmarksList = await initializeUi(bookmarksApi);

    const folderRow = getPowerBookmarksRowElement(powerBookmarksList, '5');
    assertTrue(!!folderRow);

    const urlListItem = folderRow.shadowRoot.querySelector('cr-url-list-item')!;
    urlListItem.focus();

    const toggleEvent =
        eventToPromise('power-bookmark-toggle', powerBookmarksList);
    folderRow.dispatchEvent(ARROW_RIGHT_EVENT);
    await toggleEvent;
    await flushTasks();
    await waitAfterNextRender(powerBookmarksList);

    assertEquals(
        1, metrics.count('PowerBookmarks.SidePanel.BookmarksShown', 5));
  });
});
