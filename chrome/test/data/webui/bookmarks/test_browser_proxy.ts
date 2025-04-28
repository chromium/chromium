// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome://bookmarks/bookmarks.js';
import {IncognitoAvailability} from 'chrome://bookmarks/bookmarks.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Test version of the bookmarks browser proxy.
 */
export class TestBookmarksBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  constructor() {
    super([
      'getIncognitoAvailability',
      'getCanEditBookmarks',
      'recordInHistogram',
      'getBatchUploadPromoInfo',
      'onBatchUploadPromoClicked',
      'onBatchUploadPromoDismissed',
    ]);
  }

  getIncognitoAvailability() {
    this.methodCalled('getIncognitoAvailability');
    return Promise.resolve(IncognitoAvailability.DISABLED);
  }

  getCanEditBookmarks() {
    this.methodCalled('getCanEditBookmarks');
    return Promise.resolve(false);
  }

  recordInHistogram(histogram: string, bucket: number, maxBucket: number) {
    this.methodCalled('recordInHistogram', [histogram, bucket, maxBucket]);
  }

  getBatchUploadPromoInfo() {
    this.methodCalled('getBatchUploadPromoInfo');
    return Promise.resolve({
      canShow: false,
      localBookmarksCount: 0,
      email: '',
    });
  }

  onBatchUploadPromoClicked() {
    this.methodCalled('onBatchUploadPromoClicked');
  }

  onBatchUploadPromoDismissed() {
    this.methodCalled('onBatchUploadPromoDismissed');
  }
}
