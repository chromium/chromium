// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {fakeSearchResults} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutSearchHandler} from 'chrome://shortcut-customization/js/search/fake_shortcut_search_handler.js';
import {MojoSearchResult} from 'chrome://shortcut-customization/js/shortcut_types.js';

import {assertDeepEquals, assertTrue} from '../../chai_assert.js';

suite('fakeShortcutSearchHandlerTest', function() {
  let handler: FakeShortcutSearchHandler|null = null;

  setup(() => {
    handler = new FakeShortcutSearchHandler();
  });

  teardown(() => {
    handler = null;
  });

  // This test is based off of the stub search() function.
  // TODO(longbowei): Add parameters to the search().
  test('getSearchResultEmpty', () => {
    assertTrue(!!handler);
    const expectedList: MojoSearchResult[] = [];
    handler.setFakeSearchResult(expectedList);
    return handler.search().then((result) => {
      assertDeepEquals(expectedList, result.results);
    });
  });

  // This test is based off of the stub search() function.
  // TODO(longbowei): Add parameters to the search().
  test('getSearchResultDefaultFake', () => {
    assertTrue(!!handler);
    handler.setFakeSearchResult(fakeSearchResults);
    return handler.search().then((result) => {
      assertDeepEquals(fakeSearchResults, result.results);
    });
  });
});
