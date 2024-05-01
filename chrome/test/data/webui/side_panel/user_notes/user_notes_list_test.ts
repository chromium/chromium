// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://user-notes-side-panel.top-chrome/user_notes_list.js';

import type {UserNoteElement} from 'chrome://user-notes-side-panel.top-chrome/user_note.js';
import type {Note} from 'chrome://user-notes-side-panel.top-chrome/user_notes.mojom-webui.js';
import {UserNotesApiProxyImpl} from 'chrome://user-notes-side-panel.top-chrome/user_notes_api_proxy.js';
import type {UserNotesListElement} from 'chrome://user-notes-side-panel.top-chrome/user_notes_list.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestUserNotesApiProxy} from './test_user_notes_api_proxy.js';

suite('UserNotesListTest', () => {
  let userNotesList: UserNotesListElement;
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

  function queryNotes(): NodeListOf<UserNoteElement> {
    return userNotesList.shadowRoot!.querySelectorAll('user-note');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestUserNotesApiProxy();
    UserNotesApiProxyImpl.setInstance(testProxy);
    testProxy.setNotes(notes);

    userNotesList = document.createElement('user-notes-list');
    const listNotes = notes.slice() as Array<Note|null>;
    listNotes.push(null);
    userNotesList.notes = listNotes;
    document.body.appendChild(userNotesList);
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
        entryNote.shadowRoot!.querySelector<HTMLElement>('#addButton');
    assertTrue(!!notesAddButton);
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
    const noteMenuButton =
        contextMenuElement.shadowRoot!.querySelector<HTMLElement>(
            '#menuButton');
    assertTrue(!!noteMenuButton);
    noteMenuButton.click();
    const noteMenu = contextMenuElement.$.menu;
    // Click edit button.
    const editButton = noteMenu.querySelector<HTMLElement>('.dropdown-item');
    assertTrue(!!editButton);
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
        note.shadowRoot!.querySelector<HTMLElement>('#addButton');
    assertTrue(!!noteAddButton);
    noteAddButton.click();
    // Verify changes to content have been saved and the note is no longer in
    // the editing state.
    const [, text] = await testProxy.whenCalled('updateNote');
    assertEquals(text, newContent);
    assertEquals('false', note.$.noteContent.getAttribute('contenteditable'));
    assertEquals(newContent, note.$.noteContent.textContent);
  });

  test('edit note and save changes', async () => {
    const notesElements = queryNotes();
    const note = notesElements[0]!;
    const originalContent = note.$.noteContent.textContent;
    assertEquals('false', note.$.noteContent.getAttribute('contenteditable'));
    const contextMenuElement =
        note.shadowRoot!.querySelector('user-note-menu')!;
    const noteMenuButton =
        contextMenuElement.shadowRoot!.querySelector<HTMLElement>(
            '#menuButton');
    assertTrue(!!noteMenuButton);
    noteMenuButton.click();
    const noteMenu = contextMenuElement.$.menu;
    // Click edit button.
    const editButton = noteMenu.querySelector<HTMLElement>('.dropdown-item');
    assertTrue(!!editButton);
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
        note.shadowRoot!.querySelector<HTMLElement>('#addButton');
    assertTrue(!!noteAddButton);
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
    const noteMenuButton =
        contextMenuElement.shadowRoot!.querySelector<HTMLElement>(
            '#menuButton');
    assertTrue(!!noteMenuButton);
    noteMenuButton.click();
    const noteMenu = contextMenuElement.$.menu;
    // Click edit button.
    const editButton = noteMenu.querySelector<HTMLElement>('.dropdown-item');
    assertTrue(!!editButton);
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
        note.shadowRoot!.querySelector<HTMLElement>('#cancelButton');
    assertTrue(!!notesCancelButton);
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
    const noteMenuButton =
        contextMenuElement.shadowRoot!.querySelector<HTMLElement>(
            '#menuButton');
    assertTrue(!!noteMenuButton);
    noteMenuButton.click();
    const noteMenu = contextMenuElement.$.menu;
    // Click delete button.
    const deleteButton =
        noteMenu.querySelectorAll<HTMLElement>('.dropdown-item')[1];
    assertTrue(!!deleteButton);
    deleteButton.click();
    await testProxy.whenCalled('deleteNote');
  });

  test('sort reverses order', async () => {
    let notesElements = queryNotes();
    assertEquals(notesElements.length, 3);
    // Verify the default sorted order.
    for (let i = 0; i < notes.length; i++) {
      assertEquals(
          'false',
          notesElements[i]!.$.noteContent.getAttribute('contenteditable'));
      assertEquals(notesElements[i]!.$.noteContent.textContent, notes[i]!.text);
    }
    let entryNote = notesElements[(notesElements.length - 1)]!;
    assertEquals(
        'plaintext-only',
        entryNote.$.noteContent.getAttribute('contenteditable'));
    assertEquals(entryNote.$.noteContent.textContent, '');
    // Click sort button.
    const sortButton = userNotesList.shadowRoot!.getElementById('sortButton') as
        HTMLButtonElement;
    sortButton.click();
    // Click sort by newest option.
    const sortMenu = userNotesList.shadowRoot!.getElementById('sortMenu')!;
    const sortNewestButton =
        sortMenu.querySelector<HTMLElement>('.dropdown-item');
    assertTrue(!!sortNewestButton);
    sortNewestButton.click();
    const sortByNewest = await testProxy.whenCalled('setSortOrder');
    testProxy.getCallbackRouterRemote().sortByNewestPrefChanged(sortByNewest);
    await flushTasks();
    // Verify note order.
    notesElements = queryNotes();
    assertEquals(notesElements.length, 3);
    entryNote = notesElements[0]!;
    assertEquals(
        'plaintext-only',
        entryNote.$.noteContent.getAttribute('contenteditable'));
    assertEquals(entryNote.$.noteContent.textContent, '');
    for (let i = 1; i <= notes.length; i++) {
      assertEquals(
          'false',
          notesElements[i]!.$.noteContent.getAttribute('contenteditable'));
      assertEquals(
          notes[notes.length - i]!.text,
          notesElements[i]!.$.noteContent.textContent);
    }
  });

  test('note entry character count exceeded', async () => {
    const notesElements = queryNotes();
    const entryNote = notesElements[2]!;
    // Add content exceeding 176 characters.
    const sampleNoteContent =
        'sample note contentsample note content sample note content sample ' +
        'note content sample note content sample note content sample note ' +
        'content sample note content sample note content sample note content';
    entryNote.$.noteContent.textContent = sampleNoteContent;
    assertEquals(
        'plaintext-only',
        entryNote.$.noteContent.getAttribute('contenteditable'));
    // Trigger input event so that the character count gets updated.
    entryNote.$.noteContent.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    entryNote.$.noteContent.focus();
    await flushTasks();
    const notesAddButton =
        entryNote.shadowRoot!.querySelector<HTMLButtonElement>('#addButton');
    assertTrue(!!notesAddButton);
    assertEquals(true, notesAddButton.disabled);
  });
});
