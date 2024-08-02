// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';

import type {GetRelatedWebsiteSetsResponse, RelatedWebsiteSetsApiBrowserProxy, RelatedWebsiteSetsPageHandlerInterface} from 'chrome://privacy-sandbox-internals/related_website_sets/related_website_sets.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestRelatedWebsiteSetsPageHandler extends TestBrowserProxy
    implements RelatedWebsiteSetsPageHandlerInterface {
  relatedWebsiteSetsInfo: GetRelatedWebsiteSetsResponse = {};

  constructor() {
    super([
      'getRelatedWebsiteSets',
    ]);
  }

  getRelatedWebsiteSets() {
    this.methodCalled('getRelatedWebsiteSets');
    return this.relatedWebsiteSetsInfo === undefined ?
        Promise.reject('RELATED_WEBSITE_SETS_INVALID') :
        Promise.resolve({relatedWebsiteSetsInfo: this.relatedWebsiteSetsInfo});
  }
}

export class TestRelatedWebsiteSetsApiBrowserProxy extends TestBrowserProxy
    implements RelatedWebsiteSetsApiBrowserProxy {
  handler: TestRelatedWebsiteSetsPageHandler =
      new TestRelatedWebsiteSetsPageHandler();
}
