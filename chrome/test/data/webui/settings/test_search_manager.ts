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

  constructor() {
    super(['search']);
  }

  setMatchesFound(matchesFound: boolean) {
    this.matchesFound_ = matchesFound;
  }

  search(text: string, node: Element) {
    this.methodCalled('search', text);

    const request = new SearchRequest(text, node);

    const matchesFound = node.nodeType === Node.TEXT_NODE ?
        this.matchesFound_ :
        this.matchesFound_ && !node.hasAttribute('no-search');
    request.updateMatchCount(matchesFound ? 1 : 0);
    request.resolver.resolve(request);

    return request.resolver.promise;
  }
}
