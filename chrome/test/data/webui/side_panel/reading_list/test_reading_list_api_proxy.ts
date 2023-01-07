// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, ReadLaterEntriesByStatus} from 'chrome://read-later.top-chrome/reading_list/reading_list.mojom-webui.js';
import {ReadingListApiProxy} from 'chrome://read-later.top-chrome/reading_list/reading_list_api_proxy.js';
import {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestReadingListApiProxy extends TestBrowserProxy implements
    ReadingListApiProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  private entries_: ReadLaterEntriesByStatus;

  constructor() {
    super([
      'getReadLaterEntries',
      'openURL',
      'updateReadStatus',
      'addCurrentTab',
      'markCurrentTabAsRead',
      'removeEntry',
      'showContextMenuForURL',
      'updateCurrentPageActionButtonState',
      'showUI',
      'closeUI',
    ]);

    this.entries_ = {
      unreadEntries: [],
      readEntries: [],
    };
  }

  getReadLaterEntries() {
    this.methodCalled('getReadLaterEntries');
    return Promise.resolve({entries: this.entries_});
  }

  openURL(url: Url, markAsRead: boolean, clickModifiers: ClickModifiers) {
    this.methodCalled('openURL', [url, markAsRead, clickModifiers]);
  }

  updateReadStatus(url: Url, read: boolean) {
    this.methodCalled('updateReadStatus', [url, read]);
  }

  addCurrentTab() {
    this.methodCalled('addCurrentTab');
  }

  markCurrentTabAsRead() {
    this.methodCalled('markCurrentTabAsRead');
  }

  removeEntry(url: Url) {
    this.methodCalled('removeEntry', url);
  }

  showContextMenuForURL(url: Url, locationX: number, locationY: number) {
    this.methodCalled('showContextMenuForURL', [url, locationX, locationY]);
  }

  updateCurrentPageActionButtonState() {
    this.methodCalled('updateCurrentPageActionButtonState');
  }

  showUI() {
    this.methodCalled('showUI');
  }

  closeUI() {
    this.methodCalled('closeUI');
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  setEntries(entries: ReadLaterEntriesByStatus) {
    this.entries_ = entries;
  }
}
