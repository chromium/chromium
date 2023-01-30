// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://user-notes-side-panel.top-chrome/app.js';

import {UserNotesAppElement} from 'chrome://user-notes-side-panel.top-chrome/app.js';
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
      lastModificationTime: {internalValue: 5n},
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

  function queryNotes() {
    return userNotesApp.shadowRoot!.querySelectorAll('user-note');
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

  test('all entries shown', () => {
    const notesElements = queryNotes();
    assertEquals(notesElements.length, 3);
    for (let i = 0; i < notes.length; i++) {
      assertEquals(
          'false',
          notesElements[i]!.$.noteContent.getAttribute('contenteditable'));
      assertEquals(notesElements[i]!.$.noteContent.textContent, notes[i]!.text);
    }
    const entryNote = notesElements[(notesElements.length - 1)]!;
    assertEquals(
        'plaintext-only',
        entryNote.$.noteContent.getAttribute('contenteditable'));
    assertEquals(entryNote.$.noteContent.textContent, '');
  });

  test('note entry adding a new note', async () => {
    const notesElements = queryNotes();
    const entryNote = notesElements[2]!;
    const sampleNoteContent = 'sample note content';
    entryNote.$.noteContent.textContent = sampleNoteContent;
    assertEquals(
        'plaintext-only',
        entryNote.$.noteContent.getAttribute('contenteditable'));
    entryNote.$.noteContent.focus();
    const notesAddButton =
        entryNote.shadowRoot!.querySelector('#addButton')! as HTMLButtonElement;
    notesAddButton.click();
    const text = await testProxy.whenCalled('newNoteFinished');
    assertEquals(text, sampleNoteContent);
    assertEquals('', entryNote.$.noteContent.textContent);
  });

  test('edit note and save changes', async () => {
    const notesElements = queryNotes();
    const note = notesElements[0]!;
    const originalContent = note.$.noteContent.textContent;
    assertEquals('false', note.$.noteContent.getAttribute('contenteditable'));
    const contextMenuElement =
        note.shadowRoot!.querySelector('user-note-menu')!;
    const noteMenuButton = contextMenuElement.shadowRoot!.querySelector(
                               '#menuButton') as HTMLButtonElement;
    noteMenuButton.click();
    const noteMenu = contextMenuElement.$.menu;
    // Click edit button.
    const editButton =
        noteMenu.querySelectorAll('.dropdown-item')[0]! as HTMLButtonElement;
    editButton.click();
    assertEquals(
        'plaintext-only', note.$.noteContent.getAttribute('contenteditable'));
    assertEquals(originalContent, note.$.noteContent.textContent);
    // Update content.
    const newContent = 'different content';
    note.$.noteContent.textContent = newContent;
    // Click add button.
    note.$.noteContent.focus();
    await flushTasks();
    const noteAddButton =
        note.shadowRoot!.querySelector('#addButton')! as HTMLButtonElement;
    noteAddButton.click();
    // Verify changes to content have been saved and the note is no longer in
    // the editing state.
    const [, text] = await testProxy.whenCalled('updateNote');
    assertEquals(text, newContent);
    assertEquals('false', note.$.noteContent.getAttribute('contenteditable'));
    assertEquals(newContent, note.$.noteContent.textContent);
  });

  test('cancel while editing note', async () => {
    const notesElements = queryNotes();
    const note = notesElements[0]!;
    const originalContent = note.$.noteContent.textContent;
    assertEquals('false', note.$.noteContent.getAttribute('contenteditable'));
    const contextMenuElement =
        note.shadowRoot!.querySelector('user-note-menu')!;
    const noteMenuButton = contextMenuElement.shadowRoot!.querySelector(
                               '#menuButton') as HTMLButtonElement;
    noteMenuButton.click();
    const noteMenu = contextMenuElement.$.menu;
    // Click edit button.
    const editButton =
        noteMenu.querySelectorAll('.dropdown-item')[0]! as HTMLButtonElement;
    editButton.click();
    assertEquals(
        'plaintext-only', note.$.noteContent.getAttribute('contenteditable'));
    assertEquals(originalContent, note.$.noteContent.textContent);
    // Update content.
    note.$.noteContent.textContent = 'different content';
    // Click cancel button.
    note.$.noteContent.focus();
    await flushTasks();
    const notesCancelButton =
        note.shadowRoot!.querySelector('#cancelButton')! as HTMLButtonElement;
    notesCancelButton.click();
    // Verify changes to content have been undone and the note is no longer in
    // the editing state.
    assertEquals('false', note.$.noteContent.getAttribute('contenteditable'));
    assertEquals(originalContent, note.$.noteContent.textContent);
  });

  test('delete note', async () => {
    const notesElements = queryNotes();
    assertEquals(notesElements.length, 3);
    const note = notesElements[0]!;
    assertEquals('false', note.$.noteContent.getAttribute('contenteditable'));
    const contextMenuElement =
        note.shadowRoot!.querySelector('user-note-menu')!;
    const noteMenuButton = contextMenuElement.shadowRoot!.querySelector(
                               '#menuButton') as HTMLButtonElement;
    noteMenuButton.click();
    const noteMenu = contextMenuElement.$.menu;
    // Click delete button.
    const deleteButton =
        noteMenu.querySelectorAll('.dropdown-item')[1]! as HTMLButtonElement;
    deleteButton.click();
    await testProxy.whenCalled('deleteNote');
  });

  test('refresh notes when url changes', async () => {
    let notesElements = queryNotes();
    assertEquals(notesElements.length, 3);
    testProxy.setNotes([]);
    testProxy.getCallbackRouterRemote().currentTabUrlChanged();
    await flushTasks();
    notesElements = queryNotes();
    assertEquals(notesElements.length, 1);
  });

  test('toggle from page notes to note overview', async () => {
    // Verify notes are found and note overviews are not.
    const notesListElement =
        userNotesApp.shadowRoot!.querySelector('user-note');
    assertEquals(true, isVisible(notesListElement));
    let noteOverviewElement =
        userNotesApp.shadowRoot!.querySelector('user-note-overviews-list')!;
    assertEquals(false, isVisible(noteOverviewElement));
    // Click button to navigate to note overviews.
    const allNotesButton = userNotesApp.shadowRoot!.getElementById(
                               'allNotesButton') as HTMLButtonElement;
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
    const allNotesButton = userNotesApp.shadowRoot!.getElementById(
                               'allNotesButton') as HTMLButtonElement;
    allNotesButton.click();
    await flushTasks();
    // Verify note overviews are found and the individual notes list is not.
    let notesListElement = userNotesApp.shadowRoot!.querySelector('user-note');
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
    testProxy.getCallbackRouterRemote().currentTabUrlChanged();
    await flushTasks();
    // Verify notes are found and note overviews are not.
    notesListElement = userNotesApp.shadowRoot!.querySelector('user-note');
    assertEquals(true, isVisible(notesListElement));
    noteOverviewsElement =
        userNotesApp.shadowRoot!.querySelector('user-note-overviews-list');
    assertEquals(false, isVisible(noteOverviewsElement));
  });
});