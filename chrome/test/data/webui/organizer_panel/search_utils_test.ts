// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {renderHighlightedText, SEARCH_PART_SEPARATOR, sliceRangesForParts} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import type {Range} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {html, render} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('SearchUtilsTest', () => {
  suite('renderHighlightedText', () => {
    let container: HTMLElement;

    setup(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      container = document.createElement('div');
      document.body.appendChild(container);
    });

    test('returns plain text when no ranges are provided', () => {
      const result = renderHighlightedText('hello world');
      assertEquals('hello world', result);
      render(html`${result}`, container);
      assertEquals('hello world', container.textContent);
      assertEquals(0, container.querySelectorAll('b').length);
    });

    test('returns plain text when empty ranges array is provided', () => {
      const result = renderHighlightedText('hello world', []);
      assertEquals('hello world', result);
      render(html`${result}`, container);
      assertEquals('hello world', container.textContent);
      assertEquals(0, container.querySelectorAll('b').length);
    });

    test('highlights matching substring for a single range', () => {
      const result =
          renderHighlightedText('hello world', [{start: 0, length: 5}]);
      render(html`${result}`, container);
      assertEquals('hello world', container.innerText);

      const hits = container.querySelectorAll<HTMLElement>('b');
      assertEquals(1, hits.length);
      assertEquals('hello', hits[0]!.textContent);
    });

    test('highlights multiple non-overlapping ranges', () => {
      const result = renderHighlightedText('hello beautiful world', [
        {start: 0, length: 5},
        {start: 16, length: 5},
      ]);
      render(html`${result}`, container);
      assertEquals('hello beautiful world', container.innerText);

      const hits = container.querySelectorAll<HTMLElement>('b');
      assertEquals(2, hits.length);
      assertEquals('hello', hits[0]!.textContent);
      assertEquals('world', hits[1]!.textContent);
    });

    test('highlights entire text when range spans full length', () => {
      const result = renderHighlightedText('match', [{start: 0, length: 5}]);
      render(html`${result}`, container);
      assertEquals('match', container.innerText);

      const hits = container.querySelectorAll<HTMLElement>('b');
      assertEquals(1, hits.length);
      assertEquals('match', hits[0]!.textContent);
    });

    test('re-rendering updates highlighted text cleanly', () => {
      render(
          html`${renderHighlightedText('first text', [{start: 0, length: 5}])}`,
          container);
      assertEquals('first text', container.innerText);
      assertEquals(1, container.querySelectorAll('b').length);

      render(
          html`${
              renderHighlightedText('second text', [{start: 7, length: 4}])}`,
          container);
      assertEquals('second text', container.innerText);

      const hits = container.querySelectorAll<HTMLElement>('b');
      assertEquals(1, hits.length);
      assertEquals('text', hits[0]!.textContent);
    });
  });

  suite('sliceRangesForParts', () => {
    test(
        'returns empty range arrays when ranges are undefined or empty', () => {
          assertDeepEquals([[], []], sliceRangesForParts(['Google', 'Search']));
          assertDeepEquals(
              [[], []], sliceRangesForParts(['Google', 'Search'], []));
        });

    test('returns empty array when parts list is empty', () => {
      assertDeepEquals([], sliceRangesForParts([], [{start: 0, length: 5}]));
    });

    test('handles single part with range', () => {
      const parts = ['Google Search'];
      const ranges: Range[] = [{start: 0, length: 6}];
      assertDeepEquals(
          [[{start: 0, length: 6}]], sliceRangesForParts(parts, ranges));
    });

    test('maps range to first part only', () => {
      // Joined string: "Google\nSearch"
      // "Google" is [0, 6), newline at 6, "Search" is [7, 13)
      const parts = ['Google', 'Search'];
      const ranges: Range[] = [{start: 0, length: 6}];
      assertDeepEquals(
          [[{start: 0, length: 6}], []], sliceRangesForParts(parts, ranges));
    });

    test('maps range to second part only with relative offset', () => {
      const parts = ['Google', 'Search'];
      const ranges: Range[] = [{start: 7, length: 6}];
      assertDeepEquals(
          [[], [{start: 0, length: 6}]], sliceRangesForParts(parts, ranges));
    });

    test('maps ranges covering multiple parts', () => {
      const parts = ['Google', 'Search'];
      const ranges: Range[] = [
        {start: 0, length: 3},
        {start: 7, length: 3},
      ];
      assertDeepEquals(
          [[{start: 0, length: 3}], [{start: 0, length: 3}]],
          sliceRangesForParts(parts, ranges));
    });

    test('splits a range that spans across the newline separator', () => {
      // Joined string: "Google\nSearch"
      // Range [4, 9) covers "le\nSe"
      // Part 0 "Google" [0, 6): overlap is [4, 6) -> start: 4, length: 2 ("le")
      // Newline at 6: skipped
      // Part 1 "Search" [7, 13): overlap is [7, 9) -> start: 0, length: 2
      // ("Se")
      const parts = ['Google', 'Search'];
      const ranges: Range[] = [{start: 4, length: 5}];
      assertDeepEquals(
          [[{start: 4, length: 2}], [{start: 0, length: 2}]],
          sliceRangesForParts(parts, ranges));
    });

    test(
        'handles range spanning entire joined string across multiple parts',
        () => {
          const parts = ['Google', 'Search'];
          const ranges: Range[] = [{start: 0, length: 13}];
          assertDeepEquals(
              [[{start: 0, length: 6}], [{start: 0, length: 6}]],
              sliceRangesForParts(parts, ranges));
        });

    test('handles three or more parts', () => {
      // Joined: "one\ntwo\nthree"
      // "one": [0, 3), newline: 3, "two": [4, 7), newline: 7, "three": [8, 13)
      const parts = ['one', 'two', 'three'];
      const ranges: Range[] = [{start: 4, length: 3}];
      assertDeepEquals(
          [[], [{start: 0, length: 3}], []],
          sliceRangesForParts(parts, ranges));
    });

    test('handles custom multi-character separator', () => {
      // Joined: "Google - Search"
      // "Google": [0, 6), separator " - " (length 3): [6, 9), "Search": [9, 15)
      const parts = ['Google', 'Search'];
      const ranges: Range[] = [{start: 9, length: 6}];
      assertDeepEquals(
          [[], [{start: 0, length: 6}]],
          sliceRangesForParts(parts, ranges, ' - '));
    });

    test('handles empty separator', () => {
      // Joined: "GoogleSearch"
      // "Google": [0, 6), "Search": [6, 12)
      const parts = ['Google', 'Search'];
      const ranges: Range[] = [{start: 6, length: 6}];
      assertDeepEquals(
          [[], [{start: 0, length: 6}]],
          sliceRangesForParts(parts, ranges, ''));
    });

    test('uses SEARCH_PART_SEPARATOR by default', () => {
      // Joined string: `Google${SEARCH_PART_SEPARATOR}Search`
      const parts = ['Google', 'Search'];
      const ranges: Range[] = [{start: 7, length: 6}];
      assertDeepEquals(
          [[], [{start: 0, length: 6}]],
          sliceRangesForParts(parts, ranges, SEARCH_PART_SEPARATOR));
      assertDeepEquals(
          sliceRangesForParts(parts, ranges, SEARCH_PART_SEPARATOR),
          sliceRangesForParts(parts, ranges));
    });
  });
});
