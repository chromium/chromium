// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fuzzySearch, TabData} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertDeepEquals, assertEquals} from '../../chai_assert.js';

/**
 * Assert search results return in specific order.
 * @param {string} input
 * @param {!Array<!TabData>} items
 * @param {!Object} options
 * @param {!Array<number>} expectedIndices
 */
function assertSearchOrders(input, items, options, expectedIndices) {
  const results = fuzzySearch(input, items, options);
  assertEquals(results.length, expectedIndices.length);
  for (let i = 0; i < results.length; ++i) {
    const expectedItem = items[expectedIndices[i]];
    const actualItem = results[i];
    assertEquals(expectedItem.tab.title, actualItem.tab.title);
    assertEquals(expectedItem.hostname, actualItem.hostname);
  }
}

suite('FuzzySearchTest', () => {
  test('fuzzySearch', () => {
    const records = [
      {
        tab: {
          title: 'OpenGL',
        },
        hostname: 'www.opengl.org',
      },
      {
        tab: {
          title: 'Google',
        },
        hostname: 'www.google.com',
      },
    ];

    const matchedRecords = [
      {
        tab: {
          title: 'Google',
        },
        hostname: 'www.google.com',
        titleHighlightRanges: [{start: 0, length: 1}, {start: 3, length: 3}],
        hostnameHighlightRanges: [{start: 4, length: 1}, {start: 7, length: 3}]
      },
      {
        tab: {
          title: 'OpenGL',
        },
        hostname: 'www.opengl.org',
        titleHighlightRanges: [{start: 2, length: 1}, {start: 4, length: 2}],
        hostnameHighlightRanges: [
          {start: 6, length: 1}, {start: 8, length: 2}, {start: 13, length: 1}
        ],
      },
    ];

    const options = {
      includeScore: true,
      ignoreLocation: true,
      includeMatches: true,
      keys: [
        {
          name: 'tab.title',
          weight: 2,
        },
        {
          name: 'hostname',
          weight: 1,
        }
      ]
    };

    assertDeepEquals(matchedRecords, fuzzySearch('gle', records, options));
    assertDeepEquals(records, fuzzySearch('', records, options));
    assertDeepEquals([], fuzzySearch('z', records, options));
  });

  test('Test the exact match ranking order.', () => {
    // Set threshold to 0.0 to assert an exact match search.
    const options = {
      threshold: 0.0,
    };

    // Initial pre-search item list.
    const records = [
      {
        tab: {
          title: 'Code Search',
        },
        hostname: 'search.chromium.search',
      },
      {tab: {title: 'Marching band'}, hostname: 'en.marching.band.com'},
      {
        tab: {title: 'Chrome Desktop Architecture'},
        hostname: 'drive.google.com',
      },
      {
        tab: {title: 'Arch Linux'},
        hostname: 'www.archlinux.org',
      },
      {
        tab: {title: 'Arches National Park'},
        hostname: 'www.nps.gov',
      },
      {
        tab: {title: 'Search Engine Land - Search Engines'},
        hostname: 'searchengineland.com'
      },
    ];

    // Resuts for 'arch'.
    const archMatchedRecords = [
      {
        tab: {title: 'Arch Linux'},
        hostname: 'www.archlinux.org',
        titleHighlightRanges: [{start: 0, length: 4}],
        hostnameHighlightRanges: [{start: 4, length: 4}],
      },
      {
        tab: {title: 'Arches National Park'},
        hostname: 'www.nps.gov',
        titleHighlightRanges: [{start: 0, length: 4}],
      },
      {
        tab: {title: 'Chrome Desktop Architecture'},
        hostname: 'drive.google.com',
        titleHighlightRanges: [{start: 15, length: 4}],
      },
      {
        tab: {title: 'Code Search'},
        hostname: 'search.chromium.search',
        titleHighlightRanges: [{start: 7, length: 4}],
        hostnameHighlightRanges:
            [{start: 2, length: 4}, {start: 18, length: 4}],
      },
      {
        tab: {title: 'Search Engine Land - Search Engines'},
        hostname: 'searchengineland.com',
        titleHighlightRanges: [{start: 2, length: 4}, {start: 23, length: 4}],
        hostnameHighlightRanges: [{start: 2, length: 4}]
      },
      {
        tab: {title: 'Marching band'},
        hostname: 'en.marching.band.com',
        titleHighlightRanges: [{start: 1, length: 4}],
        hostnameHighlightRanges: [{start: 4, length: 4}]
      },
    ];

    // Results for 'search'.
    const searchMatchedRecords = [
      {
        tab: {title: 'Code Search'},
        hostname: 'search.chromium.search',
        titleHighlightRanges: [{start: 5, length: 6}],
        hostnameHighlightRanges:
            [{start: 0, length: 6}, {start: 16, length: 6}],
      },
      {
        tab: {title: 'Search Engine Land - Search Engines'},
        hostname: 'searchengineland.com',
        titleHighlightRanges: [{start: 0, length: 6}, {start: 21, length: 6}],
        hostnameHighlightRanges: [{start: 0, length: 6}]
      },
    ];

    // Empty search should return the full list.
    assertDeepEquals(records, fuzzySearch('', records, options));

    assertDeepEquals(archMatchedRecords, fuzzySearch('arch', records, options));
    assertDeepEquals(
        searchMatchedRecords, fuzzySearch('search', records, options));

    // No matches should return an empty list.
    assertDeepEquals([], fuzzySearch('archh', records, options));
  });

  test('Test exact search with escaped characters.', () => {
    // Set threshold to 0.0 to assert an exact match search.
    const options = {
      threshold: 0.0,
    };

    // Initial pre-search item list.
    const records = [{
      tab: {title: '\'beginning\\test\\end'},
      hostname: 'beginning\\test\"end',
    }];

    // Expected results for '\test'.
    const backslashMatchedRecords = [
      {
        tab: {title: '\'beginning\\test\\end'},
        hostname: 'beginning\\test\"end',
        titleHighlightRanges: [{start: 10, length: 5}],
        hostnameHighlightRanges: [{start: 9, length: 5}]
      },
    ];

    // Expected results for '"end'.
    const quoteMatchedRecords = [
      {
        tab: {title: '\'beginning\\test\\end'},
        hostname: 'beginning\\test\"end',
        hostnameHighlightRanges: [{start: 14, length: 4}],
      },
    ];

    assertDeepEquals(
        backslashMatchedRecords, fuzzySearch('\\test', records, options));
    assertDeepEquals(
        quoteMatchedRecords, fuzzySearch('\"end', records, options));
  });

  test('Test exact match result scoring accounts for match position.', () => {
    const options = {
      threshold: 0.0,
    };
    assertSearchOrders(
        'two',
        [
          {tab: {title: 'three one two'}}, {tab: {title: 'three two one'}},
          {tab: {title: 'one two three'}}
        ],
        options, [2, 1, 0]);
  });

  test(
      'Test exact match result scoring takes into account the number of matches per item.',
      () => {
        const options = {
          threshold: 0.0,
        };
        assertSearchOrders(
            'one',
            [
              {tab: {title: 'one two three'}}, {tab: {title: 'one one three'}},
              {tab: {title: 'one one one'}}
            ],
            options, [2, 1, 0]);
      });

  test(
      'Test exact match result scoring abides by the titleToHostnameWeightRatio.',
      () => {
        const options = {
          threshold: 0.0,
          keys: [
            {
              name: 'tab.title',
              weight: 2,
            },
            {
              name: 'hostname',
              weight: 1,
            }
          ]
        };
        assertSearchOrders(
            'search',
            [
              {tab: {title: 'New tab'}, hostname: 'chrome://tab-search'},
              {tab: {title: 'chrome://tab-search'}}, {
                tab: {title: 'chrome://tab-search'},
                hostname: 'chrome://tab-search'
              }
            ],
            options, [2, 1, 0]);
      });
});
