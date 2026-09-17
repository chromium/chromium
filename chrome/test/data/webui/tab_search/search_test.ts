// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {ItemData, Range, SearchOptions} from 'chrome://tab-search.top-chrome/tab_search.js';
import {getHostname, getTitle, search, TabData, TabItemType} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createTab} from './tab_search_test_data.js';

/**
 * Assert search results return in specific order.
 */
async function assertSearchOrders(
    input: string, items: TabData[], options: SearchOptions<TabData>,
    expectedIndices: number[]) {
  const results = await search(input, items, options);
  assertEquals(results.length, expectedIndices.length);
  for (let i = 0; i < results.length; ++i) {
    const expectedItem = items[expectedIndices[i]!]!;
    const actualItem = results[i]!;
    assertEquals(expectedItem.tab.title, actualItem.tab.title);
    assertEquals(expectedItem.hostname, actualItem.hostname);
  }
}

function assertResults(expectedRecords: ItemData[], actualRecords: ItemData[]) {
  assertEquals(expectedRecords.length, actualRecords.length);
  expectedRecords.forEach((expected, i) => {
    const actual = actualRecords[i]!;
    if (expected instanceof TabData) {
      assertTrue(actual instanceof TabData);
      assertEquals(expected.tab.title, actual.tab.title);
      assertEquals(expected.hostname, actual.hostname);
    }
    if (expected.tabGroup !== undefined) {
      assertEquals(expected.tabGroup.title, actual.tabGroup!.title);
    }
    assertDeepEquals(expected.highlightRanges, actual.highlightRanges);
  });
}

