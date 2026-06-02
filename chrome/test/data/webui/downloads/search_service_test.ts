// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {browserProxyFactory, SearchService} from 'chrome://downloads/downloads.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {FakePageHandler} from './test_support.js';

function str(list: string[]): string {
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
  const handler = new FakePageHandler();
  const {instance} = browserProxyFactory.createForTest(handler);
  browserProxyFactory.setInstance(instance);
  class TestSearchService extends SearchService {
    override loadMore() { /* Remove call to backend. */
    }
  }

  const searchService = new TestSearchService();
  assertTrue(searchService.search('a'));
  assertFalse(searchService.search('a '));  // Same term + space should no-op.
});
