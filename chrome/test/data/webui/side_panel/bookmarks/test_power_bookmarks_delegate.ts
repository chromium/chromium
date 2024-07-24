// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPowerBookmarksDelegate extends TestBrowserProxy {
  constructor() {
    super([
      'setCurrentUrl',
      'setImageUrl',
      'onBookmarksLoaded',
      'onBookmarkChanged',
      'onBookmarkCreated',
      'onBookmarkMoved',
      'onBookmarkRemoved',
      'getTrackedProductInfos',
      'getAvailableProductInfos',
      'getSelectedBookmarks',
      'getProductImageUrl',
    ]);
  }

  setCurrentUrl(url: string|undefined) {
    this.methodCalled('setCurrentUrl', url);
  }

  setImageUrl(bookmark: chrome.bookmarks.BookmarkTreeNode, url: string) {
    this.methodCalled('setImageUrl', bookmark, url);
  }

  onBookmarksLoaded() {
    this.methodCalled('onBookmarksLoaded');
  }

  onBookmarkChanged(id: string, changedInfo: chrome.bookmarks.ChangeInfo) {
    this.methodCalled('onBookmarkChanged', id, changedInfo);
  }

  onBookmarkCreated(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      parent: chrome.bookmarks.BookmarkTreeNode) {
    this.methodCalled('onBookmarkCreated', bookmark, parent);
  }

  onBookmarkMoved(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      oldParent: chrome.bookmarks.BookmarkTreeNode,
      newParent: chrome.bookmarks.BookmarkTreeNode) {
    this.methodCalled('onBookmarkMoved', bookmark, oldParent, newParent);
  }

  onBookmarkRemoved(bookmark: chrome.bookmarks.BookmarkTreeNode) {
    this.methodCalled('onBookmarkRemoved', bookmark);
  }

  getTrackedProductInfos() {
    this.methodCalled('getTrackedProductInfos');
    return {};
  }

  getAvailableProductInfos() {
    this.methodCalled('getAvailableProductInfos');
    return new Map<string, BookmarkProductInfo>();
  }

  getSelectedBookmarks() {
    this.methodCalled('getSelectedBookmarks');
    return {};
  }

  getProductImageUrl(bookmark: chrome.bookmarks.BookmarkTreeNode) {
    this.methodCalled('getProductImageUrl', bookmark);
    return '';
  }
}
