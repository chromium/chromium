// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DownloadItem, PageCallbackRouter, PageRemote} from 'chrome://download-shelf.top-chrome/download_shelf.js';
import {TestBrowserProxy} from '../test_browser_proxy.js';

export class TestDownloadShelfApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'doClose',
      'doShowAll',
      'discardDownload',
      'keepDownload',
      'getDownloads',
      'getFileIcon',
      'openDownload',
      'showContextMenu',
    ]);

    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();

    /** @type {!PageRemote} */
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    /** @private {!Array<DownloadItem>} */
    this.downloadItems_ = [];
  }

  /** @override */
  doClose() {
    this.methodCalled('doClose');
  }

  /** @override */
  doShowAll() {
    this.methodCalled('doShowAll');
  }

  /** @override */
  discardDownload(downloadId) {
    this.methodCalled('discardDownload', [downloadId]);
  }

  /** @override */
  keepDownload(downloadId) {
    this.methodCalled('keepDownload', [downloadId]);
  }

  /** @override */
  getDownloads() {
    this.methodCalled('getDownloads');
    return Promise.resolve({downloadItems: this.downloadItems_});
  }

  /** @override */
  getFileIcon(downloadId) {
    this.methodCalled('getFileIcon', [downloadId]);
    return Promise.resolve('');
  }

  /** @override */
  showContextMenu(downloadId, clientX, clientY) {
    this.methodCalled('showContextMenu', [downloadId, clientX, clientY]);
  }

  /** @override */
  openDownload(downloadId) {
    this.methodCalled('openDownload', [downloadId]);
  }

  /** @override */
  getCallbackRouter() {
    return this.callbackRouter;
  }

  /** @return {!PageRemote} */
  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
  }

  /** @param {!Array<DownloadItem>} items */
  setDownloadItems(items) {
    this.downloadItems_ = items;
  }
}
