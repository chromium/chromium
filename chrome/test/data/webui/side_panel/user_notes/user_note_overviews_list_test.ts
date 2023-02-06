// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://user-notes-side-panel.top-chrome/user_note_overviews_list.js';

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

  function queryNoteOverviews() {
    return userNoteOverviewsList.shadowRoot!.querySelectorAll(
        'user-note-overview-row');
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testProxy = new TestUserNotesApiProxy();
    UserNotesApiProxyImpl.setInstance(testProxy);
    testProxy.setNoteOverviews(noteOverviews);

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
          noteOverviewElements[i]!.shadowRoot!.querySelector('#title')!
              .textContent!.trim(),
          noteOverviews[i]!.title);
    }
  });
});