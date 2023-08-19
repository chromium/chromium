// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://user-notes-side-panel.top-chrome/user_note_overviews_list.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {UserNoteOverviewRowElement} from 'chrome://user-notes-side-panel.top-chrome/user_note_overview_row.js';
import {UserNoteOverviewsListElement} from 'chrome://user-notes-side-panel.top-chrome/user_note_overviews_list.js';
import {NoteOverview} from 'chrome://user-notes-side-panel.top-chrome/user_notes.mojom-webui.js';
import {UserNotesApiProxyImpl} from 'chrome://user-notes-side-panel.top-chrome/user_notes_api_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestUserNotesApiProxy} from './test_user_notes_api_proxy.js';

suite('UserNoteOverviewsListTest', () => {
  let userNoteOverviewsList: UserNoteOverviewsListElement;
  let testProxy: TestUserNotesApiProxy;

  const noteOverviews: NoteOverview[] = [
    {
      url: {url: 'www.google.com'},
      title: 'first title',
      text: 'sample note text',
      numNotes: 5,
      isCurrentTab: true,
      lastModificationTime: {internalValue: 50n},
    },
    {
      url: {url: 'www.foo.com'},
      title: 'second note overview title',
      text: 'sample second note text',
      numNotes: 1,
      isCurrentTab: false,
      lastModificationTime: {internalValue: 20n},
    },
  ];

  function queryNoteOverviews(): NodeListOf<UserNoteOverviewRowElement> {
    return userNoteOverviewsList.shadowRoot!.querySelectorAll(
        'user-note-overview-row');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestUserNotesApiProxy();
    UserNotesApiProxyImpl.setInstance(testProxy);
    testProxy.setNoteOverviews(noteOverviews);
    loadTimeData.resetForTesting({
      currentTab: 'Current Tab',
      allNotes: 'All Notes',
    });

    userNoteOverviewsList = document.createElement('user-note-overviews-list');
    userNoteOverviewsList.overviews = noteOverviews;
    document.body.appendChild(userNoteOverviewsList);
    await flushTasks();
  });

  test('all overviews shown', () => {
    const noteOverviewElements = queryNoteOverviews();
    assertEquals(noteOverviewElements.length, 2);
    for (let i = 0; i < noteOverviews.length; i++) {
      assertEquals(
          noteOverviewElements[i]!.shadowRoot!
              .querySelector('cr-url-list-item')!.title.trim(),
          noteOverviews[i]!.title);
    }

    test('add a note context menu option', async () => {
      const noteOverviewElements = queryNoteOverviews();
      assertEquals(noteOverviewElements.length, 2);
      const overviewElement = noteOverviewElements[0]!;
      const contextMenuElement = overviewElement.shadowRoot!.querySelector(
          'user-note-overview-row-menu')!;
      const overviewMenuButton = contextMenuElement.shadowRoot!.querySelector(
                                     '#menuButton') as HTMLButtonElement;
      overviewMenuButton.click();
      const overviewMenu = contextMenuElement.$.menu;
      // Click add a note button.
      const addNoteButton = overviewMenu.querySelectorAll(
                                '.dropdown-item')[0]! as HTMLButtonElement;
      addNoteButton.click();
      const [url, _clickModifiers] =
          await testProxy.whenCalled('noteOverviewSelected');
      assertEquals(url, overviewElement.overview.url);
    });

    test('open in new tab context menu option', async () => {
      const noteOverviewElements = queryNoteOverviews();
      assertEquals(noteOverviewElements.length, 2);
      const overviewElement = noteOverviewElements[0]!;
      const contextMenuElement = overviewElement.shadowRoot!.querySelector(
          'user-note-overview-row-menu')!;
      const overviewMenuButton = contextMenuElement.shadowRoot!.querySelector(
                                     '#menuButton') as HTMLButtonElement;
      overviewMenuButton.click();
      const overviewMenu = contextMenuElement.$.menu;
      // Click open in new tab button.
      const openInNewTabButton = overviewMenu.querySelectorAll(
                                     '.dropdown-item')[1]! as HTMLButtonElement;
      openInNewTabButton.click();
      const url = await testProxy.whenCalled('openInNewTab');
      assertEquals(url, overviewElement.overview.url);
    });

    test('delete context menu option', async () => {
      const noteOverviewElements = queryNoteOverviews();
      assertEquals(noteOverviewElements.length, 2);
      const overviewElement = noteOverviewElements[0]!;
      const contextMenuElement = overviewElement.shadowRoot!.querySelector(
          'user-note-overview-row-menu')!;
      const overviewMenuButton = contextMenuElement.shadowRoot!.querySelector(
                                     '#menuButton') as HTMLButtonElement;
      overviewMenuButton.click();
      const overviewMenu = contextMenuElement.$.menu;
      // Click delete button.
      const deleteButton = overviewMenu.querySelectorAll(
                               '.dropdown-item')[4]! as HTMLButtonElement;
      deleteButton.click();
      const url = await testProxy.whenCalled('deleteNotesForUrl');
      assertEquals(url, overviewElement.overview.url);
    });
  });

  test('headers and separator shown', () => {
    const noteOverviewElements = queryNoteOverviews();
    assertEquals(noteOverviewElements.length, 2);
    const headers =
        userNoteOverviewsList.shadowRoot!.querySelectorAll('sp-heading');
    assertEquals(headers.length, 2);
    const separator = userNoteOverviewsList.shadowRoot!.querySelectorAll('.hr');
    assertEquals(separator.length, 1);
  });
});
