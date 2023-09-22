// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import {SortOrder, ViewType} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {PowerBookmarkRowElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row.js';
import {PowerBookmarksListElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';
import {ShoppingListApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
import {PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {CrUrlListItemElement} from 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {TestShoppingListApiProxy} from './commerce/test_shopping_list_api_proxy.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelPowerBookmarksListTest', () => {
  let powerBookmarksList: PowerBookmarksListElement;
  let bookmarksApi: TestBookmarksApiProxy;
  let shoppingListApi: TestShoppingListApiProxy;
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>&
      PageImageServiceHandlerRemote;

  const folders: chrome.bookmarks.BookmarkTreeNode[] = [
    {
      id: '1',
      parentId: '0',
      title: 'Bookmarks Bar',
      children: [],
    },
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

  function getBookmarks() {
    return getBookmarksInList(0).concat(getBookmarksInList(1));
  }

  function getBookmarksInList(listIndex: number):
      chrome.bookmarks.BookmarkTreeNode[] {
    const ironList =
        powerBookmarksList.shadowRoot!.querySelector<IronListElement>(
            `#shownBookmarksIronList${listIndex}`);
    if (!ironList || !ironList.items) {
      return [];
    }
    return ironList.items!;
  }

  function getBookmarkWithId(id: string): chrome.bookmarks.BookmarkTreeNode|
      undefined {
    return getBookmarks().find((bookmark) => bookmark.id === id);
  }

  function getCrUrlListItemElementWithId(id: string): CrUrlListItemElement|
      undefined {
    const powerBookmarkRowElement =
        powerBookmarksList.shadowRoot!.querySelector<PowerBookmarkRowElement>(
            `#bookmark-${id}`);
    if (!powerBookmarkRowElement) {
      return undefined;
    }
    return powerBookmarkRowElement.$.crUrlListItem;
  }

  function isHidden(element: HTMLElement): boolean {
    return element.matches('[hidden], [hidden] *');
  }

  async function performSearch(query: string) {
    const searchField = powerBookmarksList.shadowRoot!.querySelector(
        'cr-toolbar-search-field')!;
    searchField.$.searchInput.value = query;
    searchField.onSearchTermInput();
    searchField.onSearchTermSearch();

    await flushTasks();
  }

  async function openBookmark(id: string) {
    const bookmark = getBookmarkWithId(id);
    assertTrue(!!bookmark);
    powerBookmarksList.clickBookmarkRowForTests(bookmark);

    await flushTasks();
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(JSON.parse(JSON.stringify(folders)));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    shoppingListApi = new TestShoppingListApiProxy();
    ShoppingListApiProxyImpl.setInstance(shoppingListApi);

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
    });

    powerBookmarksList = document.createElement('power-bookmarks-list');

    // Ensure the PowerBookmarksListElement is given a fixed height to expand
    // to.
    const parentElement = document.createElement('div');
    parentElement.style.height = '500px';
    parentElement.appendChild(powerBookmarksList);
    document.body.appendChild(parentElement);

    await bookmarksApi.whenCalled('getFolders');
    await waitAfterNextRender(powerBookmarksList);
    flush();
  });

  test('GetsAndShowsTopLevelBookmarks', () => {
    assertEquals(1, bookmarksApi.getCallCount('getFolders'));
    assertEquals(folders[1]!.children!.length + 1, getBookmarks().length);
  });

  test('DefaultsToSortByNewest', () => {
    const bookmarks = getBookmarks();
    assertEquals(4, bookmarks.length);
    // All folders should come first
    assertEquals('1', bookmarks[0]!.id);
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
    assertEquals(1, getBookmarksInList(0).length);
    // Three bookmarks match the query but are not in the active folder.
    assertEquals(3, getBookmarksInList(1).length);

    await performSearch('nested');

    assertEquals(1, getBookmarksInList(0).length);
    assertEquals(0, getBookmarksInList(1).length);

    await performSearch('child');

    assertEquals(0, getBookmarksInList(0).length);
    assertEquals(2, getBookmarksInList(1).length);
  });

  test('UpdatesChangedBookmarks', () => {
    const changedBookmark = folders[1]!.children![0]!;
    bookmarksApi.callbackRouter.onChanged.callListeners(changedBookmark.id, {
      title: 'New title',
      url: 'http://new/url',
    });
    flush();

    const bookmark = getBookmarkWithId('3');
    assertTrue(!!bookmark);

    assertEquals('New title', bookmark.title);
    assertEquals('http://new/url', bookmark.url);

    const crUrlListItemElement = getCrUrlListItemElementWithId('3');
    assertTrue(!!crUrlListItemElement);

    assertEquals('New title', crUrlListItemElement.title);
  });

  test('UpdatesChangedBookmarksWithFilter', async () => {
    await performSearch('abc');

    assertEquals(0, getBookmarks().length);

    const changedBookmark = folders[1]!.children![0]!;
    bookmarksApi.callbackRouter.onChanged.callListeners(changedBookmark.id, {
      title: 'abcdef',
      url: 'http://new/url',
    });
    flush();

    // Bookmark matches search term and should display.
    assertEquals(1, getBookmarks().length);

    bookmarksApi.callbackRouter.onChanged.callListeners(changedBookmark.id, {
      title: 'New title',
      url: 'http://new/url',
    });
    flush();

    // Bookmark no longer matches search term and should not display.
    assertEquals(0, getBookmarks().length);
  });

  test('AddsCreatedBookmark', async () => {
    bookmarksApi.callbackRouter.onCreated.callListeners('999', {
      id: '999',
      title: 'New bookmark',
      index: 0,
      parentId: folders[1]!.id,
      url: 'http://new/bookmark',
    });
    flush();

    const bookmarks = getBookmarks();
    assertEquals(5, bookmarks.length);
  });

  test('AddsCreatedBookmarkForNewFolder', () => {
    // Create a new folder without a children array.
    bookmarksApi.callbackRouter.onCreated.callListeners('1000', {
      id: '1000',
      title: 'New folder',
      index: 0,
      parentId: folders[1]!.id,
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

    const bookmarks = getBookmarks();
    assertEquals(5, bookmarks.length);

    const newFolder = getBookmarkWithId('1000');
    assertTrue(!!newFolder);

    assertEquals(1, newFolder.children!.length);
  });

  test('AddsCreatedBookmarkWithFilter', async () => {
    await openBookmark('5');
    await performSearch('bookmark');

    assertEquals(1, getBookmarksInList(0).length);
    assertEquals(3, getBookmarksInList(1).length);

    bookmarksApi.callbackRouter.onCreated.callListeners('123', {
      id: '123',
      title: 'New bookmark',
      index: 0,
      parentId: '5',
      url: 'http://new/bookmark',
    });
    flush();

    // New bookmark matches search term and is under active folder, gets
    // displayed in primary list
    assertTrue(!!getBookmarkWithId('123'));
    assertEquals(2, getBookmarksInList(0).length);
    assertEquals(3, getBookmarksInList(1).length);

    bookmarksApi.callbackRouter.onCreated.callListeners('456', {
      id: '456',
      title: 'foo',
      index: 0,
      parentId: folders[1]!.id,
      url: 'http://foo',
    });
    flush();

    // New bookmark does not match search term, doesn't get displayed
    assertFalse(!!getBookmarkWithId('456'));
    assertEquals(2, getBookmarksInList(0).length);
    assertEquals(3, getBookmarksInList(1).length);

    bookmarksApi.callbackRouter.onCreated.callListeners('789', {
      id: '789',
      title: 'Bookmark',
      index: 0,
      parentId: folders[1]!.id,
      url: 'http://bookmark',
    });
    flush();

    // New bookmark matches search term and is not under active folder, gets
    // displayed in secondary list
    assertTrue(!!getBookmarkWithId('789'));
    assertEquals(2, getBookmarksInList(0).length);
    assertEquals(4, getBookmarksInList(1).length);
  });

  test('MovesBookmarks', () => {
    const movedBookmark = folders[1]!.children![2]!.children![0]!;
    bookmarksApi.callbackRouter.onMoved.callListeners(movedBookmark.id, {
      index: 0,
      parentId: folders[1]!.id,                   // Moving to other bookmarks.
      oldParentId: folders[1]!.children![2]!.id,  // Moving from child folder.
      oldIndex: 0,
    });
    flush();

    const bookmarks = getBookmarks();
    assertEquals(5, bookmarks.length);

    const childFolder = getBookmarkWithId('5');
    assertTrue(!!childFolder);
    assertEquals(0, childFolder.children!.length);
  });

  test('MovesBookmarksIntoNewFolder', () => {
    // Create a new folder without a children array.
    bookmarksApi.callbackRouter.onCreated.callListeners('1000', {
      id: '1000',
      title: 'New folder',
      index: 0,
      parentId: folders[1]!.id,
    });
    flush();

    const movedBookmark = folders[1]!.children![2]!.children![0]!;
    bookmarksApi.callbackRouter.onMoved.callListeners(movedBookmark.id, {
      index: 0,
      parentId: '1000',
      oldParentId: folders[1]!.children![2]!.id,
      oldIndex: 0,
    });
    flush();

    const newFolder = getBookmarkWithId('1000');
    assertTrue(!!newFolder);

    assertEquals(1, newFolder.children!.length);
  });

  test('MovesBookmarkWithFilter', async () => {
    await openBookmark('5');
    await performSearch('bookmark');

    assertEquals(1, getBookmarksInList(0).length);
    assertEquals(3, getBookmarksInList(1).length);

    const movedBookmark = getBookmarkWithId('6');
    assertTrue(!!movedBookmark);
    bookmarksApi.callbackRouter.onMoved.callListeners(movedBookmark.id, {
      index: 0,
      parentId: folders[1]!.id,                   // Moving to other bookmarks.
      oldParentId: folders[1]!.children![2]!.id,  // Moving from child folder.
      oldIndex: 0,
    });
    flush();

    // Moved bookmark is no longer in active folder, should move from primary
    // to secondary list.
    assertEquals(0, getBookmarksInList(0).length);
    assertEquals(4, getBookmarksInList(1).length);

    bookmarksApi.callbackRouter.onMoved.callListeners(movedBookmark.id, {
      index: 0,
      parentId: folders[1]!.children![2]!.id,  // Moving to child folder.
      oldParentId: folders[1]!.id,             // Moving from other bookmarks.
      oldIndex: 0,
    });
    flush();

    // Moved bookmark is now in active folder, should move from secondary
    // to primary list.
    assertEquals(1, getBookmarksInList(0).length);
    assertEquals(3, getBookmarksInList(1).length);
  });

  test('RemovesBookmark', () => {
    const originalShownBookmarkCount = getBookmarks().length;

    bookmarksApi.callbackRouter.onRemoved.callListeners('3');
    flush();

    const removedBookmark = getBookmarkWithId('3');
    assertTrue(!removedBookmark);

    assertEquals(originalShownBookmarkCount - 1, getBookmarks().length);
  });

  test('SetsCompactDescription', async () => {
    const folder = getBookmarkWithId('5');
    assertTrue(!!folder);

    assertEquals(
        '(1)', powerBookmarksList.getBookmarkDescriptionForTests(folder));
  });

  test('SetsExpandedDescription', () => {
    const viewButton: HTMLElement =
        powerBookmarksList.shadowRoot!.querySelector('#viewButton')!;
    viewButton.click();

    const folder = getBookmarkWithId('4');
    assertTrue(!!folder);

    assertEquals(
        'child', powerBookmarksList.getBookmarkDescriptionForTests(folder));
  });

  test('SetsExpandedSearchResultDescription', async () => {
    const viewButton =
        powerBookmarksList.shadowRoot!.querySelector<HTMLElement>(
            '#viewButton')!;
    viewButton.click();

    await performSearch('child bookmark');

    const folder = getBookmarkWithId('4');
    assertTrue(!!folder);

    assertEquals(
        'child - All Bookmarks',
        powerBookmarksList.getBookmarkDescriptionForTests(folder));
  });

  test('RenamesBookmark', async () => {
    const renamedBookmarkId = '4';
    powerBookmarksList.setRenamingIdForTests(renamedBookmarkId);

    await flushTasks();

    const rowElement =
        powerBookmarksList.shadowRoot!.querySelector<PowerBookmarkRowElement>(
            `#bookmark-${renamedBookmarkId}`);
    assertTrue(!!rowElement);
    let input =
        rowElement.shadowRoot!.querySelector<CrInputElement>('cr-input');
    assertTrue(!!input);
    const newName = 'foo';
    input.value = newName;
    input.inputElement.dispatchEvent(new Event('change'));

    await flushTasks();

    // Committing a new input value should rename the bookmark and remove the
    // input.
    assertEquals(1, bookmarksApi.getCallCount('renameBookmark'));
    assertEquals(
        renamedBookmarkId, bookmarksApi.getArgs('renameBookmark')[0][0]);
    assertEquals(newName, bookmarksApi.getArgs('renameBookmark')[0][1]);
    input = rowElement.shadowRoot!.querySelector<CrInputElement>('cr-input');
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
        rowElement.shadowRoot!.querySelector<CrInputElement>('cr-input');
    assertTrue(!!input);
    input.inputElement.blur();

    await flushTasks();

    // Blurring the input should remove it.
    input = rowElement.shadowRoot!.querySelector<CrInputElement>('cr-input');
    assertFalse(!!input);
  });

  test('ShowsFolderImages', () => {
    const viewButton: HTMLElement =
        powerBookmarksList.shadowRoot!.querySelector('#viewButton')!;
    viewButton.click();

    flush();

    const bookmarksBarFolderElement = getCrUrlListItemElementWithId('1');
    assertTrue(!!bookmarksBarFolderElement);
    assertEquals(0, bookmarksBarFolderElement.imageUrls.length);

    const childFolderElement = getCrUrlListItemElementWithId('5');
    assertTrue(!!childFolderElement);
    assertNotEquals(0, childFolderElement.imageUrls.length);
  });

  test('DeletesSelectedBookmarks', () => {
    const editButton: HTMLElement =
        powerBookmarksList.shadowRoot!.querySelector('#editButton')!;
    editButton.click();

    flush();

    const bookmarkElement3 = getCrUrlListItemElementWithId('3');
    assertTrue(!!bookmarkElement3);
    bookmarkElement3.click();
    const bookmarkElement5 = getCrUrlListItemElementWithId('5');
    assertTrue(!!bookmarkElement5);
    bookmarkElement5.click();

    flush();

    const deleteButton: HTMLElement =
        powerBookmarksList.shadowRoot!.querySelector('#deleteButton')!;
    deleteButton.click();

    flush();

    assertEquals(1, bookmarksApi.getCallCount('deleteBookmarks'));
    assertEquals('3', bookmarksApi.getArgs('deleteBookmarks')[0][0]);
    assertEquals('5', bookmarksApi.getArgs('deleteBookmarks')[0][1]);
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
    assertEquals(loadTimeData.getString('emptyBody'), topLevelEmptyState.body);

    // Has bookmarks.
    assertFalse(isHidden(search));
    assertTrue(isHidden(labels));
    assertFalse(isHidden(heading));
    assertTrue(isHidden(folderEmptyState));
    assertFalse(isHidden(bookmarksList));
    assertTrue(isHidden(topLevelEmptyState));
    assertFalse(isHidden(footer));

    // Opening an empty folder.
    await openBookmark('1');

    assertFalse(isHidden(search));
    assertTrue(isHidden(labels));
    assertFalse(isHidden(heading));
    assertFalse(isHidden(folderEmptyState));
    assertTrue(isHidden(bookmarksList));
    assertTrue(isHidden(topLevelEmptyState));
    assertFalse(isHidden(footer));

    // A search with no results.
    const searchField =
        powerBookmarksList.shadowRoot!.querySelector('cr-toolbar-search-field');
    assertTrue(!!searchField);
    searchField.$.searchInput.value = 'abcdef';
    searchField.onSearchTermSearch();
    await flushTasks();
    assertEquals(
        loadTimeData.getString('emptyTitleSearch'), topLevelEmptyState.heading);
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
      },
    };
    shoppingListApi.getCallbackRouterRemote().priceTrackedForBookmark(
        newProduct);
    await flushTasks();
    assertFalse(isHidden(labels));
  });
});
