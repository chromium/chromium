// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IncognitoAvailability} from 'chrome://bookmarks/bookmarks.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

/**
 * Test version of the bookmarks browser proxy.
 */
export class TestBookmarksBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getIncognitoAvailability',
      'getCanEditBookmarks',
      'recordInHistogram',
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

  recordInHistogram(histogram, bucket, maxBucket) {
    this.methodCalled('recordInHistogram');
  }
}
