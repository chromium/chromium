// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BookmarksApiProxy} from 'chrome://read-later.top-chrome/side_panel/bookmarks_api_proxy.js';

import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

class EventDispatcher {
  constructor() {
    this.eventListeners_ = [];
  }

  addListener(callback) {
    this.eventListeners_.push(callback);
  }

  removeListener(callback) {
    this.eventListeners_.splice(this.eventListeners_.indexOf(callback), 1);
  }

  /** @param {...?} var_args */
  dispatchEvent(var_args) {
    this.eventListeners_.forEach((callback) => {
      callback(...arguments);
    });
  }
}

/** @implements {BookmarksApiProxy} */
export class TestBookmarksApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getFolders',
      'openBookmark',
    ]);

    this.callbackRouter = {
      onChanged: new EventDispatcher(),
      onChildrenReordered: new EventDispatcher(),
      onCreated: new EventDispatcher(),
      onMoved: new EventDispatcher(),
      onRemoved: new EventDispatcher(),
    };

    /** @private {!Array<!chrome.bookmarks.BookmarkTreeNode>} */
    this.folders_ = [];
  }

  getFolders() {
    this.methodCalled('getFolders');
    return Promise.resolve(this.folders_);
  }

  /**
   * @param {string} url
   * @param {number} depth
   */
  openBookmark(url, depth) {
    this.methodCalled('openBookmark', url, depth);
  }

  /** @param {!Array<!chrome.bookmarks.BookmarkTreeNode>} folders */
  setFolders(folders) {
    this.folders_ = folders;
  }
}