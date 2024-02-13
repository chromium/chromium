// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import type {BookmarkData, BookmarkProxy} from 'chrome://welcome/shared/bookmark_proxy.js';

export class TestBookmarkProxy extends TestBrowserProxy implements
    BookmarkProxy {
  private fakeBookmarkId_: number = 1;

  constructor() {
    super([
      'addBookmark',
      'isBookmarkBarShown',
      'removeBookmark',
      'toggleBookmarkBar',
    ]);
  }

  addBookmark(data: BookmarkData) {
    this.methodCalled('addBookmark', data);
    return Promise.resolve({
      id: (this.fakeBookmarkId_++).toString(),
      title: '',
    });
  }

  isBookmarkBarShown() {
    this.methodCalled('isBookmarkBarShown');

    // TODO(hcarmona): make changeable to test both true/false cases.
    return Promise.resolve(true);
  }

  removeBookmark(id: string) {
    this.methodCalled('removeBookmark', id);
  }

  toggleBookmarkBar(show: boolean) {
    this.methodCalled('toggleBookmarkBar', show);
  }
}
