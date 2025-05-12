// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import {SortOrder, ViewType} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import type {PowerBookmarkRowElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row.js';
import type {PowerBookmarksListElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';
import {PageCallbackRouter} from 'chrome://resources/cr_components/commerce/price_tracking.mojom-webui.js';
import type {PageRemote} from 'chrome://resources/cr_components/commerce/price_tracking.mojom-webui.js';
import {PriceTrackingBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createTestBookmarks, getBookmarks, getBookmarksInList, getBookmarkWithId, getPowerBookmarksRowElement, initializeUi} from './power_bookmarks_list_test_util.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('General', () => {
  const FOLDERS = createTestBookmarks();
  let powerBookmarksList: PowerBookmarksListElement;
  let bookmarksApi: TestBookmarksApiProxy;
  const priceTrackingProxy = TestMock.fromClass(PriceTrackingBrowserProxyImpl);
  let callbackRouterRemote: PageRemote;
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>&
      PageImageServiceHandlerRemote;
  let metrics: MetricsTracker;

  function getAddTabButton(): CrButtonElement {
    return powerBookmarksList.shadowRoot!.querySelector<CrButtonElement>(
        '#addCurrentTabButton')!;
  }

  function getAddNewFolderButton() {
    return powerBookmarksList.shadowRoot!.querySelector<CrButtonElement>(
        '.new-folder-row')!;
  }

  function getCrUrlListItemElementWithId(id: string): CrUrlListItemElement|
      undefined {
    const powerBookmarkRowElement =
        getPowerBookmarksRowElement(powerBookmarksList, id);
    if (!powerBookmarkRowElement) {
      return undefined;
    }
    return powerBookmarkRowElement.currentUrlListItem_;
  }

  function isHidden(element: HTMLElement): boolean {
    return element.matches('[hidden], [hidden] *');
  }

  async function performSearch(query: string) {
    const searchField = powerBookmarksList.shadowRoot!.querySelector(
        'cr-toolbar-search-field')!;
    const searchChanged = eventToPromise('search-changed', searchField);
    searchField.$.searchInput.value = query;
    searchField.onSearchTermInput();
    searchField.onSearchTermSearch();

    await searchChanged;
    await flushTasks();
    await waitAfterNextRender(powerBookmarksList);
  }

  async function openBookmark(id: string) {
    const bookmark = getBookmarkWithId(powerBookmarksList, id);
    assertTrue(!!bookmark);
    powerBookmarksList.clickBookmarkRowForTests(bookmark);

    await flushTasks();
    await waitAfterNextRender(powerBookmarksList);
  }

  async function selectBookmark(id: string) {
    const checkboxClicked = eventToPromise(
        'checkbox-change',
        getPowerBookmarksRowElement(powerBookmarksList, id)!,
    );
    const bookmarkListItem = getCrUrlListItemElementWithId(id);
    assertTrue(!!bookmarkListItem);
    await bookmarkListItem.updateComplete;
    bookmarkListItem.click();
    await checkboxClicked;
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
    callbackRouterRemote = callbackRouter.$.bindNewPipeAndPassRemote();
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
      isBookmarksInTransportModeEnabled: false,
    });

    powerBookmarksList = await initializeUi(bookmarksApi);
  });

  suite('Part1', function() {
    test('GetsAndShowsTopLevelBookmarks', () => {
      assertEquals(1, bookmarksApi.getCallCount('getAllBookmarks'));
      assertEquals(
          FOLDERS[1]!.children!.length + 1,
          getBookmarks(powerBookmarksList).length);
    });

    test('RebuildsKeyboardNavigationOnBoomkmarkNodeAdded', async () => {
      await flushTasks();

      assertEquals(
          JSON.stringify(
              powerBookmarksList.getKeyboardNavigationServiceforTesting()
                  .getElementsForTesting()
                  .map((el: HTMLElement) => el.id)),
          JSON.stringify([
            'bookmark-SIDE_PANEL_BOOKMARK_BAR_ID',
            'bookmark-5',
            'bookmark-4',
            'bookmark-3',
          ]));

      bookmarksApi.callbackRouterRemote.onBookmarkNodeAdded({
        id: '999',
        title: 'New bookmark of current url',
        index: 0,
        parentId: FOLDERS[1]!.id,
        url: powerBookmarksList.getCurrentUrlForTesting()!,
        children: null,
        dateAdded: null,
        dateLastUsed: null,
        unmodifiable: false,
      });
      await microtasksFinished();
      await flushTasks();

      assertEquals(
          JSON.stringify(
              powerBookmarksList.getKeyboardNavigationServiceforTesting()
                  .getElementsForTesting()
                  .map((el: HTMLElement) => el.id)),
          JSON.stringify([
            'bookmark-SIDE_PANEL_BOOKMARK_BAR_ID',
            'bookmark-5',
            'bookmark-999',
            'bookmark-4',
            'bookmark-3',
          ]));
    });

    test('RebuildsKeyboardNavigationOnRemoved', async () => {
      await flushTasks();

      assertEquals(
          JSON.stringify(
              powerBookmarksList.getKeyboardNavigationServiceforTesting()
                  .getElementsForTesting()
                  .map((el: HTMLElement) => el.id)),
          JSON.stringify([
            'bookmark-SIDE_PANEL_BOOKMARK_BAR_ID',
            'bookmark-5',
            'bookmark-4',
            'bookmark-3',
          ]));

      bookmarksApi.callbackRouterRemote.onBookmarkNodesRemoved(['4']);
      await flushTasks();
      await waitAfterNextRender(powerBookmarksList);

      assertEquals(
          JSON.stringify(
              powerBookmarksList.getKeyboardNavigationServiceforTesting()
                  .getElementsForTesting()
                  .map((el: HTMLElement) => el.id)),
          JSON.stringify([
            'bookmark-SIDE_PANEL_BOOKMARK_BAR_ID',
            'bookmark-5',
            'bookmark-3',
          ]));
    });

    test('RebuildsKeyboardNavigationMoved', async () => {
      await flushTasks();

      assertEquals(
          JSON.stringify(
              powerBookmarksList.getKeyboardNavigationServiceforTesting()
                  .getElementsForTesting()
                  .map((el: HTMLElement) => el.id)),
          JSON.stringify([
            'bookmark-SIDE_PANEL_BOOKMARK_BAR_ID',
            'bookmark-5',
            'bookmark-4',
            'bookmark-3',
          ]));

      const movedBookmark = FOLDERS[1]!.children![2]!.children![0]!;
      assertTrue(!!movedBookmark);
      bookmarksApi.callbackRouterRemote.onBookmarkNodeMoved(
          /*oldParentId=*/ FOLDERS[1]!.children![2]!
              .id,  // Moving from child folder.
          /*oldIndex=*/ 0,
          /*parentId=*/ FOLDERS[1]!.id,  // Moving to other bookmarks.
          /*index=*/ 0,
      );
      await microtasksFinished();
      await flushTasks();

      assertEquals(
          JSON.stringify(
              powerBookmarksList.getKeyboardNavigationServiceforTesting()
                  .getElementsForTesting()
                  .map((el: HTMLElement) => el.id)),
          JSON.stringify([
            'bookmark-SIDE_PANEL_BOOKMARK_BAR_ID',
            'bookmark-5',
            'bookmark-6',
            'bookmark-4',
            'bookmark-3',
          ]));
    });

    test('DefaultsToSortByNewest', () => {
      const bookmarks = getBookmarks(powerBookmarksList);
      assertEquals(4, bookmarks.length);
      // All folders should come first
      assertEquals('SIDE_PANEL_BOOKMARK_BAR_ID', bookmarks[0]!.id);
      assertEquals('5', bookmarks[1]!.id);
      // Newest URL should come next
      assertEquals('4', bookmarks[2]!.id);
      // Older URL should be last
      assertEquals('3', bookmarks[3]!.id);
    });

    test('FiltersBookmarks', async () => {
      await openBookmark('5');
      await performSearch('bookmark');

      // One bookmark matches the query and is in the active folder.
      assertEquals(1, getBookmarksInList(powerBookmarksList, 0).length);
      // Three bookmarks match the query but are not in the active folder.
      assertEquals(3, getBookmarksInList(powerBookmarksList, 1).length);
      assertEquals(
          1,
          metrics.count(
              'PowerBookmarks.SidePanel.SearchOrFilter.BookmarksShown', 4));

      await performSearch('nested');

      assertEquals(1, getBookmarksInList(powerBookmarksList, 0).length);
      assertEquals(0, getBookmarksInList(powerBookmarksList, 1).length);
      assertEquals(
          1,
          metrics.count(
              'PowerBookmarks.SidePanel.SearchOrFilter.BookmarksShown', 1));

      await performSearch('child');

      assertEquals(0, getBookmarksInList(powerBookmarksList, 0).length);
      assertEquals(2, getBookmarksInList(powerBookmarksList, 1).length);
      assertEquals(
          1,
          metrics.count(
              'PowerBookmarks.SidePanel.SearchOrFilter.BookmarksShown', 2));
      // Bookmark list is shown when there are no filter results in the active
      // folder.
      assertTrue(isHidden(powerBookmarksList.$.topLevelEmptyState));
      assertFalse(isHidden(powerBookmarksList.$.bookmarks));
    });

    test('UpdatesChangedBookmarks', async () => {
      const changedBookmark = FOLDERS[1]!.children![0]!;
      bookmarksApi.callbackRouterRemote.onBookmarkNodeChanged(
          changedBookmark.id, 'New title', 'http://new/url');
      await flushTasks();

      const bookmark = getBookmarkWithId(powerBookmarksList, '3');
      assertTrue(!!bookmark);

      assertEquals('New title', bookmark.title);
      assertEquals('http://new/url', bookmark.url);

      const crUrlListItemElement = getCrUrlListItemElementWithId('3');
      assertTrue(!!crUrlListItemElement);
      await crUrlListItemElement.updateComplete;

      assertEquals('New title', crUrlListItemElement.title);
    });

    test('UpdatesChangedBookmarksWithFilter', async () => {
      await performSearch('abc');

      assertEquals(0, getBookmarks(powerBookmarksList).length);

      const changedBookmark = FOLDERS[1]!.children![0]!;
      bookmarksApi.callbackRouterRemote.onBookmarkNodeChanged(
          changedBookmark.id, 'abcdef', 'http://new/url');
      await flushTasks();

      // Bookmark matches search term and should display.
      assertEquals(1, getBookmarks(powerBookmarksList).length);

      bookmarksApi.callbackRouterRemote.onBookmarkNodeChanged(
          changedBookmark.id, 'New title', 'http://new/url');
      await flushTasks();

      // Bookmark no longer matches search term and should not display.
      assertEquals(0, getBookmarks(powerBookmarksList).length);
    });

    test('DefaultsAddTabButtonEnabled', () => {
      const btn = getAddTabButton();
      // The AddTabButton is enabled because the current url is not bookmarked.
      assertFalse(btn.disabled);
    });

    test('UpdatesAddTabButton', async () => {
      bookmarksApi.callbackRouterRemote.onBookmarkNodeAdded({
        id: '999',
        title: 'New bookmark of current url',
        index: 0,
        parentId: FOLDERS[1]!.id,
        url: powerBookmarksList.getCurrentUrlForTesting()!,
        children: null,
        dateAdded: null,
        dateLastUsed: null,
        unmodifiable: false,
      });
      await flushTasks();

      let btn = getAddTabButton();
      assertTrue(btn.disabled);

      bookmarksApi.callbackRouterRemote.onBookmarkNodesRemoved(['999']);
      await flushTasks();

      btn = getAddTabButton();
      assertFalse(btn.disabled);
    });

    test('AddNewFolderClicked', async () => {
      const addNewFolderButton = getAddNewFolderButton();
      assertTrue(!!addNewFolderButton);
      assertFalse(addNewFolderButton.disabled);

      addNewFolderButton.click();
      await bookmarksApi.whenCalled('createFolder');
    });

    test('AddsCreatedBookmark', async () => {
      bookmarksApi.callbackRouterRemote.onBookmarkNodeAdded({
        id: '999',
        title: 'New bookmark',
        index: 0,
        parentId: FOLDERS[1]!.id,
        url: 'http://new/bookmark',
        children: null,
        dateAdded: null,
        dateLastUsed: null,
        unmodifiable: false,
      });
      await flushTasks();

      const bookmarks = getBookmarks(powerBookmarksList);
      assertEquals(5, bookmarks.length);
    });

    test('AddsCreatedBookmarkForNewFolder', async () => {
      // Create a new folder without a children array.
      bookmarksApi.callbackRouterRemote.onBookmarkNodeAdded({
        id: '1000',
        title: 'New folder',
        index: 0,
        parentId: FOLDERS[1]!.id,
        url: null,
        children: null,
        dateAdded: null,
        dateLastUsed: null,
        unmodifiable: false,
      });
      await flushTasks();

      // Create a new bookmark within that folder.
      bookmarksApi.callbackRouterRemote.onBookmarkNodeAdded({
        id: '1001',
        title: 'New bookmark in new folder',
        index: 0,
        parentId: '1000',
        url: 'http://google.com',
        children: null,
        dateAdded: null,
        dateLastUsed: null,
        unmodifiable: false,
      });
      await flushTasks();

      const bookmarks = getBookmarks(powerBookmarksList);
      assertEquals(5, bookmarks.length);

      const newFolder = getBookmarkWithId(powerBookmarksList, '1000');
      assertTrue(!!newFolder);

      assertEquals(1, newFolder.children!.length);
    });

    test('AddsCreatedBookmarkWithFilter', async () => {
      await openBookmark('5');
      await performSearch('bookmark');

      assertEquals(1, getBookmarksInList(powerBookmarksList, 0).length);
      assertEquals(3, getBookmarksInList(powerBookmarksList, 1).length);

      bookmarksApi.callbackRouterRemote.onBookmarkNodeAdded({
        id: '123',
        title: 'New bookmark',
        index: 0,
        parentId: '5',
        url: 'http://new/bookmark',
        children: null,
        dateAdded: null,
        dateLastUsed: null,
        unmodifiable: false,
      });
      await flushTasks();

      // New bookmark matches search term and is under active folder, gets
      // displayed in primary list
      assertTrue(!!getBookmarkWithId(powerBookmarksList, '123'));
      assertEquals(2, getBookmarksInList(powerBookmarksList, 0).length);
      assertEquals(3, getBookmarksInList(powerBookmarksList, 1).length);

      bookmarksApi.callbackRouterRemote.onBookmarkNodeAdded({
        id: '456',
        title: 'foo',
        index: 0,
        parentId: FOLDERS[1]!.id,
        url: 'http://foo',
        children: null,
        dateAdded: null,
        dateLastUsed: null,
        unmodifiable: false,
      });
      await flushTasks();

      // New bookmark does not match search term, doesn't get displayed
      assertFalse(!!getBookmarkWithId(powerBookmarksList, '456'));
      assertEquals(2, getBookmarksInList(powerBookmarksList, 0).length);
      assertEquals(3, getBookmarksInList(powerBookmarksList, 1).length);

      bookmarksApi.callbackRouterRemote.onBookmarkNodeAdded({
        id: '789',
        title: 'Bookmark',
        index: 0,
        parentId: FOLDERS[1]!.id,
        url: 'http://bookmark',
        children: null,
        dateAdded: null,
        dateLastUsed: null,
        unmodifiable: false,
      });
      await flushTasks();

      // New bookmark matches search term and is not under active folder, gets
      // displayed in secondary list
      assertTrue(!!getBookmarkWithId(powerBookmarksList, '789'));
      assertEquals(2, getBookmarksInList(powerBookmarksList, 0).length);
      assertEquals(4, getBookmarksInList(powerBookmarksList, 1).length);
    });
  });

  suite('Part2', function() {
    test('MovesBookmarks', async () => {
      const movedBookmark = FOLDERS[1]!.children![2]!.children![0]!;
      assertTrue(!!movedBookmark);
      bookmarksApi.callbackRouterRemote.onBookmarkNodeMoved(
          /*oldParentId=*/ FOLDERS[1]!.children![2]!
              .id,  // Moving from child folder.
          /*oldIndex=*/ 0,
          /*parentId=*/ FOLDERS[1]!.id,  // Moving to other bookmarks.
          /*index=*/ 0,
      );
      await flushTasks();

      const bookmarks = getBookmarks(powerBookmarksList);
      assertEquals(5, bookmarks.length);

      const childFolder = getBookmarkWithId(powerBookmarksList, '5');
      assertTrue(!!childFolder);
      assertEquals(0, childFolder.children!.length);
    });

    test('MovesBookmarksIntoNewFolder', async () => {
      // Create a new folder without a children array.
      bookmarksApi.callbackRouterRemote.onBookmarkNodeAdded({
        id: '1000',
        title: 'New folder',
        index: 0,
        parentId: FOLDERS[1]!.id,
        url: null,
        children: null,
        dateAdded: null,
        dateLastUsed: null,
        unmodifiable: false,
      });
      await flushTasks();

      const movedBookmark = FOLDERS[1]!.children![2]!.children![0]!;
      assertTrue(!!movedBookmark);
      bookmarksApi.callbackRouterRemote.onBookmarkNodeMoved(
          /*oldParentId=*/ FOLDERS[1]!.children![2]!.id,
          /*oldIndex=*/ 0,
          /*parentId=*/ '1000',
          /*index=*/ 0,
      );
      await flushTasks();

      const newFolder = getBookmarkWithId(powerBookmarksList, '1000');
      assertTrue(!!newFolder);

      assertEquals(1, newFolder.children!.length);
    });

    test('MovesBookmarkWithFilter', async () => {
      await openBookmark('5');
      await performSearch('bookmark');

      assertEquals(1, getBookmarksInList(powerBookmarksList, 0).length);
      assertEquals(3, getBookmarksInList(powerBookmarksList, 1).length);

      const movedBookmark = getBookmarkWithId(powerBookmarksList, '6');
      assertTrue(!!movedBookmark);
      bookmarksApi.callbackRouterRemote.onBookmarkNodeMoved(
          /*oldParentId=*/ FOLDERS[1]!.children![2]!
              .id,  // Moving from child folder.
          /*oldIndex=*/ 0,
          /*parentId=*/ FOLDERS[1]!.id,  // Moving to other bookmarks.
          /*index=*/ 0,
      );
      await flushTasks();

      // Moved bookmark is no longer in active folder, should move from primary
      // to secondary list.
      assertEquals(0, getBookmarksInList(powerBookmarksList, 0).length);
      assertEquals(4, getBookmarksInList(powerBookmarksList, 1).length);

      bookmarksApi.callbackRouterRemote.onBookmarkNodeMoved(
          /*oldParentId=*/ FOLDERS[1]!.id,  // Moving from other bookmarks.
          /*oldIndex=*/ 0,
          /*parentId=*/ FOLDERS[1]!.children![2]!.id,  // Moving to child
                                                       // folder.
          /*index=*/ 0,
      );
      await flushTasks();

      // Moved bookmark is now in active folder, should move from secondary
      // to primary list.
      assertEquals(1, getBookmarksInList(powerBookmarksList, 0).length);
      assertEquals(3, getBookmarksInList(powerBookmarksList, 1).length);
    });

    test('RemovesBookmark', async () => {
      const originalShownBookmarkCount =
          getBookmarks(powerBookmarksList).length;

      bookmarksApi.callbackRouterRemote.onBookmarkNodesRemoved(['3']);
      await flushTasks();

      const removedBookmark = getBookmarkWithId(powerBookmarksList, '3');
      assertTrue(!removedBookmark);

      assertEquals(
          originalShownBookmarkCount - 1,
          getBookmarks(powerBookmarksList).length);
    });

    test('SetsCompactDescription', () => {
      const folder = getBookmarkWithId(powerBookmarksList, '5');
      assertTrue(!!folder);

      assertEquals(
          '(1)',
          getPowerBookmarksRowElement(powerBookmarksList, '5')
              ?.getBookmarkDescriptionForTests(folder));
    });

    test('SetsExpandedDescription', () => {
      const viewButton: HTMLElement =
          powerBookmarksList.shadowRoot!.querySelector('#viewButton')!;
      viewButton.click();

      const folder = getBookmarkWithId(powerBookmarksList, '4');
      assertTrue(!!folder);

      assertEquals(
          'child',
          getPowerBookmarksRowElement(powerBookmarksList, '4')
              ?.getBookmarkDescriptionForTests(folder));
    });

    test('SetsExpandedSearchResultDescription', async () => {
      const viewButton =
          powerBookmarksList.shadowRoot!.querySelector<HTMLElement>(
              '#viewButton')!;
      viewButton.click();

      await performSearch('child bookmark');

      const folder = getBookmarkWithId(powerBookmarksList, '4');
      assertTrue(!!folder);

      assertEquals(
          'child - All Bookmarks',
          getPowerBookmarksRowElement(powerBookmarksList, '4')
              ?.getBookmarkDescriptionForTests(folder));
    });

    test('RenamesBookmark', async () => {
      const renamedBookmarkId = '4';
      powerBookmarksList.setRenamingIdForTests(renamedBookmarkId);

      await flushTasks();

      const rowElement =
          getPowerBookmarksRowElement(powerBookmarksList, renamedBookmarkId);
      assertTrue(!!rowElement);
      let input =
          rowElement.shadowRoot.querySelector<CrInputElement>('cr-input');
      assertTrue(!!input);

      const inputChange = eventToPromise('input-change', rowElement);

      const newName = 'foo';
      input.value = newName;
      input.inputElement.dispatchEvent(new Event('change'));

      await inputChange;
      await flushTasks();

      // Committing a new input value should rename the bookmark and remove the
      // input.
      assertEquals(1, bookmarksApi.getCallCount('renameBookmark'));
      assertEquals(
          renamedBookmarkId, bookmarksApi.getArgs('renameBookmark')[0][0]);
      assertEquals(newName, bookmarksApi.getArgs('renameBookmark')[0][1]);
      input = rowElement.shadowRoot.querySelector<CrInputElement>('cr-input');
      assertFalse(!!input);
    });

    test('BlursRenameInput', async () => {
      const renamedBookmarkId = '4';
      powerBookmarksList.setRenamingIdForTests(renamedBookmarkId);

      await flushTasks();

      const rowElement =
          powerBookmarksList.shadowRoot!.querySelector<PowerBookmarkRowElement>(
              `#bookmark-${renamedBookmarkId}`);
      assertTrue(!!rowElement);
      let input =
          rowElement.shadowRoot.querySelector<CrInputElement>('cr-input');
      const inputBlurred = eventToPromise(
          'input-change',
          getPowerBookmarksRowElement(powerBookmarksList, renamedBookmarkId)!);
      assertTrue(!!input);
      input.inputElement.blur();
      await inputBlurred;

      await flushTasks();

      // Blurring the input should remove it.
      input = rowElement.shadowRoot.querySelector<CrInputElement>('cr-input');
      assertFalse(!!input);
    });

    test('ShowsFolderImages', () => {
      const viewButton: HTMLElement =
          powerBookmarksList.shadowRoot!.querySelector('#viewButton')!;
      viewButton.click();

      flush();

      const bookmarksBarFolderElement =
          getCrUrlListItemElementWithId('SIDE_PANEL_BOOKMARK_BAR_ID');
      assertTrue(!!bookmarksBarFolderElement);
      assertEquals(0, bookmarksBarFolderElement.imageUrls.length);

      const childFolderElement = getCrUrlListItemElementWithId('5');
      assertTrue(!!childFolderElement);
      assertNotEquals(0, childFolderElement.imageUrls.length);
    });

    test('DeletesSelectedBookmarks', async () => {
      const editButton: HTMLElement =
          powerBookmarksList.shadowRoot!.querySelector('#editButton')!;
      editButton.click();

      flush();

      await selectBookmark('3');
      await selectBookmark('5');

      flush();

      const deleteButton: HTMLButtonElement =
          powerBookmarksList.shadowRoot!.querySelector('#deleteButton')!;
      assertFalse(deleteButton.disabled);
      deleteButton.click();

      flush();

      assertEquals(1, bookmarksApi.getCallCount('deleteBookmarks'));
      assertEquals('3', bookmarksApi.getArgs('deleteBookmarks')[0][0]);
      assertEquals('5', bookmarksApi.getArgs('deleteBookmarks')[0][1]);
    });

    test('EditBookmarkWithBookmarksInTransportModeDisabled', async () => {
      const bookmark = getBookmarkWithId(powerBookmarksList, '3')!;
      const contextMenu = powerBookmarksList.$.contextMenu;
      const editClicked = eventToPromise('edit-clicked', contextMenu);

      // Open the context menu.
      contextMenu.showAtPosition(
          new MouseEvent('click'), [bookmark], false, false, false);
      await waitAfterNextRender(contextMenu);

      // Get the edit option in the menu.
      const menuItems =
          contextMenu.shadowRoot!.querySelectorAll('.dropdown-item');
      assertEquals(
          menuItems[3]!.textContent!.includes(
              loadTimeData.getString('menuEdit')),
          true);
      const editItem = contextMenu.shadowRoot!.querySelectorAll<HTMLElement>(
          '.dropdown-item')[3]!;

      // Click on edit and wait for the call to propagate.
      editItem.click();
      await editClicked;
      await flushTasks();

      // The edit dialog is opened.
      const editDialog = powerBookmarksList.$.editDialog;
      assertTrue(editDialog.$.dialog.open);
      assertEquals(bookmark.title, editDialog.$.nameInput.inputElement.value);
      assertEquals(bookmark.url, editDialog.$.urlInput.inputElement.value);
    });

    test('MoveBookmarksWithBookmarksInTransportModeDisabled', async () => {
      const bookmarks = [
        getBookmarkWithId(powerBookmarksList, '3')!,
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
          menuItems[4]!.textContent!.includes(
              loadTimeData.getString('tooltipMove')),
          true);
      const moveItem = contextMenu.shadowRoot!.querySelectorAll<HTMLElement>(
          '.dropdown-item')[4]!;

      // Click on move and wait for the call to propagate.
      moveItem.click();
      await editClicked;
      await flushTasks();

      // The edit dialog is opened.
      const editDialog = powerBookmarksList.$.editDialog;
      assertTrue(editDialog.$.dialog.open);
    });

    test('LogsBookmarkCountMetric', async () => {
      // Initially should have 4 bookmarks shown.
      assertEquals(
          1, metrics.count('PowerBookmarks.SidePanel.BookmarksShown', 4));

      await openBookmark('5');

      // Folder with id 5 only has 1 bookmark shown.
      assertEquals(
          1, metrics.count('PowerBookmarks.SidePanel.BookmarksShown', 1));
    });

    test('TogglesSectionVisibilityAndEmptyStates', async () => {
      const search = powerBookmarksList.$.searchField;
      const labels = powerBookmarksList.$.labels;
      const heading = powerBookmarksList.$.heading;
      const folderEmptyState = powerBookmarksList.$.folderEmptyState;
      const bookmarksList = powerBookmarksList.$.bookmarks;
      const topLevelEmptyState = powerBookmarksList.$.topLevelEmptyState;
      const footer = powerBookmarksList.$.footer;
      assertEquals(
          loadTimeData.getString('emptyTitle'), topLevelEmptyState.heading);
      assertEquals(
          loadTimeData.getString('emptyBody'), topLevelEmptyState.body);

      // Has bookmarks.
      assertFalse(isHidden(search));
      assertTrue(isHidden(labels));
      assertFalse(isHidden(heading));
      assertTrue(isHidden(folderEmptyState));
      assertFalse(isHidden(bookmarksList));
      assertTrue(isHidden(topLevelEmptyState));
      assertFalse(isHidden(footer));

      // Opening an empty folder.
      await openBookmark('SIDE_PANEL_BOOKMARK_BAR_ID');

      assertFalse(isHidden(search));
      assertTrue(isHidden(labels));
      assertFalse(isHidden(heading));
      assertFalse(isHidden(folderEmptyState));
      assertTrue(isHidden(bookmarksList));
      assertTrue(isHidden(topLevelEmptyState));
      assertFalse(isHidden(footer));

      // A search with no results.
      const searchField = powerBookmarksList.shadowRoot!.querySelector(
          'cr-toolbar-search-field');
      assertTrue(!!searchField);
      searchField.$.searchInput.value = 'abcdef';
      searchField.onSearchTermSearch();
      await flushTasks();
      assertEquals(
          loadTimeData.getString('emptyTitleSearch'),
          topLevelEmptyState.heading);
      assertFalse(isHidden(search));
      assertTrue(isHidden(labels));
      assertTrue(isHidden(heading));
      assertTrue(isHidden(folderEmptyState));
      assertTrue(isHidden(bookmarksList));
      assertFalse(isHidden(topLevelEmptyState));
      assertTrue(isHidden(footer));

      // Adding a tracked product shows filter chips.
      const newProduct = {
        bookmarkId: BigInt(3),
        info: {
          title: 'Product Baz',
          clusterTitle: 'Product Cluster Baz',
          domain: 'baz.com',
          imageUrl: {url: 'https://baz.com/image'},
          productUrl: {url: 'https://baz.com/product'},
          currentPrice: '$56',
          previousPrice: '$78',
          clusterId: BigInt(12345),
          categoryLabels: [],
          priceSummary: '',
        },
      };

      callbackRouterRemote.priceTrackedForBookmark(newProduct);
      await flushTasks();
      assertFalse(isHidden(labels));
    });
  });
});
