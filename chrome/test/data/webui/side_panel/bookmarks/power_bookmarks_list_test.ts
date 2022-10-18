// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://read-later.top-chrome/bookmarks/power_bookmarks_list.js';

import {BookmarksApiProxyImpl} from 'chrome://read-later.top-chrome/bookmarks/bookmarks_api_proxy.js';
import {ShoppingListApiProxyImpl} from 'chrome://read-later.top-chrome/bookmarks/commerce/shopping_list_api_proxy.js';
import {PowerBookmarksListElement} from 'chrome://read-later.top-chrome/bookmarks/power_bookmarks_list.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';

import {TestShoppingListApiProxy} from './commerce/test_shopping_list_api_proxy.js';
import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelPowerBookmarksListTest', () => {
  let powerBookmarksList: PowerBookmarksListElement;
  let bookmarksApi: TestBookmarksApiProxy;
  let shoppingListApi: TestShoppingListApiProxy;

  const topLevelBookmarks: chrome.bookmarks.BookmarkTreeNode[] = [
    {
      id: '1',
      parentId: '0',
      title: 'Child bookmark',
      url: 'http://child/bookmark/',
    },
    {
      id: '2',
      parentId: '0',
      title: 'Child folder',
      children: [
        {
          id: '5',
          parentId: '4',
          title: 'Nested bookmark',
          url: 'http://nested/bookmark/',
        },
      ],
    },
  ];

  function getBookmarkElements(root: HTMLElement): HTMLElement[] {
    return Array.from(root.shadowRoot!.querySelectorAll('power-bookmark-row'));
  }

  setup(async () => {
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setTopLevelBookmarks(
        JSON.parse(JSON.stringify(topLevelBookmarks)));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    shoppingListApi = new TestShoppingListApiProxy();
    ShoppingListApiProxyImpl.setInstance(shoppingListApi);

    const pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);

    powerBookmarksList = document.createElement('power-bookmarks-list');
    document.body.appendChild(powerBookmarksList);

    await flushTasks();
  });

  test('GetsAndShowsTopLevelBookmarks', () => {
    assertEquals(1, bookmarksApi.getCallCount('getTopLevelBookmarks'));
    assertEquals(
        topLevelBookmarks.length,
        getBookmarkElements(powerBookmarksList).length);
  });
});
