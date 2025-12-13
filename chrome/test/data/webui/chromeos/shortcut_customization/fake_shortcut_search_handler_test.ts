// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {fakeSearchResults} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutSearchHandler} from 'chrome://shortcut-customization/js/search/fake_shortcut_search_handler.js';
import type {MojoSearchResult} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertDeepEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('fakeShortcutSearchHandlerTest', function() {
  let handler: FakeShortcutSearchHandler|null = null;

  setup(() => {
    handler = new FakeShortcutSearchHandler();
  });

  teardown(() => {
    handler = null;
  });

  test('getSearchResultEmpty', () => {
    assertTrue(!!handler);
    const expectedList: MojoSearchResult[] = [];
    handler.setFakeSearchResult(expectedList);
    return handler.search('query1', 5).then((result) => {
      assertDeepEquals(expectedList, result.results);
    });
  });

  test('getSearchResultDefaultFake', () => {
    assertTrue(!!handler);
    handler.setFakeSearchResult(fakeSearchResults);
    return handler.search('query2', 5).then((result) => {
      assertDeepEquals(fakeSearchResults, result.results);
    });
  });
});
