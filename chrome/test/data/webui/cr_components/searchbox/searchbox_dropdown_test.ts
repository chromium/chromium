// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {SearchboxDropdownElement} from 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createAutocompleteResult, createSearchMatch} from './searchbox_test_utils.js';

// TODO(crbug.com/455876602): Move dropdown-specific tests in searchbox_test.ts
//  into this file.
suite('SearchboxDropdown', () => {
  let dropdown: SearchboxDropdownElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dropdown = document.createElement('cr-searchbox-dropdown');
    document.body.appendChild(dropdown);
  });

  test('hides matches that have `isHidden` field set to true', async () => {
    // Arrange.
    const matches = [
      createSearchMatch({contents: 'bar', isHidden: true}),
      createSearchMatch({contents: 'foo'}),
    ];

    // Act.
    dropdown.result = createAutocompleteResult({matches});
    await microtasksFinished();

    // Assert.
    const matchEls = dropdown.shadowRoot.querySelectorAll('cr-searchbox-match');
    assertEquals(1, matchEls.length);
    const contentsEl = $$(matchEls[0]!, '#contents');
    assertTrue(!!contentsEl);
    assertEquals('foo', contentsEl.textContent.trim());
    // The visible element's matchIndex must retain its original index (1),
    // even though it is the first (and only) visible element.
    assertEquals(1, matchEls[0]!.matchIndex);
  });
});
