// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EmojiPrefixSearch} from 'chrome://emoji-picker/prefix_search.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

const mockCollection1 = [
  {base: {string: '😹', name: 'cat with tears of joy'}},
  {base: {string: '🤠', name: 'cowboy hat face'}},
  {base: {string: '🥲', name: 'smiling face with tear'}}
];

const mockCollection2 = [
  {base: {string: '😺', name: 'smiling cat'}},
  {base: {string: '😼', name: 'cat with wry smile'}},
  {base: {string: '🐛', name: 'caterpillar'}},
  {base: {string: '😇', name: 'smiling face with halo'}},
];

suite('PrefixSearchUnitTest', () => {
  let prefixSearch;
  setup(() => {
    prefixSearch = new EmojiPrefixSearch();
  });

  test('clear should erase all of the collection data.', () => {
    prefixSearch.setCollection(mockCollection1);
    prefixSearch.clear();

    const actualResults = prefixSearch.matchPrefixToEmojis('fa');

    assertEquals(0, actualResults.length);
  });

  test(
      'setCollection should replace the old collection with the new one.',
      () => {
        prefixSearch.setCollection(mockCollection1);
        prefixSearch.setCollection(mockCollection2);

        const actualResults = prefixSearch.matchPrefixToEmojis('smil');

        assertFalse(actualResults.includes('🥲'));
        assertTrue(actualResults.includes('😇'));
        assertTrue(actualResults.includes('😺'));
      });

  test('matchPrefixToEmojis should return the correct emojis.', () => {
    prefixSearch.setCollection(mockCollection1);

    const actualResults1 = prefixSearch.matchPrefixToEmojis('smil');
    const actualResults2 = prefixSearch.matchPrefixToEmojis('face');
    const actualResults3 = prefixSearch.matchPrefixToEmojis('tears');

    assertEquals(1, actualResults1.length);
    assertEquals('🥲', actualResults1[0]);
    assertEquals(2, actualResults2.length);
    assertTrue(actualResults2.includes('🤠'));
    assertTrue(actualResults2.includes('🥲'));
    assertEquals(1, actualResults3.length);
    assertTrue(actualResults3.includes('😹'));
    assertFalse(actualResults3.includes('🥲'));
  });
});