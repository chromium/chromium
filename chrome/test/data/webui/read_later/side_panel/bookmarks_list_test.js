// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReadLaterUI is a Mojo WebUI controller and therefore needs mojo defined to
// finish running its tests.
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {BookmarksApiProxyImpl} from 'chrome://read-later.top-chrome/side_panel/bookmarks_api_proxy.js';
import {BookmarksList} from 'chrome://read-later.top-chrome/side_panel/bookmarks_list.js';

import {assertEquals} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';

suite('SidePanelBookmarksListTest', () => {
  /** @type {!BookmarksList} */
  let bookmarksList;

  /** @type {!TestBookmarksApiProxy} */
  let bookmarksApi;

  /** @type {!Array<!chrome.bookmarks.BookmarkTreeNode>} */
  const folders = [
    {
      id: '0',
      title: 'Bookmarks bar',
      children: [],
    },
    {
      id: '1',
      title: 'Other bookmarks',
      children: [],
    },
    {
      id: '2',
      title: 'Mobile bookmarks',
      children: [],
    },
  ];

  setup(async () => {
    document.body.innerHTML = '';

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(folders);
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    bookmarksList = /** @type {!BookmarksList} */ (
        document.createElement('bookmarks-list'));
    document.body.appendChild(bookmarksList);

    await flushTasks();
  });

  test('GetsAndShowsFolders', () => {
    assertEquals(1, bookmarksApi.getCallCount('getFolders'));
    const folderElements =
        bookmarksList.shadowRoot.querySelectorAll('bookmark-folder');
    assertEquals(folders.length, folderElements.length);
  });
});