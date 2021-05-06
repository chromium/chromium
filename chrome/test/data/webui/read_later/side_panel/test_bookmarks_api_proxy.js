// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BookmarksApiProxy} from 'chrome://read-later.top-chrome/side_panel/bookmarks_api_proxy.js';

import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

/** @implements {BookmarksApiProxy} */
export class TestBookmarksApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getFolders',
    ]);

    /** @private {!Array<!chrome.bookmarks.BookmarkTreeNode>} */
    this.folders_ = [];
  }

  getFolders() {
    this.methodCalled('getFolders');
    return Promise.resolve(this.folders_);
  }

  /** @param {!Array<!chrome.bookmarks.BookmarkTreeNode>} folders */
  setFolders(folders) {
    this.folders_ = folders;
  }
}