suite('FuzzySearchTest', () => {
  setup(() => {
    loadTimeData.overrideValues({
      cjkWordBoundaryEnabled: true,
    });
  });

  test('Test the exact match ranking order.', async () => {
    const options = {
      keys: [
        {
          name: 'tab.title',
          getter: getTitle,
          weight: 1,
        },
        {
          name: 'hostname',
          getter: getHostname,
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
        inActiveWindow: false,
        type: TabItemType.OPEN_TAB,
        a11yTypeText: '',
        tab: {title: 'Arch Linux'},
        hostname: 'www.archlinux.org',
        highlightRanges: {
          'tab.title': [{start: 0, length: 4}],
          hostname: [{start: 4, length: 4}],
        } as Record<string, Range[]>,
      },
      {
        inActiveWindow: false,
        type: TabItemType.OPEN_TAB,
        a11yTypeText: '',
        tab: {title: 'Arches National Park'},
        hostname: 'www.nps.gov',
        highlightRanges: {
          'tab.title': [{start: 0, length: 4}],
        } as Record<string, Range[]>,
      },
      {
        inActiveWindow: false,
        type: TabItemType.OPEN_TAB,
        a11yTypeText: '',
        tab: {title: 'Chrome Desktop Architecture'},
        hostname: 'drive.google.com',
        highlightRanges: {
          'tab.title': [{start: 15, length: 4}],
        } as Record<string, Range[]>,
      },
      {
        inActiveWindow: false,
        type: TabItemType.OPEN_TAB,
        a11yTypeText: '',
        tab: {title: 'Code Search'},
        hostname: 'search.chromium.search',
        highlightRanges: {
          'tab.title': [{start: 7, length: 4}],
          hostname: [{start: 2, length: 4}, {start: 18, length: 4}],
        } as Record<string, Range[]>,
      },
      {
        inActiveWindow: false,
        type: TabItemType.OPEN_TAB,
        a11yTypeText: '',
        tab: {title: 'Search Engine Land - Search Engines'},
        hostname: 'searchengineland.com',
        highlightRanges: {
          'tab.title': [{start: 2, length: 4}, {start: 23, length: 4}],
          hostname: [{start: 2, length: 4}],
        } as Record<string, Range[]>,
      },
      {
        inActiveWindow: false,
        type: TabItemType.OPEN_TAB,
        a11yTypeText: '',
        tab: {title: 'Marching band'},
        hostname: 'en.marching.band.com',
        highlightRanges: {
          'tab.title': [{start: 1, length: 4}],
          hostname: [{start: 4, length: 4}],
        } as Record<string, Range[]>,
      },
    ];

    // Results for 'search'.
    const searchMatchedRecords = [
      {
        inActiveWindow: false,
        type: TabItemType.OPEN_TAB,
        a11yTypeText: '',
        tab: {title: 'Code Search'},
        hostname: 'search.chromium.search',
        highlightRanges: {
          'tab.title': [{start: 5, length: 6}],
          hostname: [{start: 0, length: 6}, {start: 16, length: 6}],
        } as Record<string, Range[]>,
      },
      {
        inActiveWindow: false,
        type: TabItemType.OPEN_TAB,
        a11yTypeText: '',
        tab: {title: 'Search Engine Land - Search Engines'},
        hostname: 'searchengineland.com',
        highlightRanges: {
          'tab.title': [{start: 0, length: 6}, {start: 21, length: 6}],
          hostname: [{start: 0, length: 6}],
        } as Record<string, Range[]>,
      },
    ];

    // Empty search should return the full list.
    assertResults(records, await search('', records, options));
    assertResults(archMatchedRecords, await search('arch', records, options));
    assertResults(
        searchMatchedRecords, await search('search', records, options));

    // No matches should return an empty list.
    assertResults([], await search('archh', records, options));
  });

  test('Test exact search with escaped characters.', async () => {
    const options = {
      keys: [
        {
          name: 'tab.title',
          getter: getTitle,
          weight: 1,
        },
        {
          name: 'hostname',
          getter: getHostname,
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
        inActiveWindow: false,
        type: TabItemType.OPEN_TAB,
        a11yTypeText: '',
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
        inActiveWindow: false,
        type: TabItemType.OPEN_TAB,
        a11yTypeText: '',
        tab: {title: '\'beginning\\test\\end'},
        hostname: 'beginning\\test\"end',
        highlightRanges: {
          hostname: [{start: 14, length: 4}],
        },
      },
    ];

    assertResults(
        backslashMatchedRecords, await search('\\test', records, options));
    assertResults(quoteMatchedRecords, await search('\"end', records, options));
  });

  test('Test exact search with special quotation characters.', async () => {
    const options = {
      keys: [
        {
          name: 'tab.title',
          getter: getTitle,
          weight: 1,
        },
        {
          name: 'hostname',
          getter: getHostname,
          weight: 1,
        },
      ],
    };

    // Search for text with special characters in a record with special
    // characters.
    const recordsWithSpecialChar = [
      new TabData(
          createTab({title: '‘Chrome’ Browser'}), TabItemType.OPEN_TAB,
          '“google.com”'),
    ];

    const singleQuoteMatchedRecords = [{
      inActiveWindow: false,
      type: TabItemType.OPEN_TAB,
      a11yTypeText: '',
      tab: {title: '‘Chrome’ Browser'},
      hostname: '“google.com”',
      highlightRanges: {
        'tab.title': [{start: 0, length: 8}],
      },
    }];
    assertResults(
        singleQuoteMatchedRecords,
        await search('‘Chrome’', recordsWithSpecialChar, options));

    const doubleQuoteMatchedRecords = [{
      inActiveWindow: false,
      type: TabItemType.OPEN_TAB,
      a11yTypeText: '',
      tab: {title: '‘Chrome’ Browser'},
      hostname: '“google.com”',
      highlightRanges: {
        hostname: [{start: 0, length: 12}],
      },
    }];
    assertResults(
        doubleQuoteMatchedRecords,
        await search('“google.com”', recordsWithSpecialChar, options));

    // Search for text with regular characters in a record with special
    // characters.
    const singleQuoteMatchedRecordsRegular = [{
      inActiveWindow: false,
      type: TabItemType.OPEN_TAB,
      a11yTypeText: '',
      tab: {title: '‘Chrome’ Browser'},
      hostname: '“google.com”',
      highlightRanges: {
        'tab.title': [{start: 0, length: 8}],
      },
    }];
    assertResults(
        singleQuoteMatchedRecordsRegular,
        await search('\'Chrome\'', recordsWithSpecialChar, options));

    const doubleQuoteMatchedRecordsRegular = [{
      inActiveWindow: false,
      type: TabItemType.OPEN_TAB,
      a11yTypeText: '',
      tab: {title: '‘Chrome’ Browser'},
      hostname: '“google.com”',
      highlightRanges: {
        hostname: [{start: 0, length: 12}],
      },
    }];
    assertResults(
        doubleQuoteMatchedRecordsRegular,
        await search('"google.com"', recordsWithSpecialChar, options));

    // // Search for text with special characters in a record with regular
    // // characters.
    const recordsWithRegularChar = [
      new TabData(
          createTab({title: '\'Chrome\' Browser'}), TabItemType.OPEN_TAB,
          '"google.com"'),
    ];

    const singleQuoteMatchedRecordsSpecial = [{
      inActiveWindow: false,
      type: TabItemType.OPEN_TAB,
      a11yTypeText: '',
      tab: {title: '\'Chrome\' Browser'},
      hostname: '"google.com"',
      highlightRanges: {
        'tab.title': [{start: 0, length: 8}],
      },
    }];
    assertResults(
        singleQuoteMatchedRecordsSpecial,
        await search('‘Chrome’', recordsWithRegularChar, options));

    const doubleQuoteMatchedRecordsSpecial = [{
      inActiveWindow: false,
      type: TabItemType.OPEN_TAB,
      a11yTypeText: '',
      tab: {title: '\'Chrome\' Browser'},
      hostname: '"google.com"',
      highlightRanges: {
        hostname: [{start: 0, length: 12}],
      },
    }];
    assertResults(
        doubleQuoteMatchedRecordsSpecial,
        await search('“google.com”', recordsWithRegularChar, options));
  });

  test(
      'Test exact match result scoring accounts for match position.',
      async () => {
        const options = {
          keys: [
            {
              name: 'tab.title',
              getter: getTitle,
              weight: 1,
            },
            {
              name: 'hostname',
              getter: getHostname,
              weight: 1,
            },
          ],
        };

        await assertSearchOrders(
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
      async () => {
        const options = {
          keys: [
            {
              name: 'tab.title',
              getter: getTitle,
              weight: 1,
            },
            {
              name: 'hostname',
              getter: getHostname,
              weight: 1,
            },
          ],
        };

        await assertSearchOrders(
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

  test(
      'Test exact match result scoring abides by the key weights.',
      async () => {
        const options = {
          keys: [
            {
              name: 'tab.title',
              getter: getTitle,
              weight: 2,
            },
            {
              name: 'hostname',
              getter: getHostname,
              weight: 1,
            },
          ],
        };

        await assertSearchOrders(
            'search',
            [
              new TabData(
                  createTab({title: 'New tab'}), TabItemType.OPEN_TAB,
                  'chrome://tab-search'),
              new TabData(
                  createTab({title: 'chrome://tab-search'}),
                  TabItemType.OPEN_TAB, 'chrome://tab-search'),
              new TabData(
                  createTab({title: 'chrome://tab-search'}),
                  TabItemType.OPEN_TAB, 'chrome://tab-search'),
            ],
            options, [2, 1, 0]);
      });

  test('Test exact search with diacritics (accent-insensitive).', async () => {
    const options = {
      keys: [
        {
          name: 'tab.title',
          getter: getTitle,
          weight: 1,
        },
      ],
    };

    const records = [
      new TabData(
          createTab({title: 'Café Français'}), TabItemType.OPEN_TAB,
          'youtube.com'),
      new TabData(
          createTab({title: 'Google Search'}), TabItemType.OPEN_TAB,
          'google.com'),
    ];

    // Searching for base ASCII 'cafe' should match 'Café'
    const cafeMatchedRecords = [{
      inActiveWindow: false,
      type: TabItemType.OPEN_TAB,
      a11yTypeText: '',
      tab: {title: 'Café Français'},
      hostname: 'youtube.com',
      highlightRanges: {
        'tab.title': [{start: 0, length: 4}],
      },
    }];
    assertResults(cafeMatchedRecords, await search('cafe', records, options));

    // Searching for base ASCII 'francais' should match 'Français' (ignoring
    // case and cedilla 'ç')
    const francaisMatchedRecords = [{
      inActiveWindow: false,
      type: TabItemType.OPEN_TAB,
      a11yTypeText: '',
      tab: {title: 'Café Français'},
      hostname: 'youtube.com',
      highlightRanges: {
        'tab.title': [{start: 5, length: 8}],
      },
    }];
    assertResults(
        francaisMatchedRecords, await search('francais', records, options));

    // Searching with accents ('café') should also match
    assertResults(cafeMatchedRecords, await search('café', records, options));
  });

  test('Test exact search ranking with CJK word boundaries.', async () => {
    const options = {
      keys: [
        {
          name: 'tab.title',
          getter: getTitle,
          weight: 1,
        },
      ],
    };

    // Tab list ordered in reverse priority (lowest first) to verify sorting:
    // 1. "夜桜" -> Match inside a single CJK word segment (others)
    // 2. "美しい 桜" -> Match at a CJK word start after space (Word Start)
    // 3. "桜の季節" -> Match at the start of string (String Start)
    const records = [
      new TabData(
          createTab({title: '夜桜'}), TabItemType.OPEN_TAB, 'youtube.com'),
      new TabData(
          createTab({title: '美しい 桜'}), TabItemType.OPEN_TAB, 'youtube.com'),
      new TabData(
          createTab({title: '桜の季節'}), TabItemType.OPEN_TAB, 'youtube.com'),
    ];

    // Searching for "桜" should rank:
    // 1st: "桜の季節" (String Start)
    // 2nd: "美しい 桜" (Word Start)
    // 3rd: "美しい桜" (others)
    await assertSearchOrders('桜', records, options, [2, 1, 0]);
  });
});
