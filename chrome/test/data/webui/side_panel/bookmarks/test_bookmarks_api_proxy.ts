// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ActionSource, BookmarksPageRemote, BookmarksTreeNode, SortOrder, ViewType} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {BookmarksPageCallbackRouter} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import type {BookmarksApiProxy} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks_api_proxy.js';
import type {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBookmarksApiProxy extends TestBrowserProxy implements
    BookmarksApiProxy {
  private allBookmarks_: BookmarksTreeNode[] = [];
  pageCallbackRouter: BookmarksPageCallbackRouter;
  callbackRouterRemote: BookmarksPageRemote;

  callbackRouter: {
    onTabActivated: FakeChromeEvent,
    onTabUpdated: FakeChromeEvent,
  };

  constructor() {
    super([
      'getActiveUrl',
      'isActiveTabInSplit',
      'bookmarkCurrentTabInFolder',
      'openBookmark',
      'contextMenuOpenBookmarkInNewTab',
      'contextMenuOpenBookmarkInNewWindow',
      'contextMenuOpenBookmarkInIncognitoWindow',
      'contextMenuOpenBookmarkInNewTabGroup',
      'contextMenuOpenBookmarkInSplitView',
      'contextMenuEdit',
      'contextMenuMove',
      'contextMenuAddToBookmarksBar',
      'contextMenuRemoveFromBookmarksBar',
      'contextMenuDelete',
      'createFolder',
      'deleteBookmarks',
      'dropBookmarks',
      'editBookmarks',
      'renameBookmark',
      'setSortOrder',
      'setViewType',
      'showContextMenu',
      'showUi',
      'undo',
      'getAllBookmarks',
    ]);

    this.callbackRouter = {
      onTabActivated: new FakeChromeEvent(),
      onTabUpdated: new FakeChromeEvent(),
    };

    this.pageCallbackRouter = new BookmarksPageCallbackRouter();
    this.callbackRouterRemote =
        this.pageCallbackRouter.$.bindNewPipeAndPassRemote();
  }

  getActiveUrl() {
    this.methodCalled('getActiveUrl');
    return Promise.resolve('http://www.test.com');
  }

  isActiveTabInSplit() {
    this.methodCalled('isActiveTabInSplit');
    return Promise.resolve(false);
  }

  bookmarkCurrentTabInFolder() {
    this.methodCalled('bookmarkCurrentTabInFolder');
  }

  openBookmark(
      id: string, depth: number, clickModifiers: ClickModifiers,
      source: ActionSource) {
    this.methodCalled('openBookmark', id, depth, clickModifiers, source);
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

  contextMenuOpenBookmarkInSplitView(ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuOpenBookmarkInSplitView', ids, source);
  }

  contextMenuEdit(ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuEdit', ids, source);
  }

  contextMenuMove(ids: string[], source: ActionSource) {
    this.methodCalled('contextMenuMove', ids, source);
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
      Promise<{newFolderId: string}> {
    this.methodCalled('createFolder', parentId, title);
    return Promise.resolve({newFolderId: '0'});
  }

  cutBookmark(id: string) {
    this.methodCalled('cutBookmark', id);
  }

  deleteBookmarks(ids: string[]) {
    this.methodCalled('deleteBookmarks', ids);
    return Promise.resolve();
  }

  dropBookmarks(parentId: string) {
    this.methodCalled('dropBookmarks', parentId);
    return Promise.resolve();
  }

  editBookmarks(
      ids: string[], newTitle: string|undefined, newUrl: string|undefined,
      newParentId: string|undefined) {
    this.methodCalled('editBookmarks', ids, newTitle, newUrl, newParentId);
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

  setAllBookmarks(allBookmarks: BookmarksTreeNode[]) {
    this.allBookmarks_ = allBookmarks;
  }

  getAllBookmarks(): Promise<{nodes: BookmarksTreeNode[]}> {
    this.methodCalled('getAllBookmarks');
    return Promise.resolve({nodes: this.allBookmarks_});
  }
}
