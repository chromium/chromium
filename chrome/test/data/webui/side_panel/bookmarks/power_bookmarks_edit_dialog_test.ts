// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_edit_dialog.js';

import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import {PowerBookmarksEditDialogElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_edit_dialog.js';
import {PowerBookmarksService} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_service.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {TestBookmarksApiProxy} from './test_bookmarks_api_proxy.js';
import {TestPowerBookmarksDelegate} from './test_power_bookmarks_delegate.js';

suite('SidePanelPowerBookmarksEditDialogTest', () => {
  let powerBookmarksEditDialog: PowerBookmarksEditDialogElement;
  let bookmarksApi: TestBookmarksApiProxy;
  let delegate: TestPowerBookmarksDelegate;
  let service: PowerBookmarksService;

  const folders: chrome.bookmarks.BookmarkTreeNode[] = [
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

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(JSON.parse(JSON.stringify(folders)));
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    delegate = new TestPowerBookmarksDelegate();
    service = new PowerBookmarksService(delegate);
    service.startListening();

    loadTimeData.overrideValues({
      allBookmarks: 'All Bookmarks',
    });

    powerBookmarksEditDialog =
        document.createElement('power-bookmarks-edit-dialog');
    document.body.appendChild(powerBookmarksEditDialog);

    await delegate.whenCalled('onBookmarksLoaded');
  });

  test('ShowsCorrectRowCount', async () => {
    const topLevelBookmarks = service.getTopLevelBookmarks();
    powerBookmarksEditDialog.showDialog(
        undefined,
        topLevelBookmarks,
        [topLevelBookmarks[0]!],
    );

    const ironList =
        powerBookmarksEditDialog.shadowRoot!.querySelector('iron-list');
    const rows = ironList!.items!;

    assertEquals(rows.length, 1);
  });

  test('ShowsActiveFolderName', () => {
    const topLevelBookmarks = service.getTopLevelBookmarks();
    powerBookmarksEditDialog.showDialog(
        undefined,
        topLevelBookmarks,
        [topLevelBookmarks[0]!],
    );

    const titleElement =
        powerBookmarksEditDialog.shadowRoot!.querySelector('.folder-header');
    assertEquals(
        titleElement!.textContent!.includes(
            loadTimeData.getString('allBookmarks')),
        true);
  });
});
