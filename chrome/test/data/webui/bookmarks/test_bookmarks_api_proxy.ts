// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarksApiProxy, Query} from 'chrome://bookmarks/bookmarks.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBookmarksApiProxy extends TestBrowserProxy implements
    BookmarksApiProxy {
  private searchResponse_: chrome.bookmarks.BookmarkTreeNode[] = [];
  private getTreeResponse_: chrome.bookmarks.BookmarkTreeNode[] = [];

  constructor() {
    super([
      'create',
      'getTree',
      'search',
      'update',
    ]);
  }

  getTree() {
    this.methodCalled('getTree');
    return Promise.resolve(this.getTreeResponse_);
  }

  setGetTree(nodes: chrome.bookmarks.BookmarkTreeNode[]) {
    this.getTreeResponse_ = nodes;
  }

  search(query: Query) {
    this.methodCalled('search', query);
    return Promise.resolve(this.searchResponse_);
  }

  setSearchResponse(response: chrome.bookmarks.BookmarkTreeNode[]) {
    this.searchResponse_ = response;
  }

  update(id: string, changes: {title?: string, url?: string}) {
    this.methodCalled('update', [id, changes]);
    return Promise.resolve({id: '', title: ''});
  }

  create(bookmark: chrome.bookmarks.CreateDetails) {
    this.methodCalled('create', bookmark);
    return Promise.resolve({id: '', title: ''});
  }
}
