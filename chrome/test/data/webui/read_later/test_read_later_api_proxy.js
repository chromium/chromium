// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://read-later.top-chrome/read_later.mojom-lite.js';

import {ReadLaterApiProxy} from 'chrome://read-later.top-chrome/read_later_api_proxy.js';
import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {ReadLaterApiProxy} */
export class TestReadLaterApiProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getReadLaterEntries',
      'openSavedEntry',
      'updateReadStatus',
      'removeEntry',
      'showUI',
      'closeUI',
    ]);

    /** @type {!readLater.mojom.PageCallbackRouter} */
    this.callbackRouter = new readLater.mojom.PageCallbackRouter();

    /** @private {!readLater.mojom.ReadLaterEntriesByStatus} */
    this.entries_;
  }

  /** @override */
  getReadLaterEntries() {
    this.methodCalled('getReadLaterEntries');
    return Promise.resolve({entries: this.entries_});
  }

  /** @override */
  openSavedEntry(url) {
    this.methodCalled('openSavedEntry', url);
  }

  /** @override */
  updateReadStatus(url, read) {
    this.methodCalled('updateReadStatus', [url, read]);
  }

  /** @override */
  removeEntry(url) {
    this.methodCalled('removeEntry', url);
  }

  /** @override */
  showUI() {
    this.methodCalled('showUI');
  }

  /** @override */
  closeUI() {
    this.methodCalled('closeUI');
  }

  /** @override */
  getCallbackRouter() {
    return this.callbackRouter;
  }

  /** @param {!readLater.mojom.ReadLaterEntriesByStatus} entries */
  setEntries(entries) {
    this.entries_ = entries;
  }
}