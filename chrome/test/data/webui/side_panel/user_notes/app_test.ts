// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://user-notes-side-panel.top-chrome/app.js';

import {UserNotesAppElement} from 'chrome://user-notes-side-panel.top-chrome/app.js';
import {Note} from 'chrome://user-notes-side-panel.top-chrome/user_notes.mojom-webui.js';
import {UserNotesApiProxyImpl} from 'chrome://user-notes-side-panel.top-chrome/user_notes_api_proxy.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

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

  function queryNotes() {
    return userNotesApp.shadowRoot!.querySelectorAll('user-note');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestUserNotesApiProxy();
    UserNotesApiProxyImpl.setInstance(testProxy);
    testProxy.setNotes(notes);

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
        'true', entryNote.$.noteContent.getAttribute('contenteditable'));
    assertEquals(entryNote.$.noteContent.textContent, '');
  });

  test('note entry adding a new note', async () => {
    const notesElements = queryNotes();
    const entryNote = notesElements[2]!;
    const sampleNoteContent = 'sample note content';
    entryNote.$.noteContent.textContent = sampleNoteContent;
    assertEquals(
        'true', entryNote.$.noteContent.getAttribute('contenteditable'));
    entryNote.$.noteContent.focus();
    const notesAddButton =
        entryNote.shadowRoot!.querySelector('#addButton')! as HTMLButtonElement;
    notesAddButton.click();
    const text = await testProxy.whenCalled('newNoteFinished');
    assertEquals(text, sampleNoteContent);
    assertEquals('', entryNote.$.noteContent.textContent);
  });
});