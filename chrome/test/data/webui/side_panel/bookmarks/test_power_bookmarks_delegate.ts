// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shared.mojom-webui.js';
import type {BookmarksTreeNode} from 'chrome://bookmarks-side-panel.top-chrome/bookmarks.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPowerBookmarksDelegate extends TestBrowserProxy {
  constructor() {
    super([
      'setCurrentUrl',
      'setImageUrl',
      'onBookmarksLoaded',
      'onBookmarkChanged',
      'onBookmarkAdded',
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

  setImageUrl(bookmark: BookmarksTreeNode, url: string) {
    this.methodCalled('setImageUrl', bookmark, url);
  }

  onBookmarksLoaded() {
    this.methodCalled('onBookmarksLoaded');
  }

  onBookmarkChanged(id: string) {
    this.methodCalled('onBookmarkChanged', id);
  }

  onBookmarkAdded(bookmark: BookmarksTreeNode, parent: BookmarksTreeNode) {
    this.methodCalled('onBookmarkAdded', bookmark, parent);
  }

  onBookmarkMoved(
      bookmark: BookmarksTreeNode, oldParent: BookmarksTreeNode,
      newParent: BookmarksTreeNode) {
    this.methodCalled('onBookmarkMoved', bookmark, oldParent, newParent);
  }

  onBookmarkRemoved(bookmark: BookmarksTreeNode) {
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

  getProductImageUrl(bookmark: BookmarksTreeNode) {
    this.methodCalled('getProductImageUrl', bookmark);
    return '';
  }
}
