// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {Note, NoteOverview, UserNotesPageCallbackRouter} from 'chrome://user-notes-side-panel.top-chrome/user_notes.mojom-webui.js';
import {UserNotesApiProxy} from 'chrome://user-notes-side-panel.top-chrome/user_notes_api_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestUserNotesApiProxy extends TestBrowserProxy implements
    UserNotesApiProxy {
  private callbackRouter_: UserNotesPageCallbackRouter =
      new UserNotesPageCallbackRouter();
  private callbackRouterRemote_ =
      this.callbackRouter_.$.bindNewPipeAndPassRemote();
  private notes_: Note[];
  private overviews_: NoteOverview[];

  constructor() {
    super([
      'deleteNote',
      'getNotesForCurrentTab',
      'getNoteOverviews',
      'newNoteFinished',
      'noteOverviewSelected',
      'setSortOrder',
      'showUi',
      'updateNote',
    ]);

    this.notes_ = [];
    this.overviews_ = [];
  }

  deleteNote(guid: string) {
    this.methodCalled('deleteNote', guid);
    return Promise.resolve({success: true});
  }

  getNotesForCurrentTab() {
    this.methodCalled('getNotesForCurrentTab');
    return Promise.resolve({notes: this.notes_.slice()});
  }

  getNoteOverviews(userInput: string) {
    this.methodCalled('getNoteOverviews', userInput);
    return Promise.resolve({overviews: this.overviews_.slice()});
  }

  newNoteFinished(text: string) {
    this.methodCalled('newNoteFinished', text);
    return Promise.resolve({success: true});
  }

  noteOverviewSelected(url: Url, clickModifiers: ClickModifiers) {
    this.methodCalled('noteOverviewSelected', url, clickModifiers);
  }

  setSortOrder(sortByNewest: boolean) {
    this.methodCalled('setSortOrder', sortByNewest);
  }

  showUi() {
    this.methodCalled('showUi');
  }

  updateNote(guid: string, text: string) {
    this.methodCalled('updateNote', guid, text);
    return Promise.resolve({success: true});
  }

  getCallbackRouter() {
    return this.callbackRouter_;
  }

  getCallbackRouterRemote() {
    return this.callbackRouterRemote_;
  }

  setNotes(notes: Note[]) {
    this.notes_ = notes;
  }

  setNoteOverviews(overviews: NoteOverview[]) {
    this.overviews_ = overviews;
  }
}