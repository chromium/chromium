// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy, SearchService} from 'chrome://downloads/downloads.js';
import {TestDownloadsProxy} from 'chrome://test/downloads/test_support.js';

/**
 * @param {!Array<string>} list
 * @return {string}
 */
function str(list) {
  return JSON.stringify(list);
}

test('splitTerms', function() {
  assertEquals(str([]), str(SearchService.splitTerms('')));
  assertEquals(str([]), str(SearchService.splitTerms('  ')));
  assertEquals(str(['a']), str(SearchService.splitTerms('a')));
  assertEquals(str(['a b']), str(SearchService.splitTerms('a b')));
  assertEquals(str(['a', 'b']), str(SearchService.splitTerms('a "b"')));
  assertEquals(str(['a', 'b', 'c']), str(SearchService.splitTerms('a "b" c')));
  assertEquals(
      str(['a', 'b b', 'c']), str(SearchService.splitTerms('a "b b" c')));
});

test('searchWithSimilarTerms', function() {
  BrowserProxy.instance_ = new TestDownloadsProxy();
  class TestSearchService extends SearchService {
    loadMore() { /* Remove call to backend. */
    }
  }

  const searchService = new TestSearchService();
  assertTrue(searchService.search('a'));
  assertFalse(searchService.search('a '));  // Same term + space should no-op.
});
