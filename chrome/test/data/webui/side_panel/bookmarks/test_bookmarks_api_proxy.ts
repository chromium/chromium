// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActionSource, SortOrder, ViewType} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import type {BookmarksApiProxy} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import type {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBookmarksApiProxy extends TestBrowserProxy implements
    BookmarksApiProxy {
  private folders_: chrome.bookmarks.BookmarkTreeNode[] = [];
  callbackRouter: {
    onChanged: FakeChromeEvent,
    onChildrenReordered: FakeChromeEvent,
    onCreated: FakeChromeEvent,
    onMoved: FakeChromeEvent,
    onRemoved: FakeChromeEvent,
    onTabActivated: FakeChromeEvent,
    onTabUpdated: FakeChromeEvent,
  };

  constructor() {
    super([
      'getActiveUrl',
      'getFolders',
      'bookmarkCurrentTabInFolder',
      'openBookmark',
      'cutBookmark',
      'contextMenuOpenBookmarkInNewTab',
      'contextMenuOpenBookmarkInNewWindow',
      'contextMenuOpenBookmarkInIncognitoWindow',
      'contextMenuOpenBookmarkInNewTabGroup',
      'contextMenuAddToBookmarksBar',
      'contextMenuRemoveFromBookmarksBar',
      'contextMenuDelete',
      'copyBookmark',
      'createFolder',
      'editBookmarks',
      'deleteBookmarks',
      'pasteToBookmark',
      'renameBookmark',
      'setSortOrder',
      'setViewType',
      'showContextMenu',
      'showUi',
      'undo',
    ]);

    this.callbackRouter = {
      onChanged: new FakeChromeEvent(),
      onChildrenReordered: new FakeChromeEvent(),
      onCreated: new FakeChromeEvent(),
      onMoved: new FakeChromeEvent(),
      onRemoved: new FakeChromeEvent(),
      onTabActivated: new FakeChromeEvent(),
      onTabUpdated: new FakeChromeEvent(),
    };
  }

  getActiveUrl() {
    this.methodCalled('getActiveUrl');
    return Promise.resolve('http://www.test.com');
  }

  getFolders() {
    this.methodCalled('getFolders');
    return Promise.resolve(this.folders_);
  }

  bookmarkCurrentTabInFolder() {
    this.methodCalled('bookmarkCurrentTabInFolder');
  }

  openBookmark(
      id: string, depth: number, clickModifiers: ClickModifiers,
      source: ActionSource) {
    this.methodCalled('openBookmark', id, depth, clickModifiers, source);
  }

  setFolders(folders: chrome.bookmarks.BookmarkTreeNode[]) {
    this.folders_ = folders;
  }

  contextMenuOpenBookmarkInNewTab(ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuOpenBookmarkInNewTab', ids, source);
  }

  contextMenuOpenBookmarkInNewWindow(ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuOpenBookmarkInNewWindow', ids, source);
  }

  contextMenuOpenBookmarkInIncognitoWindow(
      ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuOpenBookmarkInIncognitoWindow', ids, source);
  }

  contextMenuOpenBookmarkInNewTabGroup(ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuOpenBookmarkInNewTabGroup', ids, source);
  }

  contextMenuAddToBookmarksBar(id: string, source: ActionSource) {
    this.methodCalled('contextMenuAddToBookmarksBar', id, source);
  }

  contextMenuRemoveFromBookmarksBar(id: string, source: ActionSource) {
    this.methodCalled('contextMenuRemoveFromBookmarksBar', id, source);
  }

  contextMenuDelete(ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuDelete', ids, source);
  }

  copyBookmark(id: string): Promise<void> {
    this.methodCalled('copyBookmark', id);
    return Promise.resolve();
  }

  createFolder(parentId: string, title: string):
      Promise<chrome.bookmarks.BookmarkTreeNode> {
    this.methodCalled('createFolder', parentId, title);
    return Promise.resolve({id: '0', title: 'foo'});
  }

  cutBookmark(id: string) {
    this.methodCalled('cutBookmark', id);
  }

  editBookmarks(
      ids: string[], newTitle: string|undefined, newUrl: string|undefined,
      newParentId: string|undefined) {
    this.methodCalled('editBookmarks', ids, newTitle, newUrl, newParentId);
  }

  deleteBookmarks(ids: string[]) {
    this.methodCalled('deleteBookmarks', ids);
    return Promise.resolve();
  }

  pasteToBookmark(parentId: string, destinationId?: string): Promise<void> {
    this.methodCalled('pasteToBookmark', parentId, destinationId);
    return Promise.resolve();
  }

  renameBookmark(id: string, title: string) {
    this.methodCalled('renameBookmark', id, title);
  }

  setSortOrder(sortOrder: SortOrder) {
    this.methodCalled('setSortOrder', sortOrder);
  }

  setViewType(viewType: ViewType) {
    this.methodCalled('setViewType', viewType);
  }

  showContextMenu(id: string, x: number, y: number, source: ActionSource) {
    this.methodCalled('showContextMenu', id, x, y, source);
  }

  showUi() {
    this.methodCalled('showUi');
  }

  undo() {
    this.methodCalled('undo');
  }
}
