// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fuzzySearch, FuzzySearchOptions, TabData, TabGroup, TabItemType} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {createTab, sampleToken} from './tab_search_test_data.js';

/**
 * Assert search results return in specific order.
 */
function assertSearchOrders(
    input: string, items: TabData[], options: FuzzySearchOptions<TabData>,
    expectedIndices: number[]) {
  const results = fuzzySearch(input, items, options);
  assertEquals(results.length, expectedIndices.length);
  for (let i = 0; i < results.length; ++i) {
    const expectedItem = items[expectedIndices[i]!]!;
    const actualItem = results[i]!;
    assertEquals(expectedItem.tab.title, actualItem.tab.title);
    assertEquals(expectedItem.hostname, actualItem.hostname);
  }
}

function assertResults(expectedRecords: any[], actualRecords: TabData[]) {
  assertEquals(expectedRecords.length, actualRecords.length);
  expectedRecords.forEach((expected, i) => {
    const actual = actualRecords[i]!;
    assertEquals(expected.tab.title, actual.tab.title);
    if (expected.tabGroup !== undefined) {
      assertEquals(expected.tabGroup.title, actual.tabGroup!.title);
    }
    assertEquals(expected.hostname, actual.hostname);
    assertDeepEquals(expected.highlightRanges, actual.highlightRanges);
  });
}

