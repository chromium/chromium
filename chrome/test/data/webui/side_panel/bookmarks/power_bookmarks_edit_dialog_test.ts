// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_edit_dialog.js';

import {BookmarksApiProxyImpl} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import type {PowerBookmarksEditDialogElement} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_edit_dialog.js';
import {PowerBookmarksService} from 'chrome://bookmarks-side-panel.top-chrome/power_bookmarks_service.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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
        {
          id: '6',
          parentId: '2',
          title: 'Child folder 2',
          dateAdded: 1,
          children: [],
        },
      ],
    },
  ];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    bookmarksApi = new TestBookmarksApiProxy();
    bookmarksApi.setFolders(structuredClone(folders));
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
        [],
        topLevelBookmarks,
        [topLevelBookmarks[0]!],
        false,
    );

    const ironList =
        powerBookmarksEditDialog.shadowRoot!.querySelector('iron-list');
    const rows = ironList!.items!;
    // Shows folders apart from itself/descendants
    assertEquals(rows.length, 1);
  });

  test('ShowsActiveFolderName', () => {
    const topLevelBookmarks = service.getTopLevelBookmarks();
    powerBookmarksEditDialog.showDialog(
        [],
        topLevelBookmarks,
        [topLevelBookmarks[0]!],
        false,
    );

    const titleElement =
        powerBookmarksEditDialog.shadowRoot!.querySelector('h2');
    assertEquals(
        titleElement!.textContent!.includes(
            loadTimeData.getString('allBookmarks')),
        true);
  });

  test('SavesChanges', async () => {
    let saveCount = 0;
    let savedName;
    let savedUrl;
    let savedParent;
    let savedNewFolderCount = 0;
    powerBookmarksEditDialog.addEventListener('save', ((e: CustomEvent) => {
                                                        saveCount++;
                                                        savedName =
                                                            e.detail.name;
                                                        savedUrl = e.detail.url;
                                                        savedParent =
                                                            e.detail.folderId;
                                                        savedNewFolderCount =
                                                            e.detail.newFolders
                                                                .length;
                                                      }) as EventListener);

    const topLevelBookmarks = service.getTopLevelBookmarks();
    powerBookmarksEditDialog.showDialog(
        [],
        topLevelBookmarks,
        [topLevelBookmarks[3]!],
        false,
    );

    const newFolderButton: HTMLElement =
        powerBookmarksEditDialog.shadowRoot!.querySelector('#newFolderButton')!;
    newFolderButton.click();

    const nameInput: CrInputElement =
        powerBookmarksEditDialog.shadowRoot!.querySelector('#nameInput')!;
    nameInput.inputElement.value = 'Modified value';
    nameInput.inputElement.dispatchEvent(
        new CustomEvent('input', {composed: true, bubbles: true}));
    await eventToPromise('value-changed', nameInput);

    const saveButton: HTMLElement =
        powerBookmarksEditDialog.shadowRoot!.querySelector('.action-button')!;
    saveButton.click();

    assertEquals(saveCount, 1);
    assertEquals(savedName, 'Modified value');
    assertEquals(savedUrl, 'http://child/bookmark/1/');
    // Adding a new folder should automatically select that folder.
    assertEquals(savedParent, 'tmp_new_folder_0');
    assertEquals(savedNewFolderCount, 1);
  });

  test('DoesNotSaveInvalidUrls', async () => {
    let saveCount = 0;
    powerBookmarksEditDialog.addEventListener('save', (() => {
                                                        saveCount++;
                                                      }) as EventListener);

    const topLevelBookmarks = service.getTopLevelBookmarks();
    powerBookmarksEditDialog.showDialog(
        [],
        topLevelBookmarks,
        [topLevelBookmarks[3]!],
        false,
    );

    const newFolderButton: HTMLElement =
        powerBookmarksEditDialog.shadowRoot!.querySelector('#newFolderButton')!;
    newFolderButton.click();

    const urlInput: CrInputElement =
        powerBookmarksEditDialog.shadowRoot!.querySelector('#urlInput')!;
    urlInput.inputElement.value = 'notavalidurl.2';
    urlInput.inputElement.dispatchEvent(
        new CustomEvent('input', {composed: true, bubbles: true}));
    await eventToPromise('value-changed', urlInput);

    const saveButton: HTMLElement =
        powerBookmarksEditDialog.shadowRoot!.querySelector('.action-button')!;
    saveButton.click();

    // Wait for the urlInput to update for validation, and wait one more cycle
    // to ensure we catch the save event if it occurs.
    await urlInput.updateComplete;
    await new Promise(resolve => setTimeout(resolve, 1));

    assertEquals(saveCount, 0);
  });
});
