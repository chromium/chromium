// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import type {BookmarksTreeNode} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import type {PowerBookmarkRowElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row.js';
import type {PowerBookmarkRowItemElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row_item.js';
import type {PowerBookmarksAppElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import type {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

export function createTestBookmarks(): BookmarksTreeNode[] {
  return [
    {
      id: 'SIDE_PANEL_BOOKMARK_BAR_ID',
      parentId: 'SIDE_PANEL_ROOT_BOOKMARK_ID',
      index: 0,
      title: 'Bookmarks Bar',
      url: null,
      dateAdded: null,
      dateLastUsed: null,
      unmodifiable: false,
      children: [],
    },
    {
      id: 'SIDE_PANEL_OTHER_BOOKMARKS_ID',
      parentId: 'SIDE_PANEL_ROOT_BOOKMARK_ID',
      title: 'Other Bookmarks',
      index: 1,
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
          dateLastUsed: null,
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
          dateLastUsed: null,
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
              unmodifiable: false,
              children: null,
            },
          ],
        },
      ],
    },
  ];
}

export function getBookmarks(app: PowerBookmarksAppElement):
    BookmarksTreeNode[] {
  return getBookmarksInList(app, 0).concat(getBookmarksInList(app, 1));
}

export function getBookmarksInList(
    app: PowerBookmarksAppElement, listIndex: number): BookmarksTreeNode[] {
  const listItems = app.$.bookmarksList.$.list.items;
  const items = listItems.map(item => item.bookmark);
  const elements =
      app.$.bookmarksList.shadowRoot.querySelectorAll('power-bookmark-row');
  const firstSecondaryIndex = Array.from(elements).findIndex(
      el => el.rowHeading === loadTimeData.getString('secondaryFilterHeading'));
  if (listIndex === 0) {
    return firstSecondaryIndex > -1 ? items.slice(0, firstSecondaryIndex) :
                                      items;
  } else {
    return firstSecondaryIndex > -1 ? items.slice(firstSecondaryIndex) : [];
  }
}

export function getBookmarkWithId(
    app: PowerBookmarksAppElement, id: string): BookmarksTreeNode|undefined {
  return getBookmarks(app).find(bookmark => bookmark.id === id);
}

export function getPowerBookmarksRowElement(
    element: PowerBookmarksAppElement, id: string): PowerBookmarkRowElement|
    null {
  return element.$.bookmarksList.shadowRoot
      .querySelector<PowerBookmarkRowElement>(`#bookmark-${id}`);
}

export function getPowerBookmarksRowItemElement(
    element: PowerBookmarksAppElement, id: string): PowerBookmarkRowItemElement|
    null {
  const row = getPowerBookmarksRowElement(element, id);
  if (!row) {
    return null;
  }
  return row.shadowRoot.querySelector('power-bookmark-row-item');
}

export async function initializeAppUi(bookmarksApi: TestBookmarksApiProxy):
    Promise<PowerBookmarksAppElement> {
  const element = document.createElement('power-bookmarks-app');

  const parentElement = document.createElement('div');
  parentElement.style.height = '500px';
  parentElement.appendChild(element);
  document.body.appendChild(parentElement);

  await bookmarksApi.whenCalled('getAllBookmarks');
  await waitAfterNextRender(element.$.bookmarksList);
  flush();
  return element;
}
