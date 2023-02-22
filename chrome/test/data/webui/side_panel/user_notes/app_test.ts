// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://user-notes-side-panel.top-chrome/app.js';

import {UserNotesAppElement} from 'chrome://user-notes-side-panel.top-chrome/app.js';
import {UserNoteElement} from 'chrome://user-notes-side-panel.top-chrome/user_note.js';
import {Note, NoteOverview} from 'chrome://user-notes-side-panel.top-chrome/user_notes.mojom-webui.js';
import {UserNotesApiProxyImpl} from 'chrome://user-notes-side-panel.top-chrome/user_notes_api_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestUserNotesApiProxy} from './test_user_notes_api_proxy.js';

suite('UserNotesAppTest', () => {
  let userNotesApp: UserNotesAppElement;
  let testProxy: TestUserNotesApiProxy;

  const notes: Note[] = [
    {
      guid: '1234',
      url: {url: 'www.google.com'},
      lastModificationTime: {internalValue: 5n},
      lastModificationTimeText: '5:00 AM - Oct 5',
      text: 'sample note text',
    },
    {
      guid: '1235',
      url: {url: 'www.google.com'},
      lastModificationTime: {internalValue: 20n},
      lastModificationTimeText: '7:00 AM - Oct 6',
      text: 'sample second note text',
    },
  ];

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

  function queryNotes(): NodeListOf<UserNoteElement> {
    return userNotesApp.shadowRoot!.querySelector('user-notes-list')!
        .shadowRoot!.querySelectorAll('user-note');
  }

  function queryNoteOverviews() {
    return userNotesApp.shadowRoot!.querySelector('user-note-overviews-list')!
        .shadowRoot!.querySelectorAll('user-note-overview-row');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestUserNotesApiProxy();
    UserNotesApiProxyImpl.setInstance(testProxy);
    testProxy.setNotes(notes);
    testProxy.setNoteOverviews(noteOverviews);

    userNotesApp = document.createElement('user-notes-app');
    document.body.appendChild(userNotesApp);
    await flushTasks();
  });

  test('refresh notes when url changes', async () => {
    let notesElements = queryNotes();
    assertEquals(notesElements.length, 3);
    testProxy.setNotes([]);
    testProxy.getCallbackRouterRemote().currentTabUrlChanged(false);
    await flushTasks();
    notesElements = queryNotes();
    assertEquals(notesElements.length, 1);
  });

  test('toggle from page notes to note overview', async () => {
    // Verify notes are found and note overviews are not.
    const notesListElement =
        userNotesApp.shadowRoot!.querySelector('user-notes-list')!.shadowRoot!
            .querySelector('user-note');
    assertEquals(true, isVisible(notesListElement));
    let noteOverviewElement =
        userNotesApp.shadowRoot!.querySelector('user-note-overviews-list')!;
    assertEquals(false, isVisible(noteOverviewElement));
    // Click button to navigate to note overviews.
    const allNotesButton =
        userNotesApp.shadowRoot!.querySelector('user-notes-list')!.shadowRoot!
            .getElementById('allNotesButton') as HTMLButtonElement;
    allNotesButton.click();
    await flushTasks();
    // Verify note overviews are found and the individual notes list is not.
    assertEquals(false, isVisible(notesListElement));
    noteOverviewElement =
        userNotesApp.shadowRoot!.querySelector('user-note-overviews-list')!;
    assertEquals(true, isVisible(noteOverviewElement));
    const noteOverviewElements = queryNoteOverviews();
    assertEquals(2, noteOverviewElements.length);
  });

  test('toggle from note overview to page notes', async () => {
    // Click button to navigate to note overviews.
    const allNotesButton =
        userNotesApp.shadowRoot!.querySelector('user-notes-list')!.shadowRoot!
            .getElementById('allNotesButton') as HTMLButtonElement;
    allNotesButton.click();
    await flushTasks();
    // Verify note overviews are found and the individual notes list is not.
    let notesListElement =
        userNotesApp.shadowRoot!.querySelector('user-notes-list');
    assertEquals(false, isVisible(notesListElement));
    let noteOverviewsElement =
        userNotesApp.shadowRoot!.querySelector('user-note-overviews-list');
    assertEquals(true, isVisible(noteOverviewsElement));
    // Click on a note overview.
    const noteOverview = queryNoteOverviews()[1]!;
    noteOverview.shadowRoot!.querySelector('cr-url-list-item')!.click();
    const [url, _clickModifiers] =
        await testProxy.whenCalled('noteOverviewSelected');
    assertEquals(noteOverview.overview.url, url);
    testProxy.getCallbackRouterRemote().currentTabUrlChanged(false);
    await flushTasks();
    // Verify notes are found and note overviews are not.
    notesListElement =
        userNotesApp.shadowRoot!.querySelector('user-notes-list');
    assertEquals(true, isVisible(notesListElement));
    noteOverviewsElement =
        userNotesApp.shadowRoot!.querySelector('user-note-overviews-list');
    assertEquals(false, isVisible(noteOverviewsElement));
  });
});