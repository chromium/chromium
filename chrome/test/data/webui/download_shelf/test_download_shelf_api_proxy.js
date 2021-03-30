// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageRemote} from 'chrome://download-shelf.top-chrome/download_shelf.mojom-webui.js';
import {DownloadShelfApiProxy, DownloadShelfApiProxyImpl} from 'chrome://download-shelf.top-chrome/download_shelf_api_proxy.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {DownloadShelfApiProxy} */
export class TestDownloadShelfApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getDownloads', 'getDownloadById', 'getFileIcon', 'onCreated',
      'onChanged', 'onErased', 'showContextMenu'
    ]);

    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();

    /** @type {!PageRemote} */
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    /** @private {!Array} */
    this.downloadItems_ = [];

    /** @private {Function} */
    this.onCreatedCallback_;

    /** @private {Function} */
    this.onChangedCallback_;

    /** @private {Function} */
    this.onErasedCallback_;
  }

  /** @override */
  getDownloads() {
    this.methodCalled('getDownloads');
    return Promise.resolve(this.downloadItems_);
  }

  /** @override */
  getDownloadById(downloadId) {
    this.methodCalled('getDownloadById', [downloadId]);
    const item = this.downloadItems_.find(item => item.id === downloadId);
    return Promise.resolve([item]);
  }

  /** @override */
  getFileIcon(downloadId) {
    this.methodCalled('getFileIcon', [downloadId]);
    return Promise.resolve('');
  }

  /** @override */
  onCreated(callback) {
    this.methodCalled('onCreated', [callback]);
    this.onCreatedCallback_ = callback;
  }

  /** @override */
  onChanged(callback) {
    this.methodCalled('onChanged', [callback]);
    this.onChangedCallback_ = callback;
  }

  /** @override */
  onErased(callback) {
    this.methodCalled('onErased', [callback]);
    this.onErasedCallback_ = callback;
  }

  /** @override */
  showContextMenu(downloadId, clientX, clientY) {
    this.methodCalled('showContextMenu', [downloadId, clientX, clientY]);
  }

  /** @override */
  getCallbackRouter() {
    return this.callbackRouter;
  }

  /** return {!PageRemote} */
  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
  }

  setDownloadItems(items) {
    this.downloadItems_ = items;
  }

  create(item) {
    if (this.onCreatedCallback_) {
      this.onCreatedCallback_(item);
    }
  }

  change(changes) {
    if (this.onChangedCallback_) {
      this.onChangedCallback_(changes);
    }
  }

  erase(id) {
    if (this.onErasedCallback_) {
      this.onErasedCallback_(id);
    }
  }
}
