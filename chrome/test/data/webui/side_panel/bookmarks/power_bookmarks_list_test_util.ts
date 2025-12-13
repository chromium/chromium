// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';

import type {BookmarksTreeNode} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import type {PowerBookmarkRowElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmark_row.js';
import type {PowerBookmarksListElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_list.js';
import type {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
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

export function getBookmarks(element: PowerBookmarksListElement) {
  return getBookmarksInList(element, 0).concat(getBookmarksInList(element, 1));
}

export function getBookmarksInList(
    element: PowerBookmarksListElement,
    listIndex: number): BookmarksTreeNode[] {
  const ironList = element.shadowRoot!.querySelector<IronListElement>(
      `#shownBookmarksIronList${listIndex}`);
  if (!ironList || !ironList.items) {
    return [];
  }
  return ironList.items;
}

export function getBookmarkWithId(
    element: PowerBookmarksListElement, id: string): BookmarksTreeNode|
    undefined {
  return getBookmarks(element).find(bookmark => bookmark.id === id);
}

export function getPowerBookmarksRowElement(
    element: PowerBookmarksListElement|PowerBookmarkRowElement,
    id: string): PowerBookmarkRowElement|undefined {
  return element.shadowRoot!.querySelector<PowerBookmarkRowElement>(
             `#bookmark-${id}`) ||
      undefined;
}

export async function initializeUi(bookmarksApi: TestBookmarksApiProxy):
    Promise<PowerBookmarksListElement> {
  const element = document.createElement('power-bookmarks-list');

  // Ensure the PowerBookmarksListElement is given a fixed height to expand
  // to.
  const parentElement = document.createElement('div');
  parentElement.style.height = '500px';
  parentElement.appendChild(element);
  document.body.appendChild(parentElement);

  await bookmarksApi.whenCalled('getAllBookmarks');
  await waitAfterNextRender(element);
  flush();
  return element;
}
