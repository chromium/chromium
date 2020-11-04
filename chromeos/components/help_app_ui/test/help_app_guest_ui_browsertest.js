// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for chrome-untrusted://help-app. */

// Test that language is set correctly on the guest frame.
GUEST_TEST('GuestHasLang', () => {
  assertEquals(document.documentElement.lang, 'en-US');
});

/**
 * Waits for the app's initial index update to complete. This prevents it from
 * interfering with test code. After the update completes, there will be at
 * least one search result for the query "Chrome".
 *
 * Returns the app's client API delegate.
 */
async function waitForInitialIndexUpdate() {
  const delegate = /** @type {!helpApp.ClientApi} */ (
    document.querySelector('showoff-app')).getDelegate();

  for (let numTries = 0; numTries < 50; numTries++) {
    // 'Chrome' appears in the mock app's fake search results, and should appear
    // in the real app's search results.
    const response = await delegate.findInSearchIndex('Chrome');
    if (response && response.results && response.results.length > 0) break;
    await new Promise(resolve => {setTimeout(resolve, 50)});
  }
  return delegate;
}

// Test that search (add, find) works when called from the guest frame,
// and it works for searchable items with and without subheadings.
GUEST_TEST('GuestCanSearchWithHeadings', async () => {
  const delegate = await waitForInitialIndexUpdate();

  await delegate.addOrUpdateSearchIndex([{
    // Title match. No subheadings.
    id: 'test-id-1',
    title: 'Title with verycomplicatedsearchtoken',
    body: 'Body text',
    mainCategoryName: 'Help',
    locale: 'en-US',
  },{
    // Subheading match.
    id: 'test-id-2',
    title: 'Title 2',
    subheadings: [
      'Subheading 1',
      'verycomplicatedsearchtoken in subheading. Verycomplicatedsearchtoken',
      'Another subheading with verycomplicatedsearchtoken',
    ],
    body: 'Body text',
    mainCategoryName: 'Help',
    locale: 'en-US',
  },{
    // Should not appear in the results.
    id: 'test-id-3',
    title: 'Title of irrelevant article',
    body: 'Body text',
    mainCategoryName: 'Help',
    locale: 'en-US',
  }]);

  // Keep polling until the index finishes updating or too much time has passed.
  /** @type {?helpApp.FindResponse} */
  let response = null;
  for (let numTries = 0; numTries < 50; numTries++) {
    // This search query was chosen because it is unlikely to show any search
    // results for the real app's data.
    response = await delegate.findInSearchIndex('verycomplicatedsearchtoken');
    if (response && response.results && response.results.length > 0) break;
    await new Promise(resolve => {setTimeout(resolve, 50)});
  }

  assertDeepEquals(response.results, [
    // The first result only matches on the title.
    {
      id: 'test-id-1',
      titlePositions: [{start: 11, length: 26}],
      subheadingIndex: null,
      subheadingPositions: null,
      bodyPositions: [],
    },
    // The second result only matches on the second and third subheadings, and
    // it uses the subheading with the most matches in the snippet.
    {
      id: 'test-id-2',
      titlePositions: [],
      subheadingIndex: 1,
      subheadingPositions: [
        {start: 0, length: 26},
        {start: 42, length: 26},
      ],
      bodyPositions: [],
    },
  ]);
});

// Test that the guest frame can clear the search index.
GUEST_TEST('GuestCanClearSearchIndex', async () => {
  const delegate = await waitForInitialIndexUpdate();

  // Clear resolves after the index finishes clearing, so we don't need to try
  // finding multiple times.
  await delegate.clearSearchIndex();

  const res = await delegate.findInSearchIndex('Chrome');
  assertDeepEquals(res, {results: null});
});