suite('FuzzySearchTest', () => {
  test('fuzzySearch', () => {
    const records: TabData[] = [
      new TabData(
          createTab({title: 'OpenGL'}), TabItemType.OPEN_TAB, 'www.opengl.org'),
      new TabData(
          createTab({title: 'Google'}), TabItemType.OPEN_TAB, 'www.google.com'),
    ];

    const matchedRecords = [
      {
        tab: {
          title: 'Google',
        },
        hostname: 'www.google.com',
        highlightRanges: {
          'tab.title': [{start: 0, length: 1}, {start: 3, length: 3}],
          hostname: [{start: 4, length: 1}, {start: 7, length: 3}],
        },
      },
      {
        tab: {
          title: 'OpenGL',
        },
        hostname: 'www.opengl.org',
        highlightRanges: {
          'tab.title': [{start: 2, length: 1}, {start: 4, length: 2}],
          hostname: [
            {start: 6, length: 1},
            {start: 8, length: 2},
            {start: 13, length: 1},
          ],
        },
      },
    ];

    const options = {
      useFuzzySearch: true,
      includeScore: true,
      includeMatches: true,
      keys: [
        {
          name: 'tab.title',
          weight: 2,
        },
        {
          name: 'hostname',
          weight: 1,
        },
      ],
    };

    assertResults(matchedRecords, fuzzySearch('gle', records, options));
    assertResults(records, fuzzySearch('', records, options));
    assertResults([], fuzzySearch('z', records, options));
  });

  test('fuzzy search title, hostname, and tab group title keys.', () => {
    const tabDataWithGroup = new TabData(
        createTab({title: 'Meet the cast'}), TabItemType.OPEN_TAB,
        'meet the cast');

    const tabGroup: TabGroup = {
      title: 'Glee TV show',
      color: 0,
      id: sampleToken(1n, 1n),
    };
    tabDataWithGroup.tabGroup = tabGroup;

    const records = [
      new TabData(
          createTab({title: 'OpenGL'}), TabItemType.OPEN_TAB, 'www.opengl.org'),
      new TabData(
          createTab({title: 'Google'}), TabItemType.OPEN_TAB, 'www.google.com'),
      tabDataWithGroup,
    ];

    const matchedRecords = [
      {
        tab: {
          title: 'Meet the cast',
        },
        hostname: 'meet the cast',
        tabGroup: {title: 'Glee TV show'},
        highlightRanges: {
          'tabGroup.title': [{start: 0, length: 3}],
        },
      },
      {
        tab: {
          title: 'Google',
        },
        hostname: 'www.google.com',
        highlightRanges: {
          'tab.title': [{start: 0, length: 1}, {start: 3, length: 3}],
          hostname: [{start: 4, length: 1}, {start: 7, length: 3}],
        },
      },
      {
        tab: {
          title: 'OpenGL',
        },
        hostname: 'www.opengl.org',
        highlightRanges: {
          'tab.title': [{start: 2, length: 1}, {start: 4, length: 2}],
          hostname: [
            {start: 6, length: 1},
            {start: 8, length: 2},
            {start: 13, length: 1},
          ],
        },
      },
    ];

    const options = {
      useFuzzySearch: true,
      includeScore: true,
      includeMatches: true,
      keys: [
        {
          name: 'tab.title',
          weight: 2,
        },
        {
          name: 'hostname',
          weight: 1,
        },
        {name: 'tabGroup.title', weight: 1.5},
      ],
    };

    assertResults(matchedRecords, fuzzySearch('gle', records, options));
    assertResults(records, fuzzySearch('', records, options));
    assertResults([], fuzzySearch('z', records, options));
  });

  test(
      'Test fuzzy search prioritize string start over word start over others',
      () => {
        const records = [
          new TabData(
              createTab({title: 'Asear'}), TabItemType.OPEN_TAB, 'asear'),
          new TabData(
              createTab({title: 'Tab Search'}), TabItemType.OPEN_TAB,
              'tab search'),
          new TabData(
              createTab({title: 'Search Engine'}), TabItemType.OPEN_TAB,
              'search engine'),
        ];

        const options = {
          useFuzzySearch: true,
          includeScore: true,
          includeMatches: true,
          keys: [
            {
              name: 'tab.title',
              weight: 1,
            },
          ],
        };
        assertSearchOrders('sear', records, options, [2, 1, 0]);
      });

  test('Test the exact match ranking order.', () => {
    const options = {
      useFuzzySearch: false,
      keys: [
        {
          name: 'tab.title',
          weight: 1,
        },
        {
          name: 'hostname',
          weight: 1,
        },
      ],
    };

    // Initial pre-search item list.
    const records = [
      new TabData(
          createTab({title: 'Code Search'}), TabItemType.OPEN_TAB,
          'search.chromium.search'),
      new TabData(
          createTab({title: 'Marching band'}), TabItemType.OPEN_TAB,
          'en.marching.band.com'),
      new TabData(
          createTab({title: 'Chrome Desktop Architecture'}),
          TabItemType.OPEN_TAB, 'drive.google.com'),
      new TabData(
          createTab({title: 'Arch Linux'}), TabItemType.OPEN_TAB,
          'www.archlinux.org'),
      new TabData(
          createTab({title: 'Arches National Park'}), TabItemType.OPEN_TAB,
          'www.nps.gov'),
      new TabData(
          createTab({title: 'Search Engine Land - Search Engines'}),
          TabItemType.OPEN_TAB, 'searchengineland.com'),
    ];

    // Results for 'arch'.
    const archMatchedRecords = [
      {
        tab: {title: 'Arch Linux'},
        hostname: 'www.archlinux.org',
        highlightRanges: {
          'tab.title': [{start: 0, length: 4}],
          hostname: [{start: 4, length: 4}],
        },
      },
      {
        tab: {title: 'Arches National Park'},
        hostname: 'www.nps.gov',
        highlightRanges: {
          'tab.title': [{start: 0, length: 4}],
        },
      },
      {
        tab: {title: 'Chrome Desktop Architecture'},
        hostname: 'drive.google.com',
        highlightRanges: {
          'tab.title': [{start: 15, length: 4}],
        },
      },
      {
        tab: {title: 'Code Search'},
        hostname: 'search.chromium.search',
        highlightRanges: {
          'tab.title': [{start: 7, length: 4}],
          hostname: [{start: 2, length: 4}, {start: 18, length: 4}],
        },
      },
      {
        tab: {title: 'Search Engine Land - Search Engines'},
        hostname: 'searchengineland.com',
        highlightRanges: {
          'tab.title': [{start: 2, length: 4}, {start: 23, length: 4}],
          hostname: [{start: 2, length: 4}],
        },
      },
      {
        tab: {title: 'Marching band'},
        hostname: 'en.marching.band.com',
        highlightRanges: {
          'tab.title': [{start: 1, length: 4}],
          hostname: [{start: 4, length: 4}],
        },
      },
    ];

    // Results for 'search'.
    const searchMatchedRecords = [
      {
        tab: {title: 'Code Search'},
        hostname: 'search.chromium.search',
        highlightRanges: {
          'tab.title': [{start: 5, length: 6}],
          hostname: [{start: 0, length: 6}, {start: 16, length: 6}],
        },
      },
      {
        tab: {title: 'Search Engine Land - Search Engines'},
        hostname: 'searchengineland.com',
        highlightRanges: {
          'tab.title': [{start: 0, length: 6}, {start: 21, length: 6}],
          hostname: [{start: 0, length: 6}],
        },
      },
    ];

    // Empty search should return the full list.
    assertResults(records, fuzzySearch('', records, options));
    assertResults(archMatchedRecords, fuzzySearch('arch', records, options));
    assertResults(
        searchMatchedRecords, fuzzySearch('search', records, options));

    // No matches should return an empty list.
    assertResults([], fuzzySearch('archh', records, options));
  });

  test('Test exact search with escaped characters.', () => {
    const options = {
      useFuzzySearch: false,
      keys: [
        {
          name: 'tab.title',
          weight: 1,
        },
        {
          name: 'hostname',
          weight: 1,
        },
      ],
    };

    // Initial pre-search item list.
    const records = [
      new TabData(
          createTab({title: '\'beginning\\test\\end'}), TabItemType.OPEN_TAB,
          'beginning\\test\"end'),
    ];

    // Expected results for '\test'.
    const backslashMatchedRecords = [
      {
        tab: {title: '\'beginning\\test\\end'},
        hostname: 'beginning\\test\"end',
        highlightRanges: {
          'tab.title': [{start: 10, length: 5}],
          hostname: [{start: 9, length: 5}],
        },
      },
    ];

    // Expected results for '"end'.
    const quoteMatchedRecords = [
      {
        tab: {title: '\'beginning\\test\\end'},
        hostname: 'beginning\\test\"end',
        highlightRanges: {
          hostname: [{start: 14, length: 4}],
        },
      },
    ];

    assertResults(
        backslashMatchedRecords, fuzzySearch('\\test', records, options));
    assertResults(quoteMatchedRecords, fuzzySearch('\"end', records, options));
  });

  test('Test exact match result scoring accounts for match position.', () => {
    const options = {
      useFuzzySearch: false,
      keys: [
        {
          name: 'tab.title',
          weight: 1,
        },
        {
          name: 'hostname',
          weight: 1,
        },
      ],
    };

    assertSearchOrders(
        'two',
        [
          new TabData(
              createTab({title: 'three one two'}), TabItemType.OPEN_TAB,
              'three one two'),
          new TabData(
              createTab({title: 'three two one'}), TabItemType.OPEN_TAB,
              'three two one'),
          new TabData(
              createTab({title: 'one two three'}), TabItemType.OPEN_TAB,
              'one two three'),
        ],
        options, [2, 1, 0]);
  });

  test(
      'Test exact match result scoring takes into account the number of matches per item.',
      () => {
        const options = {
          useFuzzySearch: false,
          keys: [
            {
              name: 'tab.title',
              weight: 1,
            },
            {
              name: 'hostname',
              weight: 1,
            },
          ],
        };

        assertSearchOrders(
            'one',
            [
              new TabData(
                  createTab({title: 'one two three'}), TabItemType.OPEN_TAB,
                  'one two three'),
              new TabData(
                  createTab({title: 'one one three'}), TabItemType.OPEN_TAB,
                  'one one three'),
              new TabData(
                  createTab({title: 'one one one'}), TabItemType.OPEN_TAB,
                  'one one one'),
            ],
            options, [2, 1, 0]);
      });

  test('Test exact match result scoring abides by the key weights.', () => {
    const options = {
      useFuzzySearch: false,
      keys: [
        {
          name: 'tab.title',
          weight: 2,
        },
        {
          name: 'hostname',
          weight: 1,
        },
      ],
    };

    assertSearchOrders(
        'search',
        [
          new TabData(
              createTab({title: 'New tab'}), TabItemType.OPEN_TAB,
              'chrome://tab-search'),
          new TabData(
              createTab({title: 'chrome://tab-search'}), TabItemType.OPEN_TAB,
              'chrome://tab-search'),
          new TabData(
              createTab({title: 'chrome://tab-search'}), TabItemType.OPEN_TAB,
              'chrome://tab-search'),
        ],
        options, [2, 1, 0]);
  });
});
