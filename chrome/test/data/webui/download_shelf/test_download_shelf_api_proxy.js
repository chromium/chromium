// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DownloadItem, PageCallbackRouter, PageRemote} from 'chrome://download-shelf.top-chrome/download_shelf.mojom-webui.js';
import {DownloadShelfApiProxy, DownloadShelfApiProxyImpl} from 'chrome://download-shelf.top-chrome/download_shelf_api_proxy.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {DownloadShelfApiProxy} */
export class TestDownloadShelfApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'doClose',
      'getDownloads',
      'getFileIcon',
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
