// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ReadLaterEntriesByStatus} from 'chrome://read-later.top-chrome/reading_list.mojom-webui.js';
import {PageCallbackRouter} from 'chrome://read-later.top-chrome/reading_list.mojom-webui.js';
import type {ReadingListApiProxy} from 'chrome://read-later.top-chrome/reading_list_api_proxy.js';
import type {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestReadingListApiProxy extends TestBrowserProxy implements
    ReadingListApiProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  private entries_: ReadLaterEntriesByStatus;

  constructor() {
    super([
      'getReadLaterEntries',
      'openUrl',
      'updateReadStatus',
      'addCurrentTab',
      'markCurrentTabAsRead',
      'removeEntry',
      'showContextMenuForUrl',
      'updateCurrentPageActionButtonState',
      'showUi',
      'closeUi',
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

  openUrl(url: Url, markAsRead: boolean, clickModifiers: ClickModifiers) {
    this.methodCalled('openUrl', [url, markAsRead, clickModifiers]);
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

  showContextMenuForUrl(url: Url, locationX: number, locationY: number) {
    this.methodCalled('showContextMenuForUrl', [url, locationX, locationY]);
  }

  updateCurrentPageActionButtonState() {
    this.methodCalled('updateCurrentPageActionButtonState');
  }

  showUi() {
    this.methodCalled('showUi');
  }

  closeUi() {
    this.methodCalled('closeUi');
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  setEntries(entries: ReadLaterEntriesByStatus) {
    this.entries_ = entries;
  }
}
