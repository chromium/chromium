// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for chrome-untrusted://help-app. */

// Test that language is set correctly on the guest frame.
GUEST_TEST('GuestHasLang', () => {
  assertEquals(document.documentElement.lang, 'en-US');
});

// Test that search works when called from the guest frame, and it works for
// searchable items with and without subheadings.
GUEST_TEST('GuestCanSearchWithHeadings', async () => {
  const delegate = /** @type {!helpApp.ClientApi} */ (
      document.querySelector('showoff-app')).getDelegate();
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
  for (let numTries = 0; numTries < 100; numTries++) {
    response = await delegate.findInSearchIndex('verycomplicatedsearchtoken');
    if (response && response.results) break;
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
