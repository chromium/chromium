// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SearchManager} from 'chrome://settings/settings.js';
import {SearchRequest} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Extending TestBrowserProxy even though SearchManager is not a browser proxy
 * itself. Essentially TestBrowserProxy can act as a "proxy" for any external
 * dependency, not just "browser proxies" (and maybe should be renamed to
 * TestProxy).
 */
export class TestSearchManager extends TestBrowserProxy implements
    SearchManager {
  private matchesFound_: boolean = true;
  private searchRequest_: SearchRequest|null = null;

  constructor() {
    super(['search']);
  }

  setMatchesFound(matchesFound: boolean) {
    this.matchesFound_ = matchesFound;
  }

  search(text: string, page: Element) {
    this.methodCalled('search', text);

    if (this.searchRequest_ == null || !this.searchRequest_.isSame(text)) {
      this.searchRequest_ = new SearchRequest(text, page);
      this.searchRequest_.updateMatchCount(this.matchesFound_ ? 1 : 0);
      this.searchRequest_.resolver.resolve(this.searchRequest_);
    }
    return this.searchRequest_.resolver.promise;
  }
}